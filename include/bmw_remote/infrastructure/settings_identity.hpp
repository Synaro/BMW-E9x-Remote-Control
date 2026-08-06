#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "bmw_remote/infrastructure/settings_payload.hpp"

namespace bmw::remote::infrastructure {

inline constexpr std::size_t SettingsDeviceIdentityPayloadSize = 12U;
inline constexpr std::array<std::uint8_t, 4U> SettingsDeviceProductSignature = {
    'E', '9', 'R', 'C'};

inline constexpr std::uint8_t FirmwareVersionMajor = 0U;
inline constexpr std::uint8_t FirmwareVersionMinor = 3U;
inline constexpr std::uint8_t FirmwareVersionPatch = 0U;

enum class SettingsHardwareTarget : std::uint8_t {
    Unspecified = 0U,
    Esp32S3DevKitC1 = 1U,
    HostSimulation = 0xFEU,
};

enum class SettingsDeviceCapability : std::uint32_t {
    SettingsRead = 1U << 0U,
    SettingsWrite = 1U << 1U,
    PersistentSettings = 1U << 2U,
};

struct SettingsDeviceIdentity final {
    SettingsHardwareTarget hardwareTarget{SettingsHardwareTarget::Unspecified};
    std::uint8_t firmwareMajor{FirmwareVersionMajor};
    std::uint8_t firmwareMinor{FirmwareVersionMinor};
    std::uint8_t firmwarePatch{FirmwareVersionPatch};
    std::uint32_t capabilities{0U};
};

[[nodiscard]] constexpr std::uint32_t capabilityMask(
    const SettingsDeviceCapability capability) noexcept {
    return static_cast<std::uint32_t>(capability);
}

[[nodiscard]] constexpr bool hasCapability(
    const SettingsDeviceIdentity identity,
    const SettingsDeviceCapability capability) noexcept {
    return (identity.capabilities & capabilityMask(capability)) != 0U;
}

[[nodiscard]] constexpr SettingsDeviceIdentity settingsPrototypeIdentity(
    const SettingsHardwareTarget target) noexcept {
    return SettingsDeviceIdentity{
        target,
        FirmwareVersionMajor,
        FirmwareVersionMinor,
        FirmwareVersionPatch,
        capabilityMask(SettingsDeviceCapability::SettingsRead) |
            capabilityMask(SettingsDeviceCapability::SettingsWrite) |
            capabilityMask(SettingsDeviceCapability::PersistentSettings)};
}

[[nodiscard]] bool encodeSettingsDeviceIdentity(
    const SettingsDeviceIdentity& identity,
    UserSettingsPayload& payload) noexcept;

[[nodiscard]] bool decodeSettingsDeviceIdentity(
    const UserSettingsPayload& payload,
    std::size_t payloadSize,
    SettingsDeviceIdentity& identity) noexcept;

[[nodiscard]] const char* toString(SettingsHardwareTarget target) noexcept;

}  // namespace bmw::remote::infrastructure
