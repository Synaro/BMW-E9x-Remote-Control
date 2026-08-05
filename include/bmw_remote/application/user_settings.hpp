#pragma once

#include <cstdint>

#include "bmw_remote/application/controller.hpp"
#include "bmw_remote/application/feature_catalog.hpp"
#include "bmw_remote/application/lock_sequence_detector.hpp"

namespace bmw::remote::application {

enum class HoodMonitoringMode : std::uint8_t {
    Required,
    Disabled,
};

struct UserSettings final {
    bool remoteStartEnabled{true};
    HoodMonitoringMode hoodMonitoring{HoodMonitoringMode::Required};
    DriverEntryMode driverEntryMode{DriverEntryMode::RequireTakeover};
    std::uint32_t maximumRemoteRunTimeMs{15U * 60U * 1'000U};
    std::uint32_t driverTakeoverTimeoutMs{60'000U};
    std::uint8_t lockPressCount{3U};
    std::uint32_t lockMinimumGapMs{80U};
    std::uint32_t lockMaximumGapMs{1'500U};
    std::uint32_t lockMaximumSequenceMs{3'000U};
    FeatureRequests features{};
};

struct UserSettingsLimits final {
    static constexpr std::uint32_t MinimumRemoteRunTimeMs{60'000U};
    static constexpr std::uint32_t MaximumRemoteRunTimeMs{60U * 60U * 1'000U};
    static constexpr std::uint32_t MinimumTakeoverTimeoutMs{10'000U};
    static constexpr std::uint32_t MaximumTakeoverTimeoutMs{5U * 60U * 1'000U};
    static constexpr std::uint8_t MinimumLockPressCount{2U};
    static constexpr std::uint8_t MaximumLockPressCount{5U};
    static constexpr std::uint32_t MinimumLockGapMs{50U};
    static constexpr std::uint32_t MaximumLockGapMs{5'000U};
    static constexpr std::uint32_t MinimumLockSequenceMs{500U};
    static constexpr std::uint32_t MaximumLockSequenceMs{15'000U};
};

enum class UserSettingsReason : std::uint16_t {
    None = 0U,
    UnsupportedHoodMode = 1U << 0U,
    UnsupportedDriverEntryMode = 1U << 1U,
    RemoteRunTimeOutOfRange = 1U << 2U,
    TakeoverTimeoutOutOfRange = 1U << 3U,
    LockPressCountOutOfRange = 1U << 4U,
    LockMinimumGapOutOfRange = 1U << 5U,
    LockMaximumGapOutOfRange = 1U << 6U,
    LockSequenceTimeOutOfRange = 1U << 7U,
    InconsistentLockTiming = 1U << 8U,
    InvalidFeatureMask = 1U << 9U,
};

[[nodiscard]] constexpr std::uint16_t userSettingsMask(
    const UserSettingsReason reason) noexcept {
    return static_cast<std::uint16_t>(reason);
}

struct UserSettingsValidation final {
    std::uint16_t reasons{userSettingsMask(UserSettingsReason::None)};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return reasons == userSettingsMask(UserSettingsReason::None);
    }

    [[nodiscard]] constexpr bool contains(
        const UserSettingsReason reason) const noexcept {
        return (reasons & userSettingsMask(reason)) != 0U;
    }

    constexpr void add(const UserSettingsReason reason) noexcept {
        reasons = static_cast<std::uint16_t>(reasons | userSettingsMask(reason));
    }
};

struct UserConfiguration final {
    UserSettingsValidation validation{};
    ControllerConfig controller{};
    LockSequenceConfig lockSequence{};
};

[[nodiscard]] UserSettingsValidation validateUserSettings(
    const UserSettings& settings) noexcept;

[[nodiscard]] UserConfiguration makeUserConfiguration(
    const UserSettings& settings,
    const domain::VehicleProfile* vehicleProfile) noexcept;

[[nodiscard]] const char* toString(HoodMonitoringMode mode) noexcept;
[[nodiscard]] const char* toString(DriverEntryMode mode) noexcept;

}  // namespace bmw::remote::application
