#include "bmw_remote/infrastructure/settings_protocol.hpp"

namespace bmw::remote::infrastructure {
namespace {

constexpr std::array<std::uint8_t, 4U> Magic = {'B', 'M', 'C', 'F'};
constexpr std::size_t VersionOffset = 4U;
constexpr std::size_t TypeOffset = 5U;
constexpr std::size_t StatusOffset = 6U;
constexpr std::size_t ReservedOffset = 7U;
constexpr std::size_t RequestIdOffset = 8U;
constexpr std::size_t PayloadSizeOffset = 10U;
constexpr std::size_t PayloadOffset = SettingsProtocolCodec::HeaderSize;

void writeU16(
    std::uint8_t* const destination,
    const std::uint16_t value) noexcept {
    destination[0] = static_cast<std::uint8_t>(value & 0xFFU);
    destination[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

void writeU32(
    std::uint8_t* const destination,
    const std::uint32_t value) noexcept {
    destination[0] = static_cast<std::uint8_t>(value & 0xFFU);
    destination[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    destination[2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    destination[3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

[[nodiscard]] std::uint16_t readU16(
    const std::uint8_t* const source) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(source[0]) |
        static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(source[1]) << 8U));
}

[[nodiscard]] std::uint32_t readU32(
    const std::uint8_t* const source) noexcept {
    return static_cast<std::uint32_t>(source[0]) |
           (static_cast<std::uint32_t>(source[1]) << 8U) |
           (static_cast<std::uint32_t>(source[2]) << 16U) |
           (static_cast<std::uint32_t>(source[3]) << 24U);
}

[[nodiscard]] std::uint32_t crc32(
    const std::uint8_t* const data,
    const std::size_t size) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t index = 0U; index < size; ++index) {
        crc ^= data[index];
        for (std::uint8_t bit = 0U; bit < 8U; ++bit) {
            const std::uint32_t mask =
                static_cast<std::uint32_t>(0U - (crc & 1U));
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

[[nodiscard]] SettingsProtocolFrame responseFor(
    const SettingsProtocolFrame& request,
    const SettingsMessageType responseType,
    const SettingsProtocolStatus status) noexcept {
    SettingsProtocolFrame response{};
    response.type = responseType;
    response.status = status;
    response.requestId = request.requestId;
    return response;
}

}  // namespace

bool SettingsProtocolCodec::encode(
    const SettingsProtocolFrame& frame,
    EncodedFrame& encoded,
    std::size_t& encodedSize) noexcept {
    encoded.fill(0U);
    encodedSize = 0U;
    if (frame.payloadSize > UserSettingsPayloadSize) {
        return false;
    }

    for (std::size_t index = 0U; index < Magic.size(); ++index) {
        encoded[index] = Magic[index];
    }
    encoded[VersionOffset] = Version;
    encoded[TypeOffset] = static_cast<std::uint8_t>(frame.type);
    encoded[StatusOffset] = static_cast<std::uint8_t>(frame.status);
    encoded[ReservedOffset] = 0U;
    writeU16(encoded.data() + RequestIdOffset, frame.requestId);
    writeU16(encoded.data() + PayloadSizeOffset, frame.payloadSize);
    for (std::size_t index = 0U; index < frame.payloadSize; ++index) {
        encoded[PayloadOffset + index] = frame.payload[index];
    }

    const std::size_t checksumOffset = PayloadOffset + frame.payloadSize;
    writeU32(encoded.data() + checksumOffset, crc32(encoded.data(), checksumOffset));
    encodedSize = checksumOffset + ChecksumSize;
    return true;
}

SettingsFrameDecodeResult SettingsProtocolCodec::decode(
    const std::uint8_t* const encoded,
    const std::size_t encodedSize) noexcept {
    SettingsFrameDecodeResult result{};
    if (encoded == nullptr || encodedSize < MinimumFrameSize) {
        result.status = SettingsFrameDecodeStatus::TooShort;
        return result;
    }
    if (encodedSize > MaximumFrameSize) {
        result.status = SettingsFrameDecodeStatus::TooLong;
        return result;
    }
    for (std::size_t index = 0U; index < Magic.size(); ++index) {
        if (encoded[index] != Magic[index]) {
            result.status = SettingsFrameDecodeStatus::InvalidMagic;
            return result;
        }
    }
    if (encoded[VersionOffset] != Version) {
        result.status = SettingsFrameDecodeStatus::UnsupportedVersion;
        return result;
    }
    if (encoded[ReservedOffset] != 0U) {
        result.status = SettingsFrameDecodeStatus::ReservedFieldSet;
        return result;
    }

    const std::uint16_t payloadSize = readU16(encoded + PayloadSizeOffset);
    if (payloadSize > UserSettingsPayloadSize) {
        result.status = SettingsFrameDecodeStatus::PayloadTooLarge;
        return result;
    }
    const std::size_t expectedSize =
        HeaderSize + static_cast<std::size_t>(payloadSize) + ChecksumSize;
    if (expectedSize != encodedSize) {
        result.status = SettingsFrameDecodeStatus::SizeMismatch;
        return result;
    }

    const std::size_t checksumOffset = HeaderSize + payloadSize;
    if (readU32(encoded + checksumOffset) != crc32(encoded, checksumOffset)) {
        result.status = SettingsFrameDecodeStatus::ChecksumMismatch;
        return result;
    }

    result.frame.type = static_cast<SettingsMessageType>(encoded[TypeOffset]);
    result.frame.status =
        static_cast<SettingsProtocolStatus>(encoded[StatusOffset]);
    result.frame.requestId = readU16(encoded + RequestIdOffset);
    result.frame.payloadSize = payloadSize;
    for (std::size_t index = 0U; index < payloadSize; ++index) {
        result.frame.payload[index] = encoded[PayloadOffset + index];
    }
    result.status = SettingsFrameDecodeStatus::Ok;
    return result;
}

SettingsProtocolFrame SettingsProtocolService::handle(
    const SettingsProtocolFrame& request,
    const SettingsProtocolAccess access) noexcept {
    SettingsMessageType responseType = SettingsMessageType::ErrorResponse;
    switch (request.type) {
        case SettingsMessageType::ReadRequest:
            responseType = SettingsMessageType::ReadResponse;
            break;
        case SettingsMessageType::WriteRequest:
            responseType = SettingsMessageType::WriteResponse;
            break;
        default:
            return responseFor(
                request,
                SettingsMessageType::ErrorResponse,
                SettingsProtocolStatus::UnsupportedMessage);
    }

    const std::uint16_t expectedPayloadSize =
        request.type == SettingsMessageType::WriteRequest
            ? static_cast<std::uint16_t>(UserSettingsPayloadSize)
            : 0U;
    if (request.status != SettingsProtocolStatus::Ok ||
        request.payloadSize != expectedPayloadSize) {
        return responseFor(
            request,
            responseType,
            SettingsProtocolStatus::InvalidPayload);
    }
    if (!access.authorized) {
        return responseFor(
            request,
            responseType,
            SettingsProtocolStatus::Unauthorized);
    }

    if (request.type == SettingsMessageType::ReadRequest) {
        application::UserSettings settings{};
        SettingsProtocolFrame response = responseFor(
            request,
            responseType,
            SettingsProtocolStatus::SettingsUnavailable);
        if (!store_.load(settings) ||
            !encodeUserSettingsPayload(settings, response.payload)) {
            return response;
        }
        response.status = SettingsProtocolStatus::Ok;
        response.payloadSize = static_cast<std::uint16_t>(UserSettingsPayloadSize);
        return response;
    }

    if (access.controllerState != application::ControllerState::Idle) {
        return responseFor(
            request,
            responseType,
            SettingsProtocolStatus::Busy);
    }

    application::UserSettings settings{};
    if (!decodeUserSettingsPayload(request.payload, settings)) {
        return responseFor(
            request,
            responseType,
            SettingsProtocolStatus::InvalidSettings);
    }
    if (!store_.save(settings)) {
        return responseFor(
            request,
            responseType,
            SettingsProtocolStatus::StorageFailure);
    }
    return responseFor(
        request,
        responseType,
        SettingsProtocolStatus::Ok);
}

const char* toString(const SettingsProtocolStatus status) noexcept {
    switch (status) {
        case SettingsProtocolStatus::Ok: return "ok";
        case SettingsProtocolStatus::InvalidPayload: return "invalid_payload";
        case SettingsProtocolStatus::UnsupportedMessage: return "unsupported_message";
        case SettingsProtocolStatus::Unauthorized: return "unauthorized";
        case SettingsProtocolStatus::Busy: return "busy";
        case SettingsProtocolStatus::InvalidSettings: return "invalid_settings";
        case SettingsProtocolStatus::StorageFailure: return "storage_failure";
        case SettingsProtocolStatus::SettingsUnavailable: return "settings_unavailable";
    }
    return "unknown";
}

const char* toString(const SettingsFrameDecodeStatus status) noexcept {
    switch (status) {
        case SettingsFrameDecodeStatus::Ok: return "ok";
        case SettingsFrameDecodeStatus::TooShort: return "too_short";
        case SettingsFrameDecodeStatus::TooLong: return "too_long";
        case SettingsFrameDecodeStatus::InvalidMagic: return "invalid_magic";
        case SettingsFrameDecodeStatus::UnsupportedVersion: return "unsupported_version";
        case SettingsFrameDecodeStatus::ReservedFieldSet: return "reserved_field_set";
        case SettingsFrameDecodeStatus::PayloadTooLarge: return "payload_too_large";
        case SettingsFrameDecodeStatus::SizeMismatch: return "size_mismatch";
        case SettingsFrameDecodeStatus::ChecksumMismatch: return "checksum_mismatch";
    }
    return "unknown";
}

}  // namespace bmw::remote::infrastructure
