#pragma once

#include <cstdint>

#include "can_core/can_frame.hpp"

namespace can_core {

enum class CanBitrate : std::uint32_t {
    Kbit100 = 100'000U,
    Kbit500 = 500'000U,
};

[[nodiscard]] constexpr std::uint32_t bitrateValue(
    const CanBitrate bitrate) noexcept {
    return static_cast<std::uint32_t>(bitrate);
}

[[nodiscard]] constexpr bool isSupportedBenchBitrate(
    const CanBitrate bitrate) noexcept {
    return bitrate == CanBitrate::Kbit100 || bitrate == CanBitrate::Kbit500;
}

struct CanReceiverConfig final {
    CanBitrate bitrate{CanBitrate::Kbit100};
    std::uint8_t channel{0U};

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return isSupportedBenchBitrate(bitrate);
    }
};

struct CanReceiveStatistics final {
    std::uint64_t framesReceived{0U};
    std::uint64_t framesQueued{0U};
    std::uint64_t framesDequeued{0U};
    std::uint64_t softwareQueueOverruns{0U};
    std::uint64_t driverQueueMissed{0U};
    std::uint64_t driverFifoOverruns{0U};
    std::uint64_t busErrors{0U};
    std::uint64_t errorWarningTransitions{0U};
    std::uint64_t errorPassiveTransitions{0U};
    std::uint64_t busOffTransitions{0U};
    std::uint64_t peripheralResets{0U};
    std::uint64_t lastSequence{0U};
    std::uint32_t receiveErrorCounter{0U};
    std::uint32_t transmitErrorCounter{0U};
    std::uint16_t queuedFrames{0U};
    std::uint16_t queueHighWatermark{0U};
};

enum class CanReceiverState : std::uint8_t {
    Stopped,
    RunningListenOnly,
    Fault,
};

enum class CanReceiverStartStatus : std::uint8_t {
    Started,
    AlreadyRunning,
    InvalidConfiguration,
    SafetyRejected,
    DriverInstallFailed,
    DriverStartFailed,
    TaskStartFailed,
};

class CanFrameReceiver {
public:
    virtual ~CanFrameReceiver() = default;

    [[nodiscard]] virtual CanReceiverStartStatus start() noexcept = 0;
    virtual void stop() noexcept = 0;
    [[nodiscard]] virtual bool tryReceive(CanFrame& frame) noexcept = 0;
    [[nodiscard]] virtual CanReceiveStatistics statistics() const noexcept = 0;
    [[nodiscard]] virtual CanReceiverState state() const noexcept = 0;
};

}  // namespace can_core
