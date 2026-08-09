#if defined(ESP_PLATFORM)

#include "bmw_remote/infrastructure/esp_idf_twai_receiver.hpp"

#include <algorithm>

#include "driver/twai.h"
#include "esp_timer.h"

#ifndef CONFIG_TWAI_ERRATA_FIX_LISTEN_ONLY_DOM
#error "Phase 3 requires CONFIG_TWAI_ERRATA_FIX_LISTEN_ONLY_DOM=y"
#endif

namespace bmw::remote::infrastructure {
namespace {

constexpr std::uint32_t ReceiveAlerts =
    TWAI_ALERT_RX_QUEUE_FULL |
    TWAI_ALERT_RX_FIFO_OVERRUN |
    TWAI_ALERT_BUS_ERROR |
    TWAI_ALERT_ABOVE_ERR_WARN |
    TWAI_ALERT_BELOW_ERR_WARN |
    TWAI_ALERT_ERR_PASS |
    TWAI_ALERT_ERR_ACTIVE |
    TWAI_ALERT_BUS_OFF |
    TWAI_ALERT_BUS_RECOVERED |
    TWAI_ALERT_PERIPH_RESET;

[[nodiscard]] constexpr std::uint16_t statusValue(
    const can_core::CanFrameStatus status) noexcept {
    return static_cast<std::uint16_t>(status);
}

void addStatus(
    std::atomic<std::uint16_t>& value,
    const can_core::CanFrameStatus status) noexcept {
    value.fetch_or(statusValue(status), std::memory_order_relaxed);
}

void removeStatus(
    std::atomic<std::uint16_t>& value,
    const can_core::CanFrameStatus status) noexcept {
    value.fetch_and(
        static_cast<std::uint16_t>(~statusValue(status)),
        std::memory_order_relaxed);
}

[[nodiscard]] twai_timing_config_t timingFor(
    const can_core::CanBitrate bitrate) noexcept {
    if (bitrate == can_core::CanBitrate::Kbit500) {
        return TWAI_TIMING_CONFIG_500KBITS();
    }
    return TWAI_TIMING_CONFIG_100KBITS();
}

}  // namespace

EspIdfTwaiReceiver::~EspIdfTwaiReceiver() {
    stop();
}

can_core::CanReceiverStartStatus EspIdfTwaiReceiver::start() noexcept {
    if (state_.load(std::memory_order_acquire) ==
        can_core::CanReceiverState::RunningListenOnly) {
        return can_core::CanReceiverStartStatus::AlreadyRunning;
    }
    if (!config_.isValid()) {
        state_.store(can_core::CanReceiverState::Fault, std::memory_order_release);
        return can_core::CanReceiverStartStatus::InvalidConfiguration;
    }
    if (!prepareTwaiListenOnlySafety(safety_)) {
        state_.store(can_core::CanReceiverState::Fault, std::memory_order_release);
        return can_core::CanReceiverStartStatus::SafetyRejected;
    }

    twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(
        static_cast<gpio_num_t>(config_.transmitPin),
        static_cast<gpio_num_t>(config_.receivePin),
        TWAI_MODE_LISTEN_ONLY);
    general.tx_queue_len = 0U;
    general.rx_queue_len = config_.driverReceiveQueueDepth;
    general.alerts_enabled = ReceiveAlerts;
    const twai_timing_config_t timing = timingFor(config_.receiver.bitrate);
    const twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&general, &timing, &filter) != ESP_OK) {
        state_.store(can_core::CanReceiverState::Fault, std::memory_order_release);
        return can_core::CanReceiverStartStatus::DriverInstallFailed;
    }
    driverInstalled_ = true;
    if (twai_start() != ESP_OK) {
        static_cast<void>(twai_driver_uninstall());
        driverInstalled_ = false;
        state_.store(can_core::CanReceiverState::Fault, std::memory_order_release);
        return can_core::CanReceiverStartStatus::DriverStartFailed;
    }

    queue_.clear();
    resetStatistics();
    nextSequence_ = 1U;
    taskExited_.store(false, std::memory_order_release);
    running_.store(true, std::memory_order_release);
    taskHandle_ = xTaskCreateStaticPinnedToCore(
        receiveTaskEntry,
        "twai_rx_only",
        TaskStackDepth,
        this,
        static_cast<UBaseType_t>(config_.taskPriority),
        taskStack_,
        &taskControlBlock_,
        config_.taskCore);
    if (taskHandle_ == nullptr) {
        running_.store(false, std::memory_order_release);
        taskExited_.store(true, std::memory_order_release);
        static_cast<void>(twai_stop());
        static_cast<void>(twai_driver_uninstall());
        driverInstalled_ = false;
        state_.store(can_core::CanReceiverState::Fault, std::memory_order_release);
        return can_core::CanReceiverStartStatus::TaskStartFailed;
    }

    state_.store(
        can_core::CanReceiverState::RunningListenOnly,
        std::memory_order_release);
    return can_core::CanReceiverStartStatus::Started;
}

void EspIdfTwaiReceiver::stop() noexcept {
    if (!driverInstalled_) {
        state_.store(can_core::CanReceiverState::Stopped, std::memory_order_release);
        return;
    }

    running_.store(false, std::memory_order_release);
    static_cast<void>(twai_stop());
    for (std::uint32_t wait = 0U;
         wait < 20U && !taskExited_.load(std::memory_order_acquire);
         ++wait) {
        vTaskDelay(pdMS_TO_TICKS(5U));
    }

    if (!taskExited_.load(std::memory_order_acquire)) {
        state_.store(can_core::CanReceiverState::Fault, std::memory_order_release);
        return;
    }

    static_cast<void>(twai_driver_uninstall());
    driverInstalled_ = false;
    taskHandle_ = nullptr;
    state_.store(can_core::CanReceiverState::Stopped, std::memory_order_release);
}

bool EspIdfTwaiReceiver::tryReceive(can_core::CanFrame& frame) noexcept {
    if (!queue_.tryPop(frame)) {
        return false;
    }
    statistics_.framesDequeued.fetch_add(1U, std::memory_order_relaxed);
    return true;
}

can_core::CanReceiveStatistics EspIdfTwaiReceiver::statistics() const noexcept {
    can_core::CanReceiveStatistics result{};
    result.framesReceived = statistics_.framesReceived.load();
    result.framesQueued = statistics_.framesQueued.load();
    result.framesDequeued = statistics_.framesDequeued.load();
    result.softwareQueueOverruns = statistics_.softwareQueueOverruns.load();
    result.driverQueueMissed = statistics_.driverQueueMissed.load();
    result.driverFifoOverruns = statistics_.driverFifoOverruns.load();
    result.busErrors = statistics_.busErrors.load();
    result.errorWarningTransitions = statistics_.errorWarningTransitions.load();
    result.errorPassiveTransitions = statistics_.errorPassiveTransitions.load();
    result.busOffTransitions = statistics_.busOffTransitions.load();
    result.peripheralResets = statistics_.peripheralResets.load();
    result.lastSequence = statistics_.lastSequence.load();
    result.receiveErrorCounter = statistics_.receiveErrorCounter.load();
    result.transmitErrorCounter = statistics_.transmitErrorCounter.load();
    result.queuedFrames = static_cast<std::uint16_t>(queue_.size());
    result.queueHighWatermark = statistics_.queueHighWatermark.load();
    return result;
}

can_core::CanReceiverState EspIdfTwaiReceiver::state() const noexcept {
    return state_.load(std::memory_order_acquire);
}

void EspIdfTwaiReceiver::receiveTaskEntry(void* const context) noexcept {
    static_cast<EspIdfTwaiReceiver*>(context)->receiveTask();
}

void EspIdfTwaiReceiver::receiveTask() noexcept {
    while (running_.load(std::memory_order_acquire)) {
        twai_message_t message{};
        const esp_err_t received = twai_receive(
            &message,
            pdMS_TO_TICKS(config_.receiveTimeoutMs));
        if (received == ESP_OK) {
            enqueueReceivedFrame(
                message.identifier,
                message.extd != 0U,
                message.rtr != 0U,
                message.dlc_non_comp != 0U,
                message.data_length_code,
                message.data);
        }
        updateAlerts();
        updateDriverStatus();
    }

    taskExited_.store(true, std::memory_order_release);
    vTaskDelete(nullptr);
}

void EspIdfTwaiReceiver::resetStatistics() noexcept {
    statistics_.framesReceived.store(0U);
    statistics_.framesQueued.store(0U);
    statistics_.framesDequeued.store(0U);
    statistics_.softwareQueueOverruns.store(0U);
    statistics_.driverQueueMissed.store(0U);
    statistics_.driverFifoOverruns.store(0U);
    statistics_.busErrors.store(0U);
    statistics_.errorWarningTransitions.store(0U);
    statistics_.errorPassiveTransitions.store(0U);
    statistics_.busOffTransitions.store(0U);
    statistics_.peripheralResets.store(0U);
    statistics_.lastSequence.store(0U);
    statistics_.receiveErrorCounter.store(0U);
    statistics_.transmitErrorCounter.store(0U);
    statistics_.queueHighWatermark.store(0U);
    currentFrameStatus_.store(0U);
}

void EspIdfTwaiReceiver::updateAlerts() noexcept {
    std::uint32_t alerts = 0U;
    if (twai_read_alerts(&alerts, 0U) != ESP_OK) {
        return;
    }
    if ((alerts & TWAI_ALERT_ABOVE_ERR_WARN) != 0U) {
        statistics_.errorWarningTransitions.fetch_add(1U);
        addStatus(currentFrameStatus_, can_core::CanFrameStatus::ErrorWarning);
    }
    if ((alerts & TWAI_ALERT_BELOW_ERR_WARN) != 0U) {
        removeStatus(currentFrameStatus_, can_core::CanFrameStatus::ErrorWarning);
    }
    if ((alerts & TWAI_ALERT_ERR_PASS) != 0U) {
        statistics_.errorPassiveTransitions.fetch_add(1U);
        addStatus(currentFrameStatus_, can_core::CanFrameStatus::ErrorPassive);
    }
    if ((alerts & TWAI_ALERT_ERR_ACTIVE) != 0U) {
        removeStatus(currentFrameStatus_, can_core::CanFrameStatus::ErrorPassive);
    }
    if ((alerts & TWAI_ALERT_BUS_OFF) != 0U) {
        statistics_.busOffTransitions.fetch_add(1U);
        addStatus(currentFrameStatus_, can_core::CanFrameStatus::BusOff);
    }
    if ((alerts & TWAI_ALERT_BUS_RECOVERED) != 0U) {
        removeStatus(currentFrameStatus_, can_core::CanFrameStatus::BusOff);
    }
    if ((alerts & TWAI_ALERT_PERIPH_RESET) != 0U) {
        statistics_.peripheralResets.fetch_add(1U);
        addStatus(currentFrameStatus_, can_core::CanFrameStatus::PeripheralReset);
    }
}

void EspIdfTwaiReceiver::updateDriverStatus() noexcept {
    twai_status_info_t status{};
    if (twai_get_status_info(&status) != ESP_OK) {
        return;
    }
    statistics_.driverQueueMissed.store(status.rx_missed_count);
    statistics_.driverFifoOverruns.store(status.rx_overrun_count);
    statistics_.busErrors.store(status.bus_error_count);
    statistics_.receiveErrorCounter.store(status.rx_error_counter);
    statistics_.transmitErrorCounter.store(status.tx_error_counter);
}

void EspIdfTwaiReceiver::enqueueReceivedFrame(
    const std::uint32_t identifier,
    const bool extended,
    const bool remote,
    const bool dlcNonCompliant,
    const std::uint8_t dataLength,
    const std::uint8_t* const data) noexcept {
    can_core::CanFrame frame{};
    frame.timestampUs = static_cast<std::uint64_t>(esp_timer_get_time());
    frame.sequence = nextSequence_++;
    frame.bitrate = can_core::bitrateValue(config_.receiver.bitrate);
    frame.identifier = identifier;
    frame.channel = config_.receiver.channel;
    frame.extended = extended;
    frame.dataLength = std::min(dataLength, can_core::CanFrame::MaximumDataLength);
    frame.status = static_cast<can_core::CanFrameStatus>(
        currentFrameStatus_.load(std::memory_order_relaxed));
    if (remote) {
        frame.status = frame.status | can_core::CanFrameStatus::RemoteFrame;
    }
    if (dlcNonCompliant || dataLength > can_core::CanFrame::MaximumDataLength) {
        frame.status = frame.status | can_core::CanFrameStatus::DlcNonCompliant;
    }
    if (!remote) {
        for (std::size_t index = 0U; index < frame.dataLength; ++index) {
            frame.data[index] = data[index];
        }
    }

    statistics_.framesReceived.fetch_add(1U);
    statistics_.lastSequence.store(frame.sequence);
    if (!queue_.tryPush(frame)) {
        statistics_.softwareQueueOverruns.fetch_add(1U);
        return;
    }
    statistics_.framesQueued.fetch_add(1U);

    const std::uint16_t depth = static_cast<std::uint16_t>(queue_.size());
    std::uint16_t high = statistics_.queueHighWatermark.load();
    while (depth > high &&
           !statistics_.queueHighWatermark.compare_exchange_weak(high, depth)) {
    }
}

}  // namespace bmw::remote::infrastructure

#endif
