#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "bmw_remote/application/user_settings.hpp"

namespace bmw::remote::infrastructure {

inline constexpr std::size_t UserSettingsPayloadSize = 24U;
using UserSettingsPayload =
    std::array<std::uint8_t, UserSettingsPayloadSize>;

[[nodiscard]] bool encodeUserSettingsPayload(
    const application::UserSettings& settings,
    UserSettingsPayload& payload) noexcept;

[[nodiscard]] bool decodeUserSettingsPayload(
    const UserSettingsPayload& payload,
    application::UserSettings& settings) noexcept;

[[nodiscard]] bool userSettingsEqual(
    const application::UserSettings& left,
    const application::UserSettings& right) noexcept;

}  // namespace bmw::remote::infrastructure
