#if defined(ARDUINO)

#include <Arduino.h>

#include <cstddef>
#include <cstdint>

#include "bmw_remote/application/controller.hpp"
#include "bmw_remote/infrastructure/arduino_settings.hpp"
#include "bmw_remote/infrastructure/settings_storage.hpp"
#include "bmw_remote/infrastructure/settings_stream.hpp"

namespace {

using bmw::remote::application::ControllerState;
using bmw::remote::infrastructure::ArduinoEepromSettingsStorage;
using bmw::remote::infrastructure::ArduinoStreamSettingsTransport;
using bmw::remote::infrastructure::JournaledUserSettingsStore;
using bmw::remote::infrastructure::SettingsProtocolAccess;
using bmw::remote::infrastructure::SettingsProtocolEndpoint;

constexpr std::size_t MaximumBytesPerLoop = 64U;

ArduinoEepromSettingsStorage settingsStorage;
JournaledUserSettingsStore settingsStore{settingsStorage};
ArduinoStreamSettingsTransport settingsTransport{Serial};
SettingsProtocolEndpoint settingsEndpoint{settingsStore, settingsTransport};

[[nodiscard]] SettingsProtocolAccess localUsbAccess() noexcept {
    // This firmware contains no vehicle runtime or actuator adapter. Its state
    // is therefore permanently Idle, and physical access to the dedicated USB
    // connector is the local authorization boundary for this bench target.
    return SettingsProtocolAccess{true, ControllerState::Idle};
}

void serviceUsbSettings() noexcept {
    const SettingsProtocolAccess access = localUsbAccess();
    std::size_t consumed = 0U;
    while (Serial.available() > 0 && consumed < MaximumBytesPerLoop) {
        const int value = Serial.read();
        if (value < 0) {
            break;
        }
        const auto result = settingsEndpoint.consume(
            static_cast<std::uint8_t>(value),
            static_cast<std::uint32_t>(millis()),
            access);
        (void)result;
        ++consumed;
    }

    const auto pollResult =
        settingsEndpoint.poll(static_cast<std::uint32_t>(millis()));
    (void)pollResult;
}

}  // namespace

void setup() {
    Serial.begin(115200);
    (void)settingsStorage.begin();
}

void loop() {
    serviceUsbSettings();
    delay(1U);
}

#else

int main() {
    // The native firmware target intentionally performs no physical action.
    // Behavioral validation is implemented in tests/test_main.cpp.
    return 0;
}

#endif
