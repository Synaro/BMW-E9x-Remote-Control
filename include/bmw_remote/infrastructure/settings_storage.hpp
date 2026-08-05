#pragma once

#include <cstddef>
#include <cstdint>

#include "bmw_remote/infrastructure/ports.hpp"

namespace bmw::remote::infrastructure {

class SettingsByteStorage {
public:
    virtual ~SettingsByteStorage() = default;

    [[nodiscard]] virtual std::size_t capacity() const noexcept = 0;
    virtual bool read(
        std::size_t offset,
        std::uint8_t* destination,
        std::size_t size) noexcept = 0;
    virtual bool write(
        std::size_t offset,
        const std::uint8_t* source,
        std::size_t size) noexcept = 0;
    virtual bool commit() noexcept = 0;
};

class JournaledUserSettingsStore final : public UserSettingsStore {
public:
    static constexpr std::uint16_t SchemaVersion = 2U;
    static constexpr std::size_t SlotSize = 64U;
    static constexpr std::size_t SlotCount = 2U;
    static constexpr std::size_t RequiredCapacity = SlotSize * SlotCount;
    static constexpr std::size_t RecordSize = 48U;

    explicit JournaledUserSettingsStore(
        SettingsByteStorage& storage) noexcept
        : storage_(storage) {}

    bool load(application::UserSettings& settings) noexcept override;
    bool save(const application::UserSettings& settings) noexcept override;

private:
    SettingsByteStorage& storage_;
};

[[nodiscard]] bool loadUserSettingsFailSafe(
    UserSettingsStore& store,
    application::UserSettings& settings) noexcept;

}  // namespace bmw::remote::infrastructure
