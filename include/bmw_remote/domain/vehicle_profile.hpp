#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "bmw_remote/domain/vehicle_signal.hpp"
#include "bmw_remote/domain/vehicle_state.hpp"

namespace bmw::remote::domain {

enum class VehiclePlatform : std::uint8_t {
    Unknown,
    BmwE9x,
};

enum class BodyVariant : std::uint8_t {
    Unknown,
    E90,
    E91,
    E92,
    E93,
};

enum class FuelType : std::uint8_t {
    Unknown,
    Gasoline,
    Diesel,
};

enum class QualificationStage : std::uint8_t {
    Discovery,
    ReadOnlyValidated,
    BenchValidated,
    VehicleQualified,
};

enum class SignalSupport : std::uint8_t {
    Unavailable,
    Candidate,
    Verified,
};

struct VehicleProfile final {
    const char* id{"unknown"};
    const char* displayName{"Unknown vehicle"};
    VehiclePlatform platform{VehiclePlatform::Unknown};
    BodyVariant body{BodyVariant::Unknown};
    std::uint16_t firstModelYear{0U};
    std::uint16_t lastModelYear{0U};
    const char* engineCode{"unknown"};
    FuelType fuel{FuelType::Unknown};
    Transmission transmission{Transmission::Unknown};
    QualificationStage qualification{QualificationStage::Discovery};
    std::array<SignalSupport, vehicleSignalCount()> signals{};

    [[nodiscard]] constexpr SignalSupport support(const VehicleSignal signal) const noexcept {
        const std::size_t index = signalIndex(signal);
        return index < signals.size() ? signals[index] : SignalSupport::Unavailable;
    }
};

class VehicleProfileRegistry final {
public:
    constexpr VehicleProfileRegistry(
        const VehicleProfile* const profiles,
        const std::size_t profileCount) noexcept
        : profiles_(profiles), profileCount_(profileCount) {}

    [[nodiscard]] const VehicleProfile* find(const char* id) const noexcept;
    [[nodiscard]] constexpr const VehicleProfile* data() const noexcept { return profiles_; }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return profileCount_; }

private:
    const VehicleProfile* profiles_{nullptr};
    std::size_t profileCount_{0U};
};

[[nodiscard]] const char* toString(VehiclePlatform platform) noexcept;
[[nodiscard]] const char* toString(BodyVariant body) noexcept;
[[nodiscard]] const char* toString(QualificationStage stage) noexcept;
[[nodiscard]] const char* toString(SignalSupport support) noexcept;

}  // namespace bmw::remote::domain
