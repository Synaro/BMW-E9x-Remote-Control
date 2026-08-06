#pragma once

#include <cstddef>
#include <cstdint>

namespace bmw::remote::domain {

enum class VehicleSignal : std::uint8_t {
    BatteryMillivolts,
    EngineRpm,
    CoolantTemperatureC,
    EngineOilTemperatureC,
    TransmissionOilTemperatureC,
    DpfRegenerationActive,
    HoodClosed,
    DoorsClosed,
    TrunkClosed,
    BrakePressed,
    ParkingBrakeApplied,
    Transmission,
    Gear,
    CriticalFaultPresent,
    Count,
};

[[nodiscard]] constexpr std::size_t signalIndex(const VehicleSignal signal) noexcept {
    return static_cast<std::size_t>(signal);
}

[[nodiscard]] constexpr std::size_t vehicleSignalCount() noexcept {
    return signalIndex(VehicleSignal::Count);
}

[[nodiscard]] const char* toString(VehicleSignal signal) noexcept;

}  // namespace bmw::remote::domain
