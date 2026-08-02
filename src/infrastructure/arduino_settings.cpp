#if defined(ARDUINO)

#include "bmw_remote/infrastructure/arduino_settings.hpp"

#include <EEPROM.h>

namespace bmw::remote::infrastructure {

bool ArduinoEepromSettingsStorage::begin() noexcept {
    if (!ready_) {
        ready_ = EEPROM.begin(JournaledUserSettingsStore::RequiredCapacity);
    }
    return ready_;
}

std::size_t ArduinoEepromSettingsStorage::capacity() const noexcept {
    return JournaledUserSettingsStore::RequiredCapacity;
}

bool ArduinoEepromSettingsStorage::read(
    const std::size_t offset,
    std::uint8_t* const destination,
    const std::size_t size) noexcept {
    if (!ready_ || (destination == nullptr && size != 0U) ||
        !validRange(offset, size)) {
        return false;
    }

    for (std::size_t index = 0U; index < size; ++index) {
        destination[index] = EEPROM.read(static_cast<int>(offset + index));
    }
    return true;
}

bool ArduinoEepromSettingsStorage::write(
    const std::size_t offset,
    const std::uint8_t* const source,
    const std::size_t size) noexcept {
    if (!ready_ || (source == nullptr && size != 0U) ||
        !validRange(offset, size)) {
        return false;
    }

    for (std::size_t index = 0U; index < size; ++index) {
        EEPROM.write(
            static_cast<int>(offset + index),
            source[index]);
    }
    return true;
}

bool ArduinoEepromSettingsStorage::commit() noexcept {
    return ready_ && EEPROM.commit();
}

bool ArduinoEepromSettingsStorage::validRange(
    const std::size_t offset,
    const std::size_t size) noexcept {
    constexpr std::size_t StorageSize =
        JournaledUserSettingsStore::RequiredCapacity;
    return offset <= StorageSize && size <= StorageSize - offset;
}

bool ArduinoStreamSettingsTransport::send(
    const std::uint8_t* const data,
    const std::size_t size) noexcept {
    if (data == nullptr && size != 0U) {
        return false;
    }
    return stream_.write(data, size) == size;
}

}  // namespace bmw::remote::infrastructure

#endif
