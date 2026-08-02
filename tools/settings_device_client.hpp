#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "bmw_remote/application/user_settings.hpp"
#include "bmw_remote/infrastructure/settings_protocol.hpp"

namespace bmw::remote::host {

enum class SettingsChannelReadStatus : std::uint8_t {
    Data,
    Timeout,
    Failure,
};

class SettingsDeviceChannel {
public:
    virtual ~SettingsDeviceChannel() = default;

    virtual bool clearInput(std::string& error) = 0;

    virtual bool writeAll(
        const std::uint8_t* data,
        std::size_t size,
        std::uint32_t timeoutMs,
        std::string& error) = 0;

    virtual SettingsChannelReadStatus readByte(
        std::uint8_t& byte,
        std::uint32_t timeoutMs,
        std::string& error) = 0;
};

struct SettingsDeviceClientConfig final {
    std::uint32_t responseTimeoutMs{2'000U};
    std::uint32_t interByteTimeoutMs{250U};
};

class SettingsDeviceClient final {
public:
    explicit SettingsDeviceClient(
        SettingsDeviceChannel& channel,
        SettingsDeviceClientConfig config = {}) noexcept
        : channel_(channel), config_(config) {}

    [[nodiscard]] bool probe(
        infrastructure::SettingsDeviceIdentity& identity,
        std::string& error);

    [[nodiscard]] bool read(
        application::UserSettings& settings,
        std::string& error);

    [[nodiscard]] bool writeAndVerify(
        const application::UserSettings& settings,
        std::string& error);

private:
    [[nodiscard]] bool exchange(
        infrastructure::SettingsProtocolFrame request,
        infrastructure::SettingsMessageType expectedResponseType,
        infrastructure::SettingsProtocolFrame& response,
        std::string& error);

    [[nodiscard]] std::uint16_t allocateRequestId() noexcept;

    SettingsDeviceChannel& channel_;
    SettingsDeviceClientConfig config_{};
    std::uint16_t nextRequestId_{1U};
};

}  // namespace bmw::remote::host
