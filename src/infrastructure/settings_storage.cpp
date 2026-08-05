#include "bmw_remote/infrastructure/settings_storage.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

#include "bmw_remote/application/user_settings.hpp"
#include "bmw_remote/infrastructure/settings_payload.hpp"

namespace bmw::remote::infrastructure {
namespace {

constexpr std::array<std::uint8_t, 4U> Magic = {'B', 'M', 'R', 'C'};
constexpr std::size_t VersionOffset = 4U;
constexpr std::size_t PayloadSizeOffset = 6U;
constexpr std::size_t GenerationOffset = 8U;
constexpr std::size_t PayloadOffset = 12U;
constexpr std::size_t PayloadSize = UserSettingsPayloadSize;
constexpr std::size_t CrcOffset = PayloadOffset + PayloadSize;
constexpr std::uint16_t LegacySchemaVersion = 1U;

struct DecodedRecord final {
    application::UserSettings settings{};
    std::uint32_t generation{0U};
    bool valid{false};
};

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

[[nodiscard]] bool encodeRecord(
    const application::UserSettings& settings,
    const std::uint32_t generation,
    std::array<std::uint8_t, JournaledUserSettingsStore::RecordSize>& record) noexcept {
    record.fill(0xFFU);
    for (std::size_t index = 0U; index < Magic.size(); ++index) {
        record[index] = Magic[index];
    }
    writeU16(record.data() + VersionOffset, JournaledUserSettingsStore::SchemaVersion);
    writeU16(record.data() + PayloadSizeOffset, static_cast<std::uint16_t>(PayloadSize));
    writeU32(record.data() + GenerationOffset, generation);

    UserSettingsPayload payload{};
    if (!encodeUserSettingsPayload(settings, payload)) {
        return false;
    }
    for (std::size_t index = 0U; index < payload.size(); ++index) {
        record[PayloadOffset + index] = payload[index];
    }
    writeU32(record.data() + CrcOffset, crc32(record.data(), CrcOffset));
    return true;
}

[[nodiscard]] DecodedRecord decodeRecord(
    const std::array<std::uint8_t, JournaledUserSettingsStore::RecordSize>& record) noexcept {
    DecodedRecord decoded{};
    for (std::size_t index = 0U; index < Magic.size(); ++index) {
        if (record[index] != Magic[index]) {
            return decoded;
        }
    }

    const std::uint16_t version = readU16(record.data() + VersionOffset);
    const std::size_t storedPayloadSize =
        readU16(record.data() + PayloadSizeOffset);
    const bool supportedVersion =
        (version == LegacySchemaVersion &&
         storedPayloadSize == LegacyUserSettingsPayloadSize) ||
        (version == JournaledUserSettingsStore::SchemaVersion &&
         storedPayloadSize == UserSettingsPayloadSize);
    const std::size_t storedCrcOffset = PayloadOffset + storedPayloadSize;
    if (!supportedVersion ||
        storedCrcOffset + sizeof(std::uint32_t) > record.size() ||
        readU32(record.data() + storedCrcOffset) !=
            crc32(record.data(), storedCrcOffset)) {
        return decoded;
    }

    UserSettingsPayload payload{};
    for (std::size_t index = 0U; index < storedPayloadSize; ++index) {
        payload[index] = record[PayloadOffset + index];
    }
    if (!decodeUserSettingsPayload(
            payload, storedPayloadSize, decoded.settings)) {
        return DecodedRecord{};
    }

    decoded.generation = readU32(record.data() + GenerationOffset);
    decoded.valid = true;
    return decoded;
}

[[nodiscard]] bool generationIsNewer(
    const std::uint32_t candidate,
    const std::uint32_t reference) noexcept {
    return static_cast<std::int32_t>(candidate - reference) > 0;
}

[[nodiscard]] bool readRecord(
    SettingsByteStorage& storage,
    const std::size_t slot,
    DecodedRecord& decoded) noexcept {
    std::array<std::uint8_t, JournaledUserSettingsStore::RecordSize> record{};
    if (!storage.read(
            slot * JournaledUserSettingsStore::SlotSize,
            record.data(),
            record.size())) {
        decoded = DecodedRecord{};
        return false;
    }
    decoded = decodeRecord(record);
    return true;
}

}  // namespace

bool JournaledUserSettingsStore::load(
    application::UserSettings& settings) noexcept {
    if (storage_.capacity() < RequiredCapacity) {
        return false;
    }

    DecodedRecord first{};
    DecodedRecord second{};
    const bool firstReadable = readRecord(storage_, 0U, first);
    const bool secondReadable = readRecord(storage_, 1U, second);
    if ((!firstReadable || !first.valid) && (!secondReadable || !second.valid)) {
        return false;
    }

    const DecodedRecord& selected =
        !first.valid
            ? second
            : (!second.valid || !generationIsNewer(second.generation, first.generation)
                   ? first
                   : second);
    settings = selected.settings;
    return true;
}

bool JournaledUserSettingsStore::save(
    const application::UserSettings& settings) noexcept {
    if (storage_.capacity() < RequiredCapacity ||
        !application::validateUserSettings(settings).valid()) {
        return false;
    }

    DecodedRecord first{};
    DecodedRecord second{};
    if (!readRecord(storage_, 0U, first) || !readRecord(storage_, 1U, second)) {
        return false;
    }

    std::size_t targetSlot = 0U;
    std::uint32_t nextGeneration = 1U;
    if (first.valid && second.valid) {
        const bool secondIsNewer = generationIsNewer(second.generation, first.generation);
        targetSlot = secondIsNewer ? 0U : 1U;
        nextGeneration = (secondIsNewer ? second.generation : first.generation) + 1U;
    } else if (first.valid) {
        targetSlot = 1U;
        nextGeneration = first.generation + 1U;
    } else if (second.valid) {
        targetSlot = 0U;
        nextGeneration = second.generation + 1U;
    }

    std::array<std::uint8_t, RecordSize> record{};
    if (!encodeRecord(settings, nextGeneration, record)) {
        return false;
    }
    if (!storage_.write(targetSlot * SlotSize, record.data(), record.size()) ||
        !storage_.commit()) {
        return false;
    }

    DecodedRecord verified{};
    return readRecord(storage_, targetSlot, verified) &&
           verified.valid &&
           verified.generation == nextGeneration &&
           userSettingsEqual(verified.settings, settings);
}

bool loadUserSettingsFailSafe(
    UserSettingsStore& store,
    application::UserSettings& settings) noexcept {
    application::UserSettings candidate{};
    candidate.remoteStartEnabled = false;
    if (!store.load(candidate) ||
        !application::validateUserSettings(candidate).valid()) {
        settings = application::UserSettings{};
        settings.remoteStartEnabled = false;
        return false;
    }

    settings = candidate;
    return true;
}

}  // namespace bmw::remote::infrastructure
