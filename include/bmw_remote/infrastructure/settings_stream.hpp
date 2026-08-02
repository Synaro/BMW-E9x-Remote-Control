#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "bmw_remote/infrastructure/settings_protocol.hpp"

namespace bmw::remote::infrastructure {

struct SettingsStreamConfig final {
    std::uint32_t interByteTimeoutMs{250U};
};

enum class SettingsStreamEventType : std::uint8_t {
    None = 0U,
    FrameReady,
    FrameRejected,
    TimedOut,
};

struct SettingsStreamEvent final {
    SettingsStreamEventType type{SettingsStreamEventType::None};
    SettingsFrameDecodeStatus decodeStatus{SettingsFrameDecodeStatus::Ok};
    SettingsProtocolFrame frame{};
};

class SettingsStreamReceiver final {
public:
    explicit SettingsStreamReceiver(
        SettingsStreamConfig config = {}) noexcept
        : config_(config) {}

    [[nodiscard]] SettingsStreamEvent consume(
        std::uint8_t byte,
        std::uint32_t nowMs) noexcept;

    [[nodiscard]] SettingsStreamEvent poll(std::uint32_t nowMs) noexcept;

    void reset() noexcept;

    [[nodiscard]] constexpr std::size_t bufferedSize() const noexcept {
        return bufferedSize_;
    }

private:
    [[nodiscard]] SettingsStreamEvent consumeSynchronized(
        std::uint8_t byte) noexcept;
    [[nodiscard]] bool expired(std::uint32_t nowMs) const noexcept;

    SettingsStreamConfig config_{};
    SettingsProtocolCodec::EncodedFrame buffer_{};
    std::size_t bufferedSize_{0U};
    std::size_t expectedSize_{0U};
    std::uint32_t lastByteTimeMs_{0U};
};

enum class SettingsEndpointStatus : std::uint8_t {
    None = 0U,
    ResponseSent,
    FrameRejected,
    FrameTimedOut,
    TransportFailure,
};

struct SettingsEndpointResult final {
    SettingsEndpointStatus status{SettingsEndpointStatus::None};
    SettingsFrameDecodeStatus decodeStatus{SettingsFrameDecodeStatus::Ok};
    SettingsProtocolStatus responseStatus{SettingsProtocolStatus::Ok};
};

class SettingsProtocolEndpoint final {
public:
    SettingsProtocolEndpoint(
        UserSettingsStore& store,
        SettingsTransportPort& transport,
        SettingsStreamConfig streamConfig = {}) noexcept
        : receiver_(streamConfig), service_(store), transport_(transport) {}

    [[nodiscard]] SettingsEndpointResult consume(
        std::uint8_t byte,
        std::uint32_t nowMs,
        SettingsProtocolAccess access) noexcept;

    [[nodiscard]] SettingsEndpointResult poll(std::uint32_t nowMs) noexcept;

    void reset() noexcept {
        receiver_.reset();
    }

private:
    [[nodiscard]] SettingsEndpointResult handleStreamEvent(
        const SettingsStreamEvent& event,
        SettingsProtocolAccess access) noexcept;

    SettingsStreamReceiver receiver_{};
    SettingsProtocolService service_;
    SettingsTransportPort& transport_;
};

[[nodiscard]] const char* toString(SettingsStreamEventType type) noexcept;
[[nodiscard]] const char* toString(SettingsEndpointStatus status) noexcept;

}  // namespace bmw::remote::infrastructure
