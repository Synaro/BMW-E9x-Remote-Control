#include "bmw_remote/application/user_settings.hpp"

namespace bmw::remote::application {
namespace {

template <typename T>
[[nodiscard]] constexpr bool outside(
    const T value,
    const T minimum,
    const T maximum) noexcept {
    return value < minimum || value > maximum;
}

}  // namespace

UserSettingsValidation validateUserSettings(
    const UserSettings& settings) noexcept {
    UserSettingsValidation validation{};

    switch (settings.hoodMonitoring) {
        case HoodMonitoringMode::Required:
        case HoodMonitoringMode::Disabled:
            break;
        default:
            validation.add(UserSettingsReason::UnsupportedHoodMode);
            break;
    }

    switch (settings.driverEntryMode) {
        case DriverEntryMode::RequireTakeover:
        case DriverEntryMode::StopImmediately:
            break;
        default:
            validation.add(UserSettingsReason::UnsupportedDriverEntryMode);
            break;
    }

    if (outside(
            settings.maximumRemoteRunTimeMs,
            UserSettingsLimits::MinimumRemoteRunTimeMs,
            UserSettingsLimits::MaximumRemoteRunTimeMs)) {
        validation.add(UserSettingsReason::RemoteRunTimeOutOfRange);
    }

    if (outside(
            settings.driverTakeoverTimeoutMs,
            UserSettingsLimits::MinimumTakeoverTimeoutMs,
            UserSettingsLimits::MaximumTakeoverTimeoutMs)) {
        validation.add(UserSettingsReason::TakeoverTimeoutOutOfRange);
    }

    if (outside(
            settings.lockPressCount,
            UserSettingsLimits::MinimumLockPressCount,
            UserSettingsLimits::MaximumLockPressCount)) {
        validation.add(UserSettingsReason::LockPressCountOutOfRange);
    }

    if (outside(
            settings.lockMinimumGapMs,
            UserSettingsLimits::MinimumLockGapMs,
            UserSettingsLimits::MaximumLockGapMs)) {
        validation.add(UserSettingsReason::LockMinimumGapOutOfRange);
    }

    if (outside(
            settings.lockMaximumGapMs,
            UserSettingsLimits::MinimumLockGapMs,
            UserSettingsLimits::MaximumLockGapMs)) {
        validation.add(UserSettingsReason::LockMaximumGapOutOfRange);
    }

    if (outside(
            settings.lockMaximumSequenceMs,
            UserSettingsLimits::MinimumLockSequenceMs,
            UserSettingsLimits::MaximumLockSequenceMs)) {
        validation.add(UserSettingsReason::LockSequenceTimeOutOfRange);
    }

    const std::uint32_t requiredIntervals =
        settings.lockPressCount == 0U
            ? 0U
            : static_cast<std::uint32_t>(settings.lockPressCount - 1U);
    const std::uint64_t minimumSequenceTime =
        static_cast<std::uint64_t>(requiredIntervals) * settings.lockMinimumGapMs;

    if (settings.lockMinimumGapMs > settings.lockMaximumGapMs ||
        settings.lockMaximumGapMs > settings.lockMaximumSequenceMs ||
        minimumSequenceTime > settings.lockMaximumSequenceMs) {
        validation.add(UserSettingsReason::InconsistentLockTiming);
    }

    return validation;
}

UserConfiguration makeUserConfiguration(
    const UserSettings& settings,
    const domain::VehicleProfile* const vehicleProfile) noexcept {
    UserConfiguration configuration{};
    configuration.validation = validateUserSettings(settings);
    configuration.controller.vehicleProfile = vehicleProfile;

    if (!configuration.validation.valid()) {
        configuration.controller.remoteStartEnabled = false;
        return configuration;
    }

    configuration.controller.remoteStartEnabled = settings.remoteStartEnabled;
    configuration.controller.driverEntryMode = settings.driverEntryMode;
    configuration.controller.safety.requireHoodClosed =
        settings.hoodMonitoring == HoodMonitoringMode::Required;
    configuration.controller.maximumRemoteRunTimeMs =
        settings.maximumRemoteRunTimeMs;
    configuration.controller.driverTakeoverTimeoutMs =
        settings.driverTakeoverTimeoutMs;

    configuration.lockSequence.requiredPresses = settings.lockPressCount;
    configuration.lockSequence.minimumGapMs = settings.lockMinimumGapMs;
    configuration.lockSequence.maximumGapMs = settings.lockMaximumGapMs;
    configuration.lockSequence.maximumSequenceMs =
        settings.lockMaximumSequenceMs;
    return configuration;
}

const char* toString(const HoodMonitoringMode mode) noexcept {
    switch (mode) {
        case HoodMonitoringMode::Required: return "required";
        case HoodMonitoringMode::Disabled: return "disabled";
    }
    return "unknown";
}

const char* toString(const DriverEntryMode mode) noexcept {
    switch (mode) {
        case DriverEntryMode::RequireTakeover: return "require_takeover";
        case DriverEntryMode::StopImmediately: return "stop_immediately";
    }
    return "unknown";
}

}  // namespace bmw::remote::application
