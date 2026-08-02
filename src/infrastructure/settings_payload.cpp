#include "bmw_remote/infrastructure/settings_payload.hpp"

namespace bmw::remote::infrastructure {
namespace {

void writeU32(
    std::uint8_t* const destination,
    const std::uint32_t value) noexcept {
    destination[0] = static_cast<std::uint8_t>(value & 0xFFU);
    destination[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    destination[2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    destination[3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

[[nodiscard]] std::uint32_t readU32(
    const std::uint8_t* const source) noexcept {
    return static_cast<std::uint32_t>(source[0]) |
           (static_cast<std::uint32_t>(source[1]) << 8U) |
           (static_cast<std::uint32_t>(source[2]) << 16U) |
           (static_cast<std::uint32_t>(source[3]) << 24U);
}

}  // namespace

bool encodeUserSettingsPayload(
    const application::UserSettings& settings,
    UserSettingsPayload& payload) noexcept {
    if (!application::validateUserSettings(settings).valid()) {
        return false;
    }

    payload.fill(0U);
    payload[0] = settings.remoteStartEnabled ? 1U : 0U;
    payload[1] = static_cast<std::uint8_t>(settings.hoodMonitoring);
    payload[2] = static_cast<std::uint8_t>(settings.driverEntryMode);
    payload[3] = settings.lockPressCount;
    writeU32(payload.data() + 4U, settings.maximumRemoteRunTimeMs);
    writeU32(payload.data() + 8U, settings.driverTakeoverTimeoutMs);
    writeU32(payload.data() + 12U, settings.lockMinimumGapMs);
    writeU32(payload.data() + 16U, settings.lockMaximumGapMs);
    writeU32(payload.data() + 20U, settings.lockMaximumSequenceMs);
    return true;
}

bool decodeUserSettingsPayload(
    const UserSettingsPayload& payload,
    application::UserSettings& settings) noexcept {
    if (payload[0] > 1U) {
        return false;
    }

    application::UserSettings decoded{};
    decoded.remoteStartEnabled = payload[0] == 1U;
    decoded.hoodMonitoring =
        static_cast<application::HoodMonitoringMode>(payload[1]);
    decoded.driverEntryMode =
        static_cast<application::DriverEntryMode>(payload[2]);
    decoded.lockPressCount = payload[3];
    decoded.maximumRemoteRunTimeMs = readU32(payload.data() + 4U);
    decoded.driverTakeoverTimeoutMs = readU32(payload.data() + 8U);
    decoded.lockMinimumGapMs = readU32(payload.data() + 12U);
    decoded.lockMaximumGapMs = readU32(payload.data() + 16U);
    decoded.lockMaximumSequenceMs = readU32(payload.data() + 20U);

    if (!application::validateUserSettings(decoded).valid()) {
        return false;
    }

    settings = decoded;
    return true;
}

bool userSettingsEqual(
    const application::UserSettings& left,
    const application::UserSettings& right) noexcept {
    return left.remoteStartEnabled == right.remoteStartEnabled &&
           left.hoodMonitoring == right.hoodMonitoring &&
           left.driverEntryMode == right.driverEntryMode &&
           left.maximumRemoteRunTimeMs == right.maximumRemoteRunTimeMs &&
           left.driverTakeoverTimeoutMs == right.driverTakeoverTimeoutMs &&
           left.lockPressCount == right.lockPressCount &&
           left.lockMinimumGapMs == right.lockMinimumGapMs &&
           left.lockMaximumGapMs == right.lockMaximumGapMs &&
           left.lockMaximumSequenceMs == right.lockMaximumSequenceMs;
}

}  // namespace bmw::remote::infrastructure
