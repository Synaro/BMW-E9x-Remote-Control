#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "bmw_remote/application/user_settings.hpp"
#include "tools/serial_settings_channel.hpp"
#include "tools/settings_device_client.hpp"
#include "tools/user_settings_file.hpp"

namespace {

using bmw::remote::application::DriverEntryMode;
using bmw::remote::application::HoodMonitoringMode;
using bmw::remote::application::UserSettings;

enum class Mode : std::uint8_t {
    Interactive,
    Show,
    Check,
    WriteDefaults,
    PrintDefaults,
    ListDevices,
    ProbeDevice,
    ReadDevice,
    WriteDevice,
};

struct Options final {
    Mode mode{Mode::Interactive};
    std::string configPath{"config/user-settings.conf"};
    std::string devicePort{};
};

void printUsage(const char* const executable) {
    std::cout
        << "BMW E9x Remote Control - configurateur\n\n"
        << "Utilisation :\n"
        << "  " << executable << " [--config CHEMIN]\n"
        << "  " << executable << " --show [--config CHEMIN]\n"
        << "  " << executable << " --check [--config CHEMIN]\n"
        << "  " << executable << " --write-defaults [--config CHEMIN]\n"
        << "  " << executable << " --print-defaults\n"
        << "  " << executable << " --list-devices\n"
        << "  " << executable << " --probe-device PORT\n"
        << "  " << executable << " --read-device PORT\n"
        << "  " << executable
        << " --write-device PORT [--config CHEMIN]\n\n"
        << "Sans option, un assistant interactif modifie la configuration.\n";
}

bool selectMode(
    const Mode requested,
    Options& options,
    std::string& error) {
    if (options.mode != Mode::Interactive) {
        error = "une seule action peut etre demandee";
        return false;
    }
    options.mode = requested;
    return true;
}

bool parseArguments(
    const int argc,
    char* argv[],
    Options& options,
    bool& helpRequested,
    std::string& error) {
    helpRequested = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--help" || argument == "-h") {
            helpRequested = true;
            return true;
        }
        if (argument == "--config") {
            if (index + 1 >= argc) {
                error = "--config exige un chemin";
                return false;
            }
            options.configPath = argv[++index];
            if (options.configPath.empty()) {
                error = "le chemin de configuration est vide";
                return false;
            }
            continue;
        }
        if (argument == "--show") {
            if (!selectMode(Mode::Show, options, error)) {
                return false;
            }
            continue;
        }
        if (argument == "--check") {
            if (!selectMode(Mode::Check, options, error)) {
                return false;
            }
            continue;
        }
        if (argument == "--write-defaults") {
            if (!selectMode(Mode::WriteDefaults, options, error)) {
                return false;
            }
            continue;
        }
        if (argument == "--print-defaults") {
            if (!selectMode(Mode::PrintDefaults, options, error)) {
                return false;
            }
            continue;
        }
        if (argument == "--list-devices") {
            if (!selectMode(Mode::ListDevices, options, error)) {
                return false;
            }
            continue;
        }
        if (argument == "--probe-device" || argument == "--read-device" ||
            argument == "--write-device") {
            if (index + 1 >= argc) {
                error = std::string{argument} + " exige un port COM";
                return false;
            }
            Mode requested = Mode::ProbeDevice;
            if (argument == "--read-device") {
                requested = Mode::ReadDevice;
            } else if (argument == "--write-device") {
                requested = Mode::WriteDevice;
            }
            if (!selectMode(requested, options, error)) {
                return false;
            }
            options.devicePort = argv[++index];
            if (options.devicePort.empty()) {
                error = "le nom du port COM est vide";
                return false;
            }
            continue;
        }
        error = "option inconnue : " + std::string{argument};
        return false;
    }
    return true;
}

bool printSettings(const UserSettings& settings, std::string& error) {
    return bmw::remote::host::writeUserSettings(std::cout, settings, error);
}

bool readAnswer(const std::string& prompt, std::string& answer) {
    std::cout << prompt << std::flush;
    if (!std::getline(std::cin, answer)) {
        return false;
    }

    const auto notSpace = [](const unsigned char character) {
        return std::isspace(character) == 0;
    };
    answer.erase(
        answer.begin(),
        std::find_if(answer.begin(), answer.end(), notSpace));
    answer.erase(
        std::find_if(answer.rbegin(), answer.rend(), notSpace).base(),
        answer.end());
    std::transform(
        answer.begin(),
        answer.end(),
        answer.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return true;
}

bool parseUnsigned(const std::string_view text, std::uint32_t& value) noexcept {
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

bool promptBoolean(
    const std::string& label,
    bool& value) {
    for (;;) {
        std::string answer{};
        if (!readAnswer(
                label + " [" + (value ? "oui" : "non") + "] : ",
                answer)) {
            return false;
        }
        if (answer.empty()) {
            return true;
        }
        if (answer == "o" || answer == "oui" || answer == "y" ||
            answer == "yes" || answer == "true" || answer == "1") {
            value = true;
            return true;
        }
        if (answer == "n" || answer == "non" || answer == "no" ||
            answer == "false" || answer == "0") {
            value = false;
            return true;
        }
        std::cout << "  Reponse attendue : oui ou non.\n";
    }
}

bool promptNumber(
    const std::string& label,
    const std::uint32_t minimum,
    const std::uint32_t maximum,
    std::uint32_t& value) {
    for (;;) {
        std::string answer{};
        if (!readAnswer(
                label + " (" + std::to_string(minimum) + " a " +
                    std::to_string(maximum) + ") [" +
                    std::to_string(value) + "] : ",
                answer)) {
            return false;
        }
        if (answer.empty()) {
            return true;
        }
        std::uint32_t parsed = 0U;
        if (parseUnsigned(answer, parsed) &&
            parsed >= minimum && parsed <= maximum) {
            value = parsed;
            return true;
        }
        std::cout << "  Valeur hors limites ou invalide.\n";
    }
}

bool promptHoodMode(HoodMonitoringMode& mode) {
    for (;;) {
        const char* const current = mode == HoodMonitoringMode::Required
                                        ? "obligatoire"
                                        : "desactive";
        std::string answer{};
        if (!readAnswer(
                "Controle du capot (1=obligatoire, 2=desactive) [" +
                    std::string{current} + "] : ",
                answer)) {
            return false;
        }
        if (answer.empty()) {
            return true;
        }
        if (answer == "1" || answer == "obligatoire" || answer == "required") {
            mode = HoodMonitoringMode::Required;
            return true;
        }
        if (answer == "2" || answer == "desactive" || answer == "disabled" ||
            answer == "optional") {
            mode = HoodMonitoringMode::Disabled;
            return true;
        }
        std::cout << "  Choisir 1 ou 2.\n";
    }
}

bool promptDriverEntryMode(DriverEntryMode& mode) {
    for (;;) {
        const char* const current = mode == DriverEntryMode::RequireTakeover
                                        ? "reprise"
                                        : "arret immediat";
        std::string answer{};
        if (!readAnswer(
                "Ouverture d'une porte (1=reprise, 2=arret immediat) [" +
                    std::string{current} + "] : ",
                answer)) {
            return false;
        }
        if (answer.empty()) {
            return true;
        }
        if (answer == "1" || answer == "reprise" ||
            answer == "require_takeover") {
            mode = DriverEntryMode::RequireTakeover;
            return true;
        }
        if (answer == "2" || answer == "arret" ||
            answer == "stop_immediately") {
            mode = DriverEntryMode::StopImmediately;
            return true;
        }
        std::cout << "  Choisir 1 ou 2.\n";
    }
}

bool promptFeatureSelection(UserSettings& settings) {
    bool editFeatures = false;
    if (!promptBoolean(
            "Modifier les fonctionnalites modulaires",
            editFeatures)) {
        return false;
    }
    if (!editFeatures) {
        return true;
    }

    std::cout
        << "\nChaque option exprime une preference. Les fonctions non implementees "
        << "ou non qualifiees resteront bloquees.\n";
    const char* previousCategory = nullptr;
    for (const bmw::remote::application::FeatureDescriptor& feature :
         bmw::remote::application::featureCatalog()) {
        const char* const category =
            bmw::remote::application::toString(feature.category);
        if (previousCategory == nullptr ||
            std::string_view{previousCategory} != category) {
            std::cout << "\n[" << category << "]\n";
            previousCategory = category;
        }

        bool enabled = settings.features.enabled(feature.id);
        if (!promptBoolean(
                std::string{feature.displayName} + " (" + feature.code + ")",
                enabled) ||
            !settings.features.setEnabled(feature.id, enabled)) {
            return false;
        }
    }
    return true;
}

bool configureInteractively(UserSettings& settings) {
    constexpr std::uint32_t MillisecondsPerMinute{60'000U};
    constexpr std::uint32_t MillisecondsPerSecond{1'000U};

    std::cout
        << "\nAppuyer sur Entree conserve la valeur affichee.\n"
        << "Ctrl+Z puis Entree annule sans enregistrer.\n\n";

    if (!promptBoolean("Demarrage distant active", settings.remoteStartEnabled) ||
        !promptHoodMode(settings.hoodMonitoring)) {
        return false;
    }

    std::uint32_t remoteRunMinutes =
        settings.maximumRemoteRunTimeMs / MillisecondsPerMinute;
    if (!promptNumber("Duree maximale moteur (minutes)", 1U, 60U, remoteRunMinutes)) {
        return false;
    }
    settings.maximumRemoteRunTimeMs = remoteRunMinutes * MillisecondsPerMinute;

    if (!promptDriverEntryMode(settings.driverEntryMode)) {
        return false;
    }

    std::uint32_t takeoverSeconds =
        settings.driverTakeoverTimeoutMs / MillisecondsPerSecond;
    if (!promptNumber(
            "Delai de confirmation de reprise (secondes)",
            10U,
            300U,
            takeoverSeconds)) {
        return false;
    }
    settings.driverTakeoverTimeoutMs = takeoverSeconds * MillisecondsPerSecond;

    for (;;) {
        std::uint32_t pressCount = settings.lockPressCount;
        if (!promptNumber("Nombre d'appuis sur verrouillage", 2U, 5U, pressCount) ||
            !promptNumber(
                "Intervalle minimal entre appuis (ms)",
                50U,
                5'000U,
                settings.lockMinimumGapMs) ||
            !promptNumber(
                "Intervalle maximal entre appuis (ms)",
                50U,
                5'000U,
                settings.lockMaximumGapMs) ||
            !promptNumber(
                "Fenetre totale de la sequence (ms)",
                500U,
                15'000U,
                settings.lockMaximumSequenceMs)) {
            return false;
        }
        settings.lockPressCount = static_cast<std::uint8_t>(pressCount);

        if (bmw::remote::application::validateUserSettings(settings).valid()) {
            break;
        }
        std::cout
            << "\nLes temporisations de verrouillage sont incoherentes. "
            << "Merci de les corriger.\n\n";
    }

    return promptFeatureSelection(settings);
}

int runInteractive(const Options& options) {
    UserSettings settings{};
    std::error_code filesystemError{};
    const bool existing = std::filesystem::exists(options.configPath, filesystemError);
    if (filesystemError) {
        std::cerr << "Erreur : impossible d'inspecter le fichier : "
                  << filesystemError.message() << '\n';
        return 2;
    }

    if (existing) {
        std::string loadError{};
        if (!bmw::remote::host::loadUserSettingsFile(
                options.configPath.c_str(), settings, loadError)) {
            std::cerr
                << "Erreur : la configuration existante est invalide : "
                << loadError << "\nAucun fichier n'a ete modifie.\n";
            return 2;
        }
        std::cout << "Configuration chargee : " << options.configPath << '\n';
    } else {
        std::cout << "Nouveau fichier avec les valeurs sures par defaut : "
                  << options.configPath << '\n';
    }

    if (!configureInteractively(settings)) {
        std::cout << "\nConfiguration annulee ; aucun fichier n'a ete modifie.\n";
        return 0;
    }

    std::string saveError{};
    if (!bmw::remote::host::saveUserSettingsFile(
            options.configPath.c_str(), settings, saveError)) {
        std::cerr << "Erreur d'enregistrement : " << saveError << '\n';
        return 2;
    }

    std::cout
        << "\nConfiguration enregistree et verifiee : " << options.configPath
        << "\nTest conseille :\n"
        << "  .\\scripts\\simulate.ps1 -ConfigPath \""
        << options.configPath << "\"\n";
    return 0;
}

int runListDevices() {
    std::string error{};
    const std::vector<std::string> ports =
        bmw::remote::host::listSerialPorts(error);
    if (!error.empty()) {
        std::cerr << "Erreur : " << error << '\n';
        return 2;
    }
    if (ports.empty()) {
        std::cout << "Aucun port serie detecte.\n";
        return 0;
    }

    std::cout << "Ports serie detectes :\n";
    for (const std::string& port : ports) {
        std::cout << "  " << port << '\n';
    }
    return 0;
}

void printDeviceIdentity(
    const bmw::remote::infrastructure::SettingsDeviceIdentity identity) {
    using bmw::remote::infrastructure::SettingsDeviceCapability;
    using bmw::remote::infrastructure::hasCapability;

    std::cout
        << "Boitier BMW E9x Remote Control compatible :\n"
        << "  cible : "
        << bmw::remote::infrastructure::toString(identity.hardwareTarget)
        << "\n  firmware : "
        << static_cast<unsigned int>(identity.firmwareMajor) << '.'
        << static_cast<unsigned int>(identity.firmwareMinor) << '.'
        << static_cast<unsigned int>(identity.firmwarePatch)
        << "\n  lecture reglages : "
        << (hasCapability(identity, SettingsDeviceCapability::SettingsRead)
                ? "oui"
                : "non")
        << "\n  ecriture reglages : "
        << (hasCapability(identity, SettingsDeviceCapability::SettingsWrite)
                ? "oui"
                : "non")
        << "\n  persistance : "
        << (hasCapability(identity, SettingsDeviceCapability::PersistentSettings)
                ? "oui"
                : "non")
        << '\n';
}

int runProbeDevice(const Options& options) {
    constexpr std::uint32_t UsbSerialBaudRate = 115'200U;
    std::string error{};
    bmw::remote::host::SerialSettingsChannel channel{};
    if (!channel.open(options.devicePort, UsbSerialBaudRate, error)) {
        std::cerr << "Erreur : " << error << '\n';
        return 2;
    }

    bmw::remote::host::SettingsDeviceClient client{channel};
    bmw::remote::infrastructure::SettingsDeviceIdentity identity{};
    if (!client.probe(identity, error)) {
        std::cerr << "Erreur : " << error << '\n';
        return 2;
    }
    printDeviceIdentity(identity);
    std::cout << "device_probe_result: PASS\n";
    return 0;
}

int runReadDevice(const Options& options) {
    constexpr std::uint32_t UsbSerialBaudRate = 115'200U;
    std::string error{};
    bmw::remote::host::SerialSettingsChannel channel{};
    if (!channel.open(options.devicePort, UsbSerialBaudRate, error)) {
        std::cerr << "Erreur : " << error << '\n';
        return 2;
    }

    bmw::remote::host::SettingsDeviceClient client{channel};
    bmw::remote::infrastructure::SettingsDeviceIdentity identity{};
    if (!client.probe(identity, error)) {
        std::cerr << "Erreur : " << error << '\n';
        return 2;
    }
    if (!bmw::remote::infrastructure::hasCapability(
            identity,
            bmw::remote::infrastructure::SettingsDeviceCapability::SettingsRead)) {
        std::cerr << "Erreur : le boitier n'annonce pas la lecture des reglages\n";
        return 2;
    }
    printDeviceIdentity(identity);
    UserSettings settings{};
    if (!client.read(settings, error)) {
        std::cerr << "Erreur : " << error << '\n';
        return 2;
    }
    if (!printSettings(settings, error)) {
        std::cerr << "Erreur : " << error << '\n';
        return 2;
    }
    std::cout << "device_read_result: PASS\n";
    return 0;
}

int runWriteDevice(const Options& options) {
    UserSettings settings{};
    std::string error{};
    if (!bmw::remote::host::loadUserSettingsFile(
            options.configPath.c_str(), settings, error)) {
        std::cerr << "Erreur : " << error << '\n';
        return 2;
    }

    constexpr std::uint32_t UsbSerialBaudRate = 115'200U;
    bmw::remote::host::SerialSettingsChannel channel{};
    if (!channel.open(options.devicePort, UsbSerialBaudRate, error)) {
        std::cerr << "Erreur : " << error << '\n';
        return 2;
    }

    bmw::remote::host::SettingsDeviceClient client{channel};
    if (!client.writeAndVerify(settings, error)) {
        std::cerr << "Erreur : " << error << '\n';
        return 2;
    }
    std::cout
        << "Configuration enregistree et relue sur " << options.devicePort
        << ".\ndevice_write_result: PASS\n";
    return 0;
}

}  // namespace

int main(const int argc, char* argv[]) {
    Options options{};
    bool helpRequested = false;
    std::string error{};
    if (!parseArguments(argc, argv, options, helpRequested, error)) {
        std::cerr << "Erreur : " << error << "\n\n";
        printUsage(argv[0]);
        return 2;
    }
    if (helpRequested) {
        printUsage(argv[0]);
        return 0;
    }

    if (options.mode == Mode::ListDevices) {
        return runListDevices();
    }
    if (options.mode == Mode::ProbeDevice) {
        return runProbeDevice(options);
    }
    if (options.mode == Mode::ReadDevice) {
        return runReadDevice(options);
    }
    if (options.mode == Mode::WriteDevice) {
        return runWriteDevice(options);
    }

    if (options.mode == Mode::PrintDefaults) {
        if (!printSettings(UserSettings{}, error)) {
            std::cerr << "Erreur : " << error << '\n';
            return 2;
        }
        return 0;
    }

    if (options.mode == Mode::WriteDefaults) {
        if (!bmw::remote::host::saveUserSettingsFile(
                options.configPath.c_str(), UserSettings{}, error)) {
            std::cerr << "Erreur : " << error << '\n';
            return 2;
        }
        std::cout << "Configuration par defaut enregistree : "
                  << options.configPath << '\n';
        return 0;
    }

    if (options.mode == Mode::Show || options.mode == Mode::Check) {
        UserSettings settings{};
        if (!bmw::remote::host::loadUserSettingsFile(
                options.configPath.c_str(), settings, error)) {
            std::cerr << "Erreur : " << error << '\n';
            return 2;
        }
        if (!printSettings(settings, error)) {
            std::cerr << "Erreur : " << error << '\n';
            return 2;
        }
        if (options.mode == Mode::Check) {
            std::cout << "configuration_result: PASS\n";
        }
        return 0;
    }

    return runInteractive(options);
}
