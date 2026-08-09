#pragma once

#if defined(ESP_PLATFORM)

#include <array>
#include <cstddef>
#include <cstdint>

#include "bmw_remote/infrastructure/settings_storage.hpp"

#include "nvs.h"

namespace bmw::remote::infrastructure {

class EspIdfNvsSettingsStorage final : public SettingsByteStorage {
public:
    EspIdfNvsSettingsStorage() noexcept = default;
    ~EspIdfNvsSettingsStorage() override;

    EspIdfNvsSettingsStorage(const EspIdfNvsSettingsStorage&) = delete;
    EspIdfNvsSettingsStorage& operator=(
        const EspIdfNvsSettingsStorage&) = delete;

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
    using StorageImage = std::array<
        std::uint8_t,
        JournaledUserSettingsStore::RequiredCapacity>;

    [[nodiscard]] static bool validRange(
        std::size_t offset,
        std::size_t size) noexcept;

    nvs_handle_t handle_{0U};
    StorageImage committed_{};
    StorageImage pending_{};
    bool ready_{false};
    bool dirty_{false};
};

class EspIdfUsbSerialJtagSettingsTransport final
    : public SettingsTransportPort {
public:
    [[nodiscard]] bool begin() noexcept;
    [[nodiscard]] std::size_t read(
        std::uint8_t* destination,
        std::size_t capacity,
        std::uint32_t timeoutMs) noexcept;

    bool send(
        const std::uint8_t* data,
        std::size_t size) noexcept override;

private:
    bool ready_{false};
};

}  // namespace bmw::remote::infrastructure

#endif
