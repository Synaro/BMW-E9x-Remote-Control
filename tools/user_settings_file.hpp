#pragma once

#include <iosfwd>
#include <string>

#include "bmw_remote/application/user_settings.hpp"

namespace bmw::remote::host {

[[nodiscard]] bool parseUserSettings(
    std::istream& input,
    application::UserSettings& settings,
    std::string& error);

[[nodiscard]] bool loadUserSettingsFile(
    const char* path,
    application::UserSettings& settings,
    std::string& error);

}  // namespace bmw::remote::host
