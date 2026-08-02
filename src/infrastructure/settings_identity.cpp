#include "bmw_remote/infrastructure/settings_identity.hpp"

#include <cstddef>
#include <cstdint>

namespace bmw::remote::infrastructure {
namespace {

constexpr std::size_t TargetOffset = 4U;
constexpr std::size_t MajorOffset = 5U;
constexpr std::size_t MinorOffset = 6U;
constexpr std::size_t PatchOffset = 7U;
constexpr std::size_t CapabilitiesOffset = 8U;

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

bool encodeSettingsDeviceIdentity(
    const SettingsDeviceIdentity& identity,
    UserSettingsPayload& payload) noexcept {
    payload.fill(0U);
    for (std::size_t index = 0U;
         index < SettingsDeviceProductSignature.size();
         ++index) {
        payload[index] = SettingsDeviceProductSignature[index];
    }
    payload[TargetOffset] = static_cast<std::uint8_t>(identity.hardwareTarget);
    payload[MajorOffset] = identity.firmwareMajor;
    payload[MinorOffset] = identity.firmwareMinor;
    payload[PatchOffset] = identity.firmwarePatch;
    writeU32(payload.data() + CapabilitiesOffset, identity.capabilities);
    return true;
}

bool decodeSettingsDeviceIdentity(
    const UserSettingsPayload& payload,
    const std::size_t payloadSize,
    SettingsDeviceIdentity& identity) noexcept {
    if (payloadSize != SettingsDeviceIdentityPayloadSize) {
        return false;
    }
    for (std::size_t index = 0U;
         index < SettingsDeviceProductSignature.size();
         ++index) {
        if (payload[index] != SettingsDeviceProductSignature[index]) {
            return false;
        }
    }

    identity.hardwareTarget =
        static_cast<SettingsHardwareTarget>(payload[TargetOffset]);
    identity.firmwareMajor = payload[MajorOffset];
    identity.firmwareMinor = payload[MinorOffset];
    identity.firmwarePatch = payload[PatchOffset];
    identity.capabilities = readU32(payload.data() + CapabilitiesOffset);
    return true;
}

const char* toString(const SettingsHardwareTarget target) noexcept {
    switch (target) {
        case SettingsHardwareTarget::Unspecified: return "unspecified";
        case SettingsHardwareTarget::Esp32S3DevKitC1:
            return "esp32-s3-devkitc-1";
        case SettingsHardwareTarget::HostSimulation: return "host-simulation";
    }
    return "unknown";
}

}  // namespace bmw::remote::infrastructure
