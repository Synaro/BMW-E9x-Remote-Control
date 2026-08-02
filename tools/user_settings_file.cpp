#include "tools/user_settings_file.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace bmw::remote::host {
namespace {

std::string_view trim(std::string_view text) noexcept {
    while (!text.empty() &&
           std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1U);
    }
    while (!text.empty() &&
           std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1U);
    }
    return text;
}

bool parseUnsigned(
    const std::string_view text,
    std::uint32_t& value) noexcept {
    if (text.empty()) {
        return false;
    }

    std::uint32_t parsed = 0U;
    const auto result = std::from_chars(
        text.data(),
        text.data() + text.size(),
        parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return false;
    }

    value = parsed;
    return true;
}

bool parseScaledDuration(
    const std::string_view text,
    const std::uint32_t scale,
    std::uint32_t& durationMs) noexcept {
    std::uint32_t value = 0U;
    if (!parseUnsigned(text, value) ||
        value > std::numeric_limits<std::uint32_t>::max() / scale) {
        return false;
    }
    durationMs = value * scale;
    return true;
}

bool parseBoolean(const std::string_view text, bool& value) noexcept {
    if (text == "true") {
        value = true;
        return true;
    }
    if (text == "false") {
        value = false;
        return true;
    }
    return false;
}

bool hasKey(
    const std::vector<std::string>& keys,
    const std::string_view key) {
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

std::string lineError(
    const std::size_t lineNumber,
    const std::string_view message) {
    return "line " + std::to_string(lineNumber) + ": " + std::string{message};
}

bool applySetting(
    const std::string_view key,
    const std::string_view value,
    application::UserSettings& settings) noexcept {
    if (key == "remote_start_enabled") {
        return parseBoolean(value, settings.remoteStartEnabled);
    }

    if (key == "hood_monitoring") {
        if (value == "required") {
            settings.hoodMonitoring = application::HoodMonitoringMode::Required;
            return true;
        }
        if (value == "disabled" || value == "optional") {
            settings.hoodMonitoring = application::HoodMonitoringMode::Disabled;
            return true;
        }
        return false;
    }

    if (key == "driver_entry_mode") {
        if (value == "require_takeover" || value == "require-takeover") {
            settings.driverEntryMode = application::DriverEntryMode::RequireTakeover;
            return true;
        }
        if (value == "stop_immediately" || value == "stop-immediately") {
            settings.driverEntryMode = application::DriverEntryMode::StopImmediately;
            return true;
        }
        return false;
    }

    if (key == "remote_run_minutes") {
        return parseScaledDuration(value, 60'000U, settings.maximumRemoteRunTimeMs);
    }

    if (key == "takeover_timeout_seconds") {
        return parseScaledDuration(value, 1'000U, settings.driverTakeoverTimeoutMs);
    }

    std::uint32_t parsed = 0U;
    if (!parseUnsigned(value, parsed)) {
        return false;
    }

    if (key == "lock_press_count") {
        if (parsed > std::numeric_limits<std::uint8_t>::max()) {
            return false;
        }
        settings.lockPressCount = static_cast<std::uint8_t>(parsed);
        return true;
    }
    if (key == "lock_minimum_gap_ms") {
        settings.lockMinimumGapMs = parsed;
        return true;
    }
    if (key == "lock_maximum_gap_ms") {
        settings.lockMaximumGapMs = parsed;
        return true;
    }
    if (key == "lock_sequence_window_ms") {
        settings.lockMaximumSequenceMs = parsed;
        return true;
    }
    return false;
}

bool knownKey(const std::string_view key) noexcept {
    return key == "remote_start_enabled" ||
           key == "hood_monitoring" ||
           key == "driver_entry_mode" ||
           key == "remote_run_minutes" ||
           key == "takeover_timeout_seconds" ||
           key == "lock_press_count" ||
           key == "lock_minimum_gap_ms" ||
           key == "lock_maximum_gap_ms" ||
           key == "lock_sequence_window_ms";
}

bool sameSettings(
    const application::UserSettings& left,
    const application::UserSettings& right) noexcept {
    return left.remoteStartEnabled == right.remoteStartEnabled &&
           left.hoodMonitoring == right.hoodMonitoring &&
           left.driverEntryMode == right.driverEntryMode &&
           left.maximumRemoteRunTimeMs == right.maximumRemoteRunTimeMs &&
           left.driverTakeoverTimeoutMs == right.driverTakeoverTimeoutMs &&
           left.lockPressCount == right.lockPressCount &&
           left.lockMinimumGapMs == right.lockMinimumGapMs &&
           left.lockMaximumGapMs == right.lockMaximumGapMs &&
           left.lockMaximumSequenceMs == right.lockMaximumSequenceMs;
}

void removeIfPresent(const std::filesystem::path& path) noexcept {
    std::error_code ignored{};
    static_cast<void>(std::filesystem::remove(path, ignored));
}

}  // namespace

bool parseUserSettings(
    std::istream& input,
    application::UserSettings& settings,
    std::string& error) {
    application::UserSettings parsed{};
    std::vector<std::string> keys{};
    std::string line{};
    std::size_t lineNumber = 0U;

    while (std::getline(input, line)) {
        ++lineNumber;
        std::string_view content{line};
        const std::size_t comment = content.find('#');
        if (comment != std::string_view::npos) {
            content = content.substr(0U, comment);
        }
        content = trim(content);
        if (content.empty()) {
            continue;
        }

        const std::size_t separator = content.find('=');
        if (separator == std::string_view::npos ||
            content.find('=', separator + 1U) != std::string_view::npos) {
            error = lineError(lineNumber, "expected key=value");
            return false;
        }

        const std::string_view key = trim(content.substr(0U, separator));
        const std::string_view value = trim(content.substr(separator + 1U));
        if (key.empty() || value.empty()) {
            error = lineError(lineNumber, "key and value must not be empty");
            return false;
        }
        if (!knownKey(key)) {
            error = lineError(lineNumber, "unknown setting '" + std::string{key} + "'");
            return false;
        }
        if (hasKey(keys, key)) {
            error = lineError(lineNumber, "duplicate setting '" + std::string{key} + "'");
            return false;
        }
        if (!applySetting(key, value, parsed)) {
            error = lineError(lineNumber, "invalid value for '" + std::string{key} + "'");
            return false;
        }
        keys.emplace_back(key);
    }

    if (!input.eof()) {
        error = "unable to read configuration";
        return false;
    }

    const application::UserSettingsValidation validation =
        application::validateUserSettings(parsed);
    if (!validation.valid()) {
        error = "configuration rejected by safety bounds, reasons_mask=" +
                std::to_string(validation.reasons);
        return false;
    }

    settings = parsed;
    error.clear();
    return true;
}

bool loadUserSettingsFile(
    const char* const path,
    application::UserSettings& settings,
    std::string& error) {
    if (path == nullptr || path[0] == '\0') {
        error = "configuration path is empty";
        return false;
    }

    std::ifstream input{path};
    if (!input) {
        error = "unable to open configuration file";
        return false;
    }
    return parseUserSettings(input, settings, error);
}

bool writeUserSettings(
    std::ostream& output,
    const application::UserSettings& settings,
    std::string& error) {
    const application::UserSettingsValidation validation =
        application::validateUserSettings(settings);
    if (!validation.valid()) {
        error = "configuration rejected by safety bounds, reasons_mask=" +
                std::to_string(validation.reasons);
        return false;
    }

    constexpr std::uint32_t MillisecondsPerMinute{60'000U};
    constexpr std::uint32_t MillisecondsPerSecond{1'000U};
    if (settings.maximumRemoteRunTimeMs % MillisecondsPerMinute != 0U) {
        error = "remote run time cannot be represented as whole minutes";
        return false;
    }
    if (settings.driverTakeoverTimeoutMs % MillisecondsPerSecond != 0U) {
        error = "takeover timeout cannot be represented as whole seconds";
        return false;
    }

    output << "# BMW E9x Remote Control - configuration utilisateur\n"
           << "# Generee par le configurateur ; les valeurs invalides sont refusees.\n\n"
           << "remote_start_enabled="
           << (settings.remoteStartEnabled ? "true" : "false") << "\n"
           << "hood_monitoring=" << application::toString(settings.hoodMonitoring)
           << "\n"
           << "remote_run_minutes="
           << settings.maximumRemoteRunTimeMs / MillisecondsPerMinute << "\n"
           << "driver_entry_mode=" << application::toString(settings.driverEntryMode)
           << "\n"
           << "takeover_timeout_seconds="
           << settings.driverTakeoverTimeoutMs / MillisecondsPerSecond << "\n"
           << "lock_press_count=" << static_cast<unsigned int>(settings.lockPressCount)
           << "\n"
           << "lock_minimum_gap_ms=" << settings.lockMinimumGapMs << "\n"
           << "lock_maximum_gap_ms=" << settings.lockMaximumGapMs << "\n"
           << "lock_sequence_window_ms=" << settings.lockMaximumSequenceMs << "\n";

    if (!output) {
        error = "unable to write configuration";
        return false;
    }

    error.clear();
    return true;
}

bool saveUserSettingsFile(
    const char* const path,
    const application::UserSettings& settings,
    std::string& error) {
    if (path == nullptr || path[0] == '\0') {
        error = "configuration path is empty";
        return false;
    }

    std::ostringstream serialized{};
    if (!writeUserSettings(serialized, settings, error)) {
        return false;
    }

    const std::filesystem::path target{path};
    std::filesystem::path temporary{target};
    temporary += ".tmp";
    std::filesystem::path backup{target};
    backup += ".bak";

    {
        std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
        if (!output) {
            error = "unable to create temporary configuration file";
            return false;
        }
        output << serialized.str();
        output.flush();
        if (!output) {
            removeIfPresent(temporary);
            error = "unable to persist temporary configuration file";
            return false;
        }
    }

    application::UserSettings verified{};
    std::string verificationError{};
    const std::string temporaryPath = temporary.string();
    if (!loadUserSettingsFile(temporaryPath.c_str(), verified, verificationError) ||
        !sameSettings(settings, verified)) {
        removeIfPresent(temporary);
        error = "temporary configuration verification failed";
        if (!verificationError.empty()) {
            error += ": " + verificationError;
        }
        return false;
    }

    std::error_code filesystemError{};
    const bool hadExistingTarget = std::filesystem::exists(target, filesystemError);
    if (filesystemError) {
        removeIfPresent(temporary);
        error = "unable to inspect destination: " + filesystemError.message();
        return false;
    }

    if (hadExistingTarget) {
        removeIfPresent(backup);
        std::filesystem::rename(target, backup, filesystemError);
        if (filesystemError) {
            removeIfPresent(temporary);
            error = "unable to preserve previous configuration: " +
                    filesystemError.message();
            return false;
        }
    }

    filesystemError.clear();
    std::filesystem::rename(temporary, target, filesystemError);
    if (filesystemError) {
        if (hadExistingTarget) {
            std::error_code restoreError{};
            std::filesystem::rename(backup, target, restoreError);
        }
        removeIfPresent(temporary);
        error = "unable to install new configuration: " +
                filesystemError.message();
        return false;
    }

    if (hadExistingTarget) {
        removeIfPresent(backup);
    }
    error.clear();
    return true;
}

}  // namespace bmw::remote::host
