#pragma once

#include <cstdint>

#include "bmw_remote/infrastructure/hardware_abstraction.hpp"
#include "can_core/can_receiver.hpp"

namespace bmw::remote::infrastructure {

struct TwaiSafetyPinConfig final {
    int silentPin{-1};
    GpioLevel silentLevel{GpioLevel::High};
    int transmitInhibitPin{-1};
    GpioLevel transmitInhibitLevel{GpioLevel::High};

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return silentPin >= 0 && silentPin <= 48 &&
               transmitInhibitPin >= 0 && transmitInhibitPin <= 48 &&
               silentPin != transmitInhibitPin;
    }
};

class GpioTwaiSafetyHal final : public TwaiSafetyHal {
public:
    GpioTwaiSafetyHal(
        GpioHal& gpio,
        TwaiSafetyPinConfig config) noexcept
        : gpio_(gpio), config_(config) {}

    bool holdHardwareSilent() noexcept override;
    bool holdTransmitInhibited() noexcept override;
    [[nodiscard]] bool safeStateConfirmed() const noexcept override;

private:
    GpioHal& gpio_;
    TwaiSafetyPinConfig config_{};
    bool silentHeld_{false};
    bool transmitInhibited_{false};
};

enum class TwaiOperatingMode : std::uint8_t {
    ListenOnly,
    Normal,
};

enum class TwaiHardwareTopology : std::uint8_t {
    LegacyBarrieredBench,
    Phase3hBidirectional,
};

struct TwaiTransceiverModePinConfig final {
    int standbyPin{-1};
    int enablePin{-1};

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return standbyPin >= 0 && standbyPin <= 48 &&
               enablePin >= 0 && enablePin <= 48 &&
               standbyPin != enablePin;
    }
};

struct TwaiDriverConfig final {
    can_core::CanReceiverConfig receiver{};
    int receivePin{-1};
    int transmitPin{-1};
    std::uint16_t driverReceiveQueueDepth{64U};
    std::uint16_t receiveTimeoutMs{10U};
    std::uint8_t taskPriority{8U};
    std::int8_t taskCore{0};
    TwaiOperatingMode operatingMode{TwaiOperatingMode::ListenOnly};
    TwaiHardwareTopology hardwareTopology{
        TwaiHardwareTopology::LegacyBarrieredBench};
    TwaiTransceiverModePinConfig transceiverModePins{};

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return receiver.isValid() &&
               receivePin >= 0 && receivePin <= 48 &&
               transmitPin >= 0 && transmitPin <= 48 &&
               receivePin != transmitPin &&
               driverReceiveQueueDepth > 0U &&
               driverReceiveQueueDepth <= 256U &&
               receiveTimeoutMs > 0U && receiveTimeoutMs <= 100U &&
               taskPriority > 0U && taskPriority <= 24U &&
               (taskCore == 0 || taskCore == 1) &&
               ((hardwareTopology == TwaiHardwareTopology::LegacyBarrieredBench &&
                 operatingMode == TwaiOperatingMode::ListenOnly) ||
                (hardwareTopology == TwaiHardwareTopology::Phase3hBidirectional &&
                 transceiverModePins.isValid() &&
                 transceiverModePins.standbyPin != receivePin &&
                 transceiverModePins.standbyPin != transmitPin &&
                 transceiverModePins.enablePin != receivePin &&
                 transceiverModePins.enablePin != transmitPin));
    }
};

// Compatibility alias for Phase 3 callers. New code should use TwaiDriverConfig.
using TwaiListenOnlyConfig = TwaiDriverConfig;

[[nodiscard]] bool prepareTwaiListenOnlySafety(
    TwaiSafetyHal& safety) noexcept;

[[nodiscard]] bool activatePhase3hTransceiver(
    GpioHal& gpio,
    const TwaiTransceiverModePinConfig& pins) noexcept;

void deactivatePhase3hTransceiver(
    GpioHal& gpio,
    const TwaiTransceiverModePinConfig& pins) noexcept;

}  // namespace bmw::remote::infrastructure
