#pragma once

#include <cstdint>

namespace bmw::remote::domain {

enum class SignalQuality : std::uint8_t {
    Unavailable,
    Stale,
    Fresh,
};

template <typename T>
struct Observed final {
    T value{};
    SignalQuality quality{SignalQuality::Unavailable};

    [[nodiscard]] constexpr bool isFresh() const noexcept {
        return quality == SignalQuality::Fresh;
    }

    [[nodiscard]] static constexpr Observed fresh(const T value) noexcept {
        return Observed{value, SignalQuality::Fresh};
    }
};

enum class Transmission : std::uint8_t {
    Unknown,
    Automatic,
    Manual,
};

enum class Gear : std::uint8_t {
    Unknown,
    Park,
    Neutral,
    Reverse,
    Drive,
};

struct VehicleState final {
    Observed<std::uint16_t> batteryMillivolts{};
    Observed<std::uint16_t> engineRpm{};
    Observed<std::int16_t> coolantTemperatureC{};
    Observed<std::int16_t> engineOilTemperatureC{};
    Observed<std::int16_t> transmissionOilTemperatureC{};
    Observed<bool> dpfRegenerationActive{};
    Observed<bool> hoodClosed{};
    Observed<bool> doorsClosed{};
    Observed<bool> trunkClosed{};
    Observed<bool> brakePressed{};
    Observed<bool> parkingBrakeApplied{};
    Observed<Transmission> transmission{};
    Observed<Gear> gear{};
    Observed<bool> criticalFaultPresent{};
};

}  // namespace bmw::remote::domain
