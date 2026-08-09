#if defined(ESP_PLATFORM)

#include "bmw_remote/infrastructure/esp_idf_settings.hpp"

#include "driver/usb_serial_jtag.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"

namespace bmw::remote::infrastructure {
namespace {

constexpr char NvsPartition[] = "nvs";
constexpr char NvsNamespace[] = "bmw_remote";
constexpr char SettingsKey[] = "settings";
constexpr std::size_t UsbReceiveBufferSize = 256U;
constexpr std::size_t UsbTransmitBufferSize = 256U;
constexpr std::uint32_t UsbWriteTimeoutMs = 100U;

}  // namespace

EspIdfNvsSettingsStorage::~EspIdfNvsSettingsStorage() {
    if (handle_ != 0U) {
        nvs_close(handle_);
    }
}

bool EspIdfNvsSettingsStorage::begin() noexcept {
    if (ready_) {
        return true;
    }
    if (nvs_flash_init_partition(NvsPartition) != ESP_OK ||
        nvs_open_from_partition(
            NvsPartition,
            NvsNamespace,
            NVS_READWRITE,
            &handle_) != ESP_OK) {
        handle_ = 0U;
        return false;
    }

    committed_.fill(0xFFU);
    std::size_t storedSize = committed_.size();
    const esp_err_t loadResult = nvs_get_blob(
        handle_,
        SettingsKey,
        committed_.data(),
        &storedSize);
    if (loadResult != ESP_OK && loadResult != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle_);
        handle_ = 0U;
        return false;
    }
    if (loadResult == ESP_OK && storedSize != committed_.size()) {
        nvs_close(handle_);
        handle_ = 0U;
        return false;
    }

    pending_ = committed_;
    dirty_ = false;
    ready_ = true;
    return true;
}

std::size_t EspIdfNvsSettingsStorage::capacity() const noexcept {
    return committed_.size();
}

bool EspIdfNvsSettingsStorage::read(
    const std::size_t offset,
    std::uint8_t* const destination,
    const std::size_t size) noexcept {
    if (!ready_ || (destination == nullptr && size != 0U) ||
        !validRange(offset, size)) {
        return false;
    }
    for (std::size_t index = 0U; index < size; ++index) {
        destination[index] = committed_[offset + index];
    }
    return true;
}

bool EspIdfNvsSettingsStorage::write(
    const std::size_t offset,
    const std::uint8_t* const source,
    const std::size_t size) noexcept {
    if (!ready_ || (source == nullptr && size != 0U) ||
        !validRange(offset, size)) {
        return false;
    }
    for (std::size_t index = 0U; index < size; ++index) {
        pending_[offset + index] = source[index];
    }
    dirty_ = dirty_ || size != 0U;
    return true;
}

bool EspIdfNvsSettingsStorage::commit() noexcept {
    if (!ready_) {
        return false;
    }
    if (!dirty_) {
        return true;
    }
    if (nvs_set_blob(
            handle_,
            SettingsKey,
            pending_.data(),
            pending_.size()) != ESP_OK ||
        nvs_commit(handle_) != ESP_OK) {
        pending_ = committed_;
        dirty_ = false;
        return false;
    }

    committed_ = pending_;
    dirty_ = false;
    return true;
}

bool EspIdfNvsSettingsStorage::validRange(
    const std::size_t offset,
    const std::size_t size) noexcept {
    constexpr std::size_t StorageSize =
        JournaledUserSettingsStore::RequiredCapacity;
    return offset <= StorageSize && size <= StorageSize - offset;
}

bool EspIdfUsbSerialJtagSettingsTransport::begin() noexcept {
    if (ready_) {
        return true;
    }
    usb_serial_jtag_driver_config_t config{};
    config.rx_buffer_size = UsbReceiveBufferSize;
    config.tx_buffer_size = UsbTransmitBufferSize;
    ready_ = usb_serial_jtag_driver_install(&config) == ESP_OK;
    return ready_;
}

std::size_t EspIdfUsbSerialJtagSettingsTransport::read(
    std::uint8_t* const destination,
    const std::size_t capacity,
    const std::uint32_t timeoutMs) noexcept {
    if (!ready_ || (destination == nullptr && capacity != 0U)) {
        return 0U;
    }
    const int received = usb_serial_jtag_read_bytes(
        destination,
        capacity,
        pdMS_TO_TICKS(timeoutMs));
    return received > 0 ? static_cast<std::size_t>(received) : 0U;
}

bool EspIdfUsbSerialJtagSettingsTransport::send(
    const std::uint8_t* const data,
    const std::size_t size) noexcept {
    if (!ready_ || (data == nullptr && size != 0U)) {
        return false;
    }
    const int sent = usb_serial_jtag_write_bytes(
        data,
        size,
        pdMS_TO_TICKS(UsbWriteTimeoutMs));
    return sent >= 0 && static_cast<std::size_t>(sent) == size;
}

}  // namespace bmw::remote::infrastructure

#endif
