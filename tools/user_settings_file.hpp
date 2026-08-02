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

[[nodiscard]] bool writeUserSettings(
    std::ostream& output,
    const application::UserSettings& settings,
    std::string& error);

[[nodiscard]] bool saveUserSettingsFile(
    const char* path,
    const application::UserSettings& settings,
    std::string& error);

}  // namespace bmw::remote::host
