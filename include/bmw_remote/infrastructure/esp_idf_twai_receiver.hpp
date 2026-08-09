#pragma once

#if defined(ESP_PLATFORM)

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "bmw_remote/infrastructure/twai_listen_only.hpp"
#include "can_core/bounded_spsc_queue.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace bmw::remote::infrastructure {

class EspIdfTwaiReceiver final : public can_core::CanFrameReceiver {
public:
    static constexpr std::size_t FrameQueueCapacity = 128U;
    static constexpr std::size_t TaskStackBytes = 4'096U;
    static constexpr std::size_t TaskStackDepth =
        TaskStackBytes / sizeof(StackType_t);

    EspIdfTwaiReceiver(
        TwaiSafetyHal& safety,
        TwaiListenOnlyConfig config) noexcept
        : safety_(safety), config_(config) {}

    ~EspIdfTwaiReceiver() override;

    EspIdfTwaiReceiver(const EspIdfTwaiReceiver&) = delete;
    EspIdfTwaiReceiver& operator=(const EspIdfTwaiReceiver&) = delete;

    [[nodiscard]] can_core::CanReceiverStartStatus start() noexcept override;
    void stop() noexcept override;
    [[nodiscard]] bool tryReceive(can_core::CanFrame& frame) noexcept override;
    [[nodiscard]] can_core::CanReceiveStatistics statistics() const noexcept override;
    [[nodiscard]] can_core::CanReceiverState state() const noexcept override;

private:
    struct AtomicStatistics final {
        std::atomic<std::uint64_t> framesReceived{0U};
        std::atomic<std::uint64_t> framesQueued{0U};
        std::atomic<std::uint64_t> framesDequeued{0U};
        std::atomic<std::uint64_t> softwareQueueOverruns{0U};
        std::atomic<std::uint64_t> driverQueueMissed{0U};
        std::atomic<std::uint64_t> driverFifoOverruns{0U};
        std::atomic<std::uint64_t> busErrors{0U};
        std::atomic<std::uint64_t> errorWarningTransitions{0U};
        std::atomic<std::uint64_t> errorPassiveTransitions{0U};
        std::atomic<std::uint64_t> busOffTransitions{0U};
        std::atomic<std::uint64_t> peripheralResets{0U};
        std::atomic<std::uint64_t> lastSequence{0U};
        std::atomic<std::uint32_t> receiveErrorCounter{0U};
        std::atomic<std::uint32_t> transmitErrorCounter{0U};
        std::atomic<std::uint16_t> queueHighWatermark{0U};
    };

    static void receiveTaskEntry(void* context) noexcept;
    void receiveTask() noexcept;
    void resetStatistics() noexcept;
    void updateAlerts() noexcept;
    void updateDriverStatus() noexcept;
    void enqueueReceivedFrame(
        std::uint32_t identifier,
        bool extended,
        bool remote,
        bool dlcNonCompliant,
        std::uint8_t dataLength,
        const std::uint8_t* data) noexcept;

    TwaiSafetyHal& safety_;
    TwaiListenOnlyConfig config_{};
    can_core::BoundedSpscQueue<
        can_core::CanFrame,
        FrameQueueCapacity> queue_{};
    AtomicStatistics statistics_{};
    std::atomic<can_core::CanReceiverState> state_{
        can_core::CanReceiverState::Stopped};
    std::atomic<bool> running_{false};
    std::atomic<bool> taskExited_{true};
    std::atomic<std::uint16_t> currentFrameStatus_{0U};
    std::uint64_t nextSequence_{1U};
    bool driverInstalled_{false};
    TaskHandle_t taskHandle_{nullptr};
    StaticTask_t taskControlBlock_{};
    StackType_t taskStack_[TaskStackDepth]{};
};

}  // namespace bmw::remote::infrastructure

#endif
