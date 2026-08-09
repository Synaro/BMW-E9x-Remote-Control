#if defined(ESP_PLATFORM)

#include <array>
#include <cstddef>
#include <cstdint>

#include "bmw_remote/application/controller.hpp"
#include "bmw_remote/infrastructure/esp_idf_hal.hpp"
#include "bmw_remote/infrastructure/esp_idf_settings.hpp"
#include "bmw_remote/infrastructure/settings_storage.hpp"
#include "bmw_remote/infrastructure/settings_stream.hpp"

#include "esp_idf_version.h"

static_assert(
    ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(5, 5, 0),
    "The ESP32-S3 reference build requires the pinned ESP-IDF 5.5.0");

namespace {

using bmw::remote::application::ControllerState;
using bmw::remote::infrastructure::EspIdfMonotonicTimeHal;
using bmw::remote::infrastructure::EspIdfNvsSettingsStorage;
using bmw::remote::infrastructure::EspIdfUsbSerialJtagSettingsTransport;
using bmw::remote::infrastructure::JournaledUserSettingsStore;
using bmw::remote::infrastructure::SettingsHardwareTarget;
using bmw::remote::infrastructure::SettingsProtocolAccess;
using bmw::remote::infrastructure::SettingsProtocolEndpoint;
using bmw::remote::infrastructure::SettingsStreamConfig;
using bmw::remote::infrastructure::settingsPrototypeIdentity;

constexpr std::size_t MaximumBytesPerCycle = 64U;
constexpr std::uint32_t UsbReadTimeoutMs = 10U;
constexpr std::uint32_t IdleDelayMs = 1U;

EspIdfNvsSettingsStorage settingsStorage;
JournaledUserSettingsStore settingsStore{settingsStorage};
EspIdfUsbSerialJtagSettingsTransport settingsTransport;
EspIdfMonotonicTimeHal monotonicTime;
SettingsProtocolEndpoint settingsEndpoint{
    settingsStore,
    settingsTransport,
    SettingsStreamConfig{},
    settingsPrototypeIdentity(SettingsHardwareTarget::Esp32S3DevKitC1)};

[[nodiscard]] SettingsProtocolAccess localUsbAccess() noexcept {
    // There is no vehicle runtime, TWAI driver or actuator adapter in Phase 2.
    // Physical access to the bench USB connector remains the local boundary.
    return SettingsProtocolAccess{true, ControllerState::Idle};
}

void serviceUsbSettings() noexcept {
    std::array<std::uint8_t, MaximumBytesPerCycle> receivedBytes{};
    const std::size_t received = settingsTransport.read(
        receivedBytes.data(),
        receivedBytes.size(),
        UsbReadTimeoutMs);
    const SettingsProtocolAccess access = localUsbAccess();
    for (std::size_t index = 0U; index < received; ++index) {
        const auto result = settingsEndpoint.consume(
            receivedBytes[index],
            monotonicTime.nowMs(),
            access);
        (void)result;
    }

    const auto pollResult = settingsEndpoint.poll(monotonicTime.nowMs());
    (void)pollResult;
}

}  // namespace

extern "C" void app_main() {
    (void)settingsStorage.begin();
    const bool transportReady = settingsTransport.begin();

    while (true) {
        if (transportReady) {
            serviceUsbSettings();
        }
        monotonicTime.delayMs(IdleDelayMs);
    }
}

#else

int main() {
    // The native firmware target intentionally performs no physical action.
    // Behavioral validation is implemented in tests/test_main.cpp.
    return 0;
}

#endif
