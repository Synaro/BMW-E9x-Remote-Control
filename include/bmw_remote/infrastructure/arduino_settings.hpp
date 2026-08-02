#pragma once

#if defined(ARDUINO)

#include <Arduino.h>

#include <cstddef>
#include <cstdint>

#include "bmw_remote/infrastructure/settings_storage.hpp"

namespace bmw::remote::infrastructure {

class ArduinoEepromSettingsStorage final : public SettingsByteStorage {
public:
    [[nodiscard]] bool begin() noexcept;

    [[nodiscard]] std::size_t capacity() const noexcept override;

    bool read(
        std::size_t offset,
        std::uint8_t* destination,
        std::size_t size) noexcept override;

    bool write(
        std::size_t offset,
        const std::uint8_t* source,
        std::size_t size) noexcept override;

    bool commit() noexcept override;

private:
    [[nodiscard]] static bool validRange(
        std::size_t offset,
        std::size_t size) noexcept;

    bool ready_{false};
};

class ArduinoStreamSettingsTransport final : public SettingsTransportPort {
public:
    explicit ArduinoStreamSettingsTransport(Stream& stream) noexcept
        : stream_(stream) {}

    bool send(
        const std::uint8_t* data,
        std::size_t size) noexcept override;

private:
    Stream& stream_;
};

}  // namespace bmw::remote::infrastructure

#endif
