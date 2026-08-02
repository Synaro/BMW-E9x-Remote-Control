#pragma once

#include <cstdint>

#include "bmw_remote/domain/vehicle_profile.hpp"

namespace bmw::remote::application {

enum class ProfileReadinessReason : std::uint8_t {
    None = 0U,
    MissingRequiredSignal = 1U << 0U,
    UnverifiedRequiredSignal = 1U << 1U,
    QualificationTooLow = 1U << 2U,
    TransmissionUnknown = 1U << 3U,
    ManualTransmissionNotAllowed = 1U << 4U,
    ProfileNotSelected = 1U << 5U,
    HoodInterlockSourceUnknown = 1U << 6U,
};

[[nodiscard]] constexpr std::uint8_t mask(const ProfileReadinessReason reason) noexcept {
    return static_cast<std::uint8_t>(reason);
}

struct ProfileReadinessAssessment final {
    std::uint8_t reasons{mask(ProfileReadinessReason::None)};

    [[nodiscard]] constexpr bool ready() const noexcept {
        return reasons == mask(ProfileReadinessReason::None);
    }

    [[nodiscard]] constexpr bool contains(const ProfileReadinessReason reason) const noexcept {
        return (reasons & mask(reason)) != 0U;
    }

    constexpr void add(const ProfileReadinessReason reason) noexcept {
        reasons = static_cast<std::uint8_t>(reasons | mask(reason));
    }
};

struct ProfileReadinessPolicy final {
    bool requireVehicleSecured{true};
    bool allowManualTransmission{false};
};

[[nodiscard]] ProfileReadinessAssessment assessRemoteStartReadiness(
    const domain::VehicleProfile& profile,
    ProfileReadinessPolicy policy = {}) noexcept;

[[nodiscard]] ProfileReadinessAssessment assessRemoteStartReadiness(
    const domain::VehicleProfile* profile,
    ProfileReadinessPolicy policy = {}) noexcept;

}  // namespace bmw::remote::application
