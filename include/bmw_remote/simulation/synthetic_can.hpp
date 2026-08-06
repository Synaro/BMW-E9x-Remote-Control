#pragma once

#include <cstdint>

#include "bmw_remote/domain/vehicle_profile.hpp"
#include "bmw_remote/domain/vehicle_state.hpp"
#include "bmw_remote/infrastructure/can_frame.hpp"
#include "bmw_remote/infrastructure/vehicle_state_assembler.hpp"

namespace bmw::remote::simulation {

// These extended identifiers are reserved for offline simulation in this project.
// They are not BMW identifiers and must never be transmitted to a vehicle.
struct SyntheticCanProtocol final {
    static constexpr std::uint32_t PowertrainFrameIdentifier = 0x1FFFFF00U;
    static constexpr std::uint32_t BodyFrameIdentifier = 0x1FFFFF01U;
    static constexpr std::uint32_t TelemetryFrameIdentifier = 0x1FFFFF02U;
    static constexpr std::uint8_t Signature = 0xA5U;
};

struct SyntheticPowertrainState final {
    std::uint16_t batteryMillivolts{12'500U};
    std::uint16_t engineRpm{0U};
    domain::Transmission transmission{domain::Transmission::Automatic};
    domain::Gear gear{domain::Gear::Park};
    bool criticalFaultPresent{false};
};

struct SyntheticBodyState final {
    bool hoodClosed{true};
    bool doorsClosed{true};
    bool trunkClosed{true};
    bool brakePressed{false};
    bool parkingBrakeApplied{true};
};

struct SyntheticTelemetryState final {
    std::int16_t coolantTemperatureC{20};
    std::int16_t engineOilTemperatureC{20};
    std::int16_t transmissionOilTemperatureC{20};
    bool dpfRegenerationActive{false};
};

class SyntheticCanDecoder final : public infrastructure::CanFrameDecoder {
public:
    [[nodiscard]] infrastructure::DecodeResult decode(
        const infrastructure::CanFrame& frame,
        infrastructure::DecodedSignalBatch& output) const noexcept override;
};

[[nodiscard]] const domain::VehicleProfile& syntheticVehicleProfile() noexcept;

[[nodiscard]] infrastructure::CanFrame makeSyntheticPowertrainFrame(
    std::uint32_t timestampMs,
    SyntheticPowertrainState state = {}) noexcept;

[[nodiscard]] infrastructure::CanFrame makeSyntheticBodyFrame(
    std::uint32_t timestampMs,
    SyntheticBodyState state = {}) noexcept;

[[nodiscard]] infrastructure::CanFrame makeSyntheticTelemetryFrame(
    std::uint32_t timestampMs,
    SyntheticTelemetryState state = {}) noexcept;

}  // namespace bmw::remote::simulation
