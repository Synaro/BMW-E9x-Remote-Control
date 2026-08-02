#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "bmw_remote/application/controller.hpp"
#include "bmw_remote/infrastructure/ports.hpp"
#include "bmw_remote/infrastructure/settings_identity.hpp"
#include "bmw_remote/infrastructure/settings_payload.hpp"

namespace bmw::remote::infrastructure {

enum class SettingsMessageType : std::uint8_t {
    ReadRequest = 0x01U,
    WriteRequest = 0x02U,
    IdentifyRequest = 0x03U,
    ReadResponse = 0x81U,
    WriteResponse = 0x82U,
    IdentifyResponse = 0x83U,
    ErrorResponse = 0xFFU,
};

enum class SettingsProtocolStatus : std::uint8_t {
    Ok = 0U,
    InvalidPayload = 1U,
    UnsupportedMessage = 2U,
    Unauthorized = 3U,
    Busy = 4U,
    InvalidSettings = 5U,
    StorageFailure = 6U,
    SettingsUnavailable = 7U,
};

enum class SettingsFrameDecodeStatus : std::uint8_t {
    Ok = 0U,
    TooShort,
    TooLong,
    InvalidMagic,
    UnsupportedVersion,
    ReservedFieldSet,
    PayloadTooLarge,
    SizeMismatch,
    ChecksumMismatch,
};

struct SettingsProtocolFrame final {
    SettingsMessageType type{SettingsMessageType::ErrorResponse};
    SettingsProtocolStatus status{SettingsProtocolStatus::Ok};
    std::uint16_t requestId{0U};
    std::uint16_t payloadSize{0U};
    UserSettingsPayload payload{};
};

struct SettingsFrameDecodeResult final {
    SettingsFrameDecodeStatus status{SettingsFrameDecodeStatus::TooShort};
    SettingsProtocolFrame frame{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return status == SettingsFrameDecodeStatus::Ok;
    }
};

class SettingsProtocolCodec final {
public:
    static constexpr std::array<std::uint8_t, 4U> Magic = {'B', 'M', 'C', 'F'};
    static constexpr std::uint8_t Version = 1U;
    static constexpr std::size_t HeaderSize = 12U;
    static constexpr std::size_t ChecksumSize = 4U;
    static constexpr std::size_t MinimumFrameSize = HeaderSize + ChecksumSize;
    static constexpr std::size_t MaximumFrameSize =
        HeaderSize + UserSettingsPayloadSize + ChecksumSize;
    using EncodedFrame = std::array<std::uint8_t, MaximumFrameSize>;

    [[nodiscard]] static bool encode(
        const SettingsProtocolFrame& frame,
        EncodedFrame& encoded,
        std::size_t& encodedSize) noexcept;

    [[nodiscard]] static SettingsFrameDecodeResult decode(
        const std::uint8_t* encoded,
        std::size_t encodedSize) noexcept;
};

struct SettingsProtocolAccess final {
    bool authorized{false};
    application::ControllerState controllerState{
        application::ControllerState::Idle};
};

class SettingsProtocolService final {
public:
    explicit SettingsProtocolService(
        UserSettingsStore& store,
        SettingsDeviceIdentity identity = settingsPrototypeIdentity(
            SettingsHardwareTarget::HostSimulation)) noexcept
        : store_(store), identity_(identity) {}

    [[nodiscard]] SettingsProtocolFrame handle(
        const SettingsProtocolFrame& request,
        SettingsProtocolAccess access) noexcept;

private:
    UserSettingsStore& store_;
    SettingsDeviceIdentity identity_{};
};

[[nodiscard]] const char* toString(SettingsProtocolStatus status) noexcept;
[[nodiscard]] const char* toString(SettingsFrameDecodeStatus status) noexcept;

}  // namespace bmw::remote::infrastructure
