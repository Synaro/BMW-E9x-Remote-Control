#pragma once

#include <cstdint>

#include "bmw_remote/domain/vehicle_state.hpp"

namespace bmw::remote::application {

enum class SafetyReason : std::uint16_t {
    None = 0U,
    SignalUnavailable = 1U << 0U,
    BatteryTooLow = 1U << 1U,
    HoodOpen = 1U << 2U,
    DoorOpen = 1U << 3U,
    TrunkOpen = 1U << 4U,
    BrakePressed = 1U << 5U,
    ParkingBrakeReleased = 1U << 6U,
    TransmissionUnsafe = 1U << 7U,
    EngineAlreadyRunning = 1U << 8U,
    EngineNotRunning = 1U << 9U,
    CriticalVehicleFault = 1U << 10U,
};

[[nodiscard]] constexpr std::uint16_t mask(const SafetyReason reason) noexcept {
    return static_cast<std::uint16_t>(reason);
}

struct SafetyAssessment final {
    std::uint16_t reasons{mask(SafetyReason::None)};

    [[nodiscard]] constexpr bool approved() const noexcept {
        return reasons == mask(SafetyReason::None);
    }

    [[nodiscard]] constexpr bool contains(const SafetyReason reason) const noexcept {
        return (reasons & mask(reason)) != 0U;
    }

    constexpr void add(const SafetyReason reason) noexcept {
        reasons = static_cast<std::uint16_t>(reasons | mask(reason));
    }
};

struct SafetyPolicyConfig final {
    std::uint16_t minimumBatteryMillivolts{11'800U};
    std::uint16_t runningRpmThreshold{500U};
    bool requireVehicleSecured{true};
    bool requireHoodClosed{true};
    bool allowManualTransmission{false};
};

class SafetyPolicy final {
public:
    explicit constexpr SafetyPolicy(const SafetyPolicyConfig config = {}) noexcept
        : config_(config) {}

    [[nodiscard]] SafetyAssessment assessStart(
        const domain::VehicleState& vehicle) const noexcept;
    [[nodiscard]] SafetyAssessment assessCranking(
        const domain::VehicleState& vehicle) const noexcept;
    [[nodiscard]] SafetyAssessment assessRemoteRun(
        const domain::VehicleState& vehicle) const noexcept;
    [[nodiscard]] SafetyAssessment assessDriverTakeover(
        const domain::VehicleState& vehicle) const noexcept;

    [[nodiscard]] constexpr std::uint16_t runningRpmThreshold() const noexcept {
        return config_.runningRpmThreshold;
    }

private:
    [[nodiscard]] SafetyAssessment assessCommon(
        const domain::VehicleState& vehicle,
        bool requireDoorsClosed,
        bool requireTrunkClosed,
        bool prohibitBrake) const noexcept;

    SafetyPolicyConfig config_{};
};

}  // namespace bmw::remote::application
