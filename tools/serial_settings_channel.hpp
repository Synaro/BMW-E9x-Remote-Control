#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "tools/settings_device_client.hpp"

namespace bmw::remote::host {

class SerialSettingsChannel final : public SettingsDeviceChannel {
public:
    SerialSettingsChannel() noexcept = default;
    ~SerialSettingsChannel() override;

    SerialSettingsChannel(const SerialSettingsChannel&) = delete;
    SerialSettingsChannel& operator=(const SerialSettingsChannel&) = delete;

    [[nodiscard]] bool open(
        const std::string& portName,
        std::uint32_t baudRate,
        std::string& error);

    void close() noexcept;

    [[nodiscard]] bool isOpen() const noexcept {
        return handle_ != nullptr;
    }

    bool clearInput(std::string& error) override;

    bool writeAll(
        const std::uint8_t* data,
        std::size_t size,
        std::uint32_t timeoutMs,
        std::string& error) override;

    SettingsChannelReadStatus readByte(
        std::uint8_t& byte,
        std::uint32_t timeoutMs,
        std::string& error) override;

private:
    void* handle_{nullptr};
};

[[nodiscard]] std::vector<std::string> listSerialPorts(
    std::string& error);

}  // namespace bmw::remote::host
