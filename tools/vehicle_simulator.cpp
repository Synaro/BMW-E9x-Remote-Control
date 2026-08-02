#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "bmw_remote/application/controller.hpp"
#include "bmw_remote/application/lock_sequence_detector.hpp"
#include "bmw_remote/application/safety_policy.hpp"
#include "bmw_remote/application/user_settings.hpp"
#include "bmw_remote/infrastructure/replay_vehicle_gateway.hpp"
#include "bmw_remote/infrastructure/runtime.hpp"
#include "bmw_remote/infrastructure/settings_storage.hpp"
#include "bmw_remote/simulation/synthetic_can.hpp"
#include "tools/can_trace_csv.hpp"
#include "tools/user_settings_file.hpp"

namespace {

using namespace bmw::remote;

enum class Scenario : std::uint8_t {
    Nominal,
    HoodRequired,
    HoodOptional,
    TakeoverTimeout,
    TakeoverConfirmed,
    UserConfig,
    SettingsRecovery,
};

class ConsoleActuators final : public infrastructure::ActuatorPort {
public:
    bool enableIgnition() noexcept override {
        std::cout << "  actuator: enable ignition\n";
        return true;
    }

    bool engageStarter() noexcept override {
        std::cout << "  actuator: engage starter\n";
        return true;
    }

    bool disengageStarter() noexcept override {
        std::cout << "  actuator: disengage starter\n";
        return true;
    }

    bool releaseRemoteControl() noexcept override {
        std::cout << "  actuator: release control to authenticated driver\n";
        return true;
    }

    bool secureOutputs() noexcept override {
        std::cout << "  actuator: secure all outputs\n";
        return true;
    }
};

class ConsoleTimer final : public infrastructure::TimerPort {
public:
    bool arm(const std::uint32_t durationMs) noexcept override {
        std::cout << "  timer: arm " << durationMs << " ms\n";
        return true;
    }

    bool cancel() noexcept override {
        std::cout << "  timer: cancel\n";
        return true;
    }
};

class ConsoleNotifications final : public infrastructure::NotificationSink {
public:
    void publish(
        const application::ActionType notification,
        const application::ControllerState state,
        const application::FaultCode fault,
        const application::SafetyAssessment safety,
        const application::ProfileReadinessAssessment profileReadiness) noexcept override {
        std::cout << "  notification: " << application::toString(notification)
                  << " state=" << application::toString(state)
                  << " fault=" << application::toString(fault)
                  << " safety_mask=" << safety.reasons
                  << " profile_mask=" << static_cast<unsigned>(profileReadiness.reasons)
                  << '\n';
    }
};

void printDecision(
    const char* const label,
    const application::Decision& decision) {
    std::cout << label << ": "
              << application::toString(decision.previousState) << " -> "
              << application::toString(decision.state) << '\n';
}

const char* qualityName(const domain::SignalQuality quality) noexcept {
    switch (quality) {
        case domain::SignalQuality::Unavailable: return "unavailable";
        case domain::SignalQuality::Stale: return "stale";
        case domain::SignalQuality::Fresh: return "fresh";
    }
    return "unknown";
}

const char* scenarioName(const Scenario scenario) noexcept {
    switch (scenario) {
        case Scenario::Nominal: return "nominal";
        case Scenario::HoodRequired: return "hood-required";
        case Scenario::HoodOptional: return "hood-optional";
        case Scenario::TakeoverTimeout: return "takeover-timeout";
        case Scenario::TakeoverConfirmed: return "takeover-confirmed";
        case Scenario::UserConfig: return "user-config";
        case Scenario::SettingsRecovery: return "settings-recovery";
    }
    return "unknown";
}

bool parseScenario(const std::string_view text, Scenario& scenario) noexcept {
    if (text == "nominal") {
        scenario = Scenario::Nominal;
        return true;
    }
    if (text == "hood-required") {
        scenario = Scenario::HoodRequired;
        return true;
    }
    if (text == "hood-optional") {
        scenario = Scenario::HoodOptional;
        return true;
    }
    if (text == "takeover-timeout") {
        scenario = Scenario::TakeoverTimeout;
        return true;
    }
    if (text == "takeover-confirmed") {
        scenario = Scenario::TakeoverConfirmed;
        return true;
    }
    if (text == "user-config") {
        scenario = Scenario::UserConfig;
        return true;
    }
    if (text == "settings-recovery") {
        scenario = Scenario::SettingsRecovery;
        return true;
    }
    return false;
}

bool parseHoodRequirement(
    const std::string_view text,
    bool& requireHoodClosed) noexcept {
    if (text == "required") {
        requireHoodClosed = true;
        return true;
    }
    if (text == "optional") {
        requireHoodClosed = false;
        return true;
    }
    return false;
}

void printScenarioList() {
    std::cout
        << "nominal       remote start, running confirmation, remote stop\n"
        << "hood-required hood opens while running and causes the expected fault\n"
        << "hood-optional hood opens while running and is explicitly ignored\n"
        << "takeover-timeout door opens but takeover is not confirmed, then stop\n"
        << "takeover-confirmed door opens and authenticated takeover succeeds\n"
        << "user-config uses every value from a supplied configuration file\n"
        << "settings-recovery corrupts the newest slot and restores the previous one\n";
}

void printUsage(const char* const executable) {
    std::cerr
        << "Usage:\n"
        << "  " << executable << " --scenario <name>\n"
        << "  " << executable
        << " --scenario user-config --config <user-settings.conf>\n"
        << "  " << executable << " --show-config <user-settings.conf>\n"
        << "  " << executable
        << " --trace <trace.cantrace.csv> [--hood required|optional]\n"
        << "  " << executable << " --list-scenarios\n"
        << "  " << executable << " --help\n"
        << "  " << executable << " <trace.cantrace.csv>  (legacy form)\n";
}

void printUserSettings(const application::UserSettings& settings) {
    std::cout
        << "remote_start_enabled: "
        << (settings.remoteStartEnabled ? "true" : "false") << '\n'
        << "hood_monitoring: " << application::toString(settings.hoodMonitoring) << '\n'
        << "remote_run_ms: " << settings.maximumRemoteRunTimeMs << '\n'
        << "driver_entry_mode: "
        << application::toString(settings.driverEntryMode) << '\n'
        << "takeover_timeout_ms: " << settings.driverTakeoverTimeoutMs << '\n'
        << "lock_press_count: " << static_cast<unsigned>(settings.lockPressCount) << '\n'
        << "lock_minimum_gap_ms: " << settings.lockMinimumGapMs << '\n'
        << "lock_maximum_gap_ms: " << settings.lockMaximumGapMs << '\n'
        << "lock_sequence_window_ms: " << settings.lockMaximumSequenceMs << '\n';
}

bool loadUserSettings(
    const char* const path,
    application::UserSettings& settings) {
    std::string error{};
    if (!host::loadUserSettingsFile(path, settings, error)) {
        std::cerr << "Unable to load user configuration: " << error << '\n';
        return false;
    }
    return true;
}

int inspectExternalTrace(
    const char* const path,
    const bool requireHoodClosed) {
    constexpr std::size_t MaximumFrames = 1'000'000U;

    std::vector<infrastructure::CanFrame> trace{};
    std::string error{};
    if (!host::loadCanonicalCanTrace(path, trace, MaximumFrames, error)) {
        std::cerr << "Unable to load trace: " << error << '\n';
        return 1;
    }

    simulation::SyntheticCanDecoder decoder{};
    infrastructure::ReplayVehicleGateway gateway{
        trace.data(), trace.size(), decoder};
    if (!gateway.setElapsedTime(trace.back().timestampMs) || !gateway.requestState()) {
        std::cerr << "Replay rejected the trace\n";
        return 2;
    }

    const infrastructure::AssemblyStatistics stats = gateway.statistics();
    const domain::VehicleState vehicle = gateway.state();
    application::SafetyPolicyConfig policyConfig{};
    policyConfig.requireHoodClosed = requireHoodClosed;
    const application::SafetyAssessment safety =
        application::SafetyPolicy{policyConfig}.assessStart(vehicle);

    std::cout << "trace: " << trace.size() << " classic CAN frame(s), duration "
              << trace.back().timestampMs << " ms\n"
              << "hood_requirement: "
              << (requireHoodClosed ? "required" : "optional") << '\n'
              << "decoder: " << stats.decodedFrames << " decoded, "
              << stats.ignoredFrames << " ignored, " << stats.rejectedFrames
              << " rejected\n"
              << "engine_rpm_quality: " << qualityName(vehicle.engineRpm.quality) << '\n'
              << "hood_closed_quality: " << qualityName(vehicle.hoodClosed.quality) << '\n'
              << "remote_start_safety: "
              << (safety.approved() ? "approved" : "denied")
              << " reasons_mask=" << safety.reasons << '\n';

    if (stats.decodedFrames == 0U) {
        std::cout << "note: this build only decodes the documented synthetic protocol; "
                     "BMW frames stay ignored until a read-only decoder is qualified\n";
    }
    return 0;
}

class SimulatorSettingsStorage final
    : public infrastructure::SettingsByteStorage {
public:
    SimulatorSettingsStorage() {
        bytes_.fill(0xFFU);
    }

    [[nodiscard]] std::size_t capacity() const noexcept override {
        return bytes_.size();
    }

    bool read(
        const std::size_t offset,
        std::uint8_t* const destination,
        const std::size_t size) noexcept override {
        if (destination == nullptr || offset > bytes_.size() ||
            size > bytes_.size() - offset) {
            return false;
        }
        for (std::size_t index = 0U; index < size; ++index) {
            destination[index] = bytes_[offset + index];
        }
        return true;
    }

    bool write(
        const std::size_t offset,
        const std::uint8_t* const source,
        const std::size_t size) noexcept override {
        if (source == nullptr || offset > bytes_.size() ||
            size > bytes_.size() - offset) {
            return false;
        }
        for (std::size_t index = 0U; index < size; ++index) {
            bytes_[offset + index] = source[index];
        }
        return true;
    }

    bool commit() noexcept override {
        return true;
    }

    void corruptNewestSlot() noexcept {
        bytes_[infrastructure::JournaledUserSettingsStore::SlotSize + 15U] ^=
            0x01U;
    }

private:
    std::array<
        std::uint8_t,
        infrastructure::JournaledUserSettingsStore::RequiredCapacity> bytes_{};
};

int runSettingsRecoveryScenario() {
    SimulatorSettingsStorage storage{};
    infrastructure::JournaledUserSettingsStore store{storage};
    application::UserSettings previous{};
    previous.maximumRemoteRunTimeMs = 10U * 60U * 1'000U;
    application::UserSettings newest = previous;
    newest.maximumRemoteRunTimeMs = 30U * 60U * 1'000U;

    const bool firstSaved = store.save(previous);
    const bool secondSaved = store.save(newest);
    storage.corruptNewestSlot();

    application::UserSettings recovered{};
    const bool loaded = store.load(recovered);
    const bool expectedOutcome =
        firstSaved && secondSaved && loaded &&
        recovered.maximumRemoteRunTimeMs == previous.maximumRemoteRunTimeMs;

    std::cout
        << "scenario: settings-recovery\n"
        << "previous_remote_run_ms: " << previous.maximumRemoteRunTimeMs << '\n'
        << "corrupted_remote_run_ms: " << newest.maximumRemoteRunTimeMs << '\n'
        << "recovered_remote_run_ms: " << recovered.maximumRemoteRunTimeMs << '\n'
        << "scenario_result: " << (expectedOutcome ? "PASS" : "FAIL") << '\n';
    return expectedOutcome ? 0 : 2;
}

int runScenario(
    const Scenario scenario,
    const application::UserSettings* const suppliedSettings = nullptr) {
    if (scenario == Scenario::SettingsRecovery) {
        return runSettingsRecoveryScenario();
    }

    application::UserSettings settings =
        suppliedSettings == nullptr ? application::UserSettings{} : *suppliedSettings;
    if (scenario == Scenario::HoodRequired) {
        settings.hoodMonitoring = application::HoodMonitoringMode::Required;
    } else if (scenario == Scenario::HoodOptional) {
        settings.hoodMonitoring = application::HoodMonitoringMode::Disabled;
    }
    if (scenario == Scenario::TakeoverTimeout ||
        scenario == Scenario::TakeoverConfirmed) {
        settings.driverEntryMode = application::DriverEntryMode::RequireTakeover;
    }

    const application::UserConfiguration configuration =
        application::makeUserConfiguration(
            settings,
            &simulation::syntheticVehicleProfile());
    if (!configuration.validation.valid()) {
        std::cerr << "Configuration rejected, reasons_mask="
                  << configuration.validation.reasons << '\n';
        return 64;
    }

    simulation::SyntheticPowertrainState stopped{};
    simulation::SyntheticPowertrainState running{};
    running.engineRpm = 850U;

    simulation::SyntheticBodyState safeBody{};
    simulation::SyntheticBodyState finalBody = safeBody;
    const bool injectHoodOpening =
        scenario == Scenario::HoodRequired || scenario == Scenario::HoodOptional;
    const bool injectDriverEntry =
        scenario == Scenario::TakeoverTimeout ||
        scenario == Scenario::TakeoverConfirmed ||
        scenario == Scenario::UserConfig;
    if (injectHoodOpening) {
        finalBody.hoodClosed = false;
    }
    if (injectDriverEntry) {
        finalBody.doorsClosed = false;
    }

    const std::array<infrastructure::CanFrame, 8U> trace = {
        simulation::makeSyntheticPowertrainFrame(0U, stopped),
        simulation::makeSyntheticBodyFrame(0U, safeBody),
        simulation::makeSyntheticPowertrainFrame(1'800U, running),
        simulation::makeSyntheticBodyFrame(1'800U, safeBody),
        simulation::makeSyntheticPowertrainFrame(5'000U, running),
        simulation::makeSyntheticBodyFrame(5'000U, finalBody),
        simulation::makeSyntheticPowertrainFrame(6'000U, stopped),
        simulation::makeSyntheticBodyFrame(6'000U, safeBody),
    };

    simulation::SyntheticCanDecoder decoder{};
    infrastructure::ReplayVehicleGateway gateway{
        trace.data(), trace.size(), decoder};
    ConsoleActuators actuators{};
    ConsoleTimer timer{};
    ConsoleNotifications notifications{};
    infrastructure::Runtime runtime{
        application::Controller{configuration.controller},
        gateway,
        actuators,
        timer,
        notifications};

    std::cout << "scenario: " << scenarioName(scenario) << '\n';
    printUserSettings(settings);

    domain::VehicleState vehicle{};
    if (!gateway.setElapsedTime(0U)) {
        return 1;
    }
    if (scenario == Scenario::UserConfig) {
        application::LockSequenceDetector detector{configuration.lockSequence};
        std::uint32_t timestampMs = 1'000U;
        bool detected = false;
        for (std::uint8_t press = 0U; press < settings.lockPressCount; ++press) {
            detected = detector.observeLockPress(timestampMs);
            timestampMs += settings.lockMinimumGapMs;
        }
        std::cout << "lock_sequence: " << (detected ? "detected" : "rejected") << '\n';
        if (!detected) {
            std::cout << "scenario_result: FAIL\n";
            return 2;
        }
    }

    auto decision = runtime.dispatch(
        application::Event{application::EventType::RemoteStartRequested}, vehicle);
    printDecision("remote start", decision);

    if (scenario == Scenario::UserConfig && !settings.remoteStartEnabled) {
        const bool expectedOutcome =
            decision.state == application::ControllerState::Idle &&
            decision.contains(application::ActionType::NotifyRemoteStartDisabled);
        std::cout << "scenario_result: "
                  << (expectedOutcome ? "PASS" : "FAIL") << '\n';
        return expectedOutcome ? 0 : 2;
    }

    vehicle = gateway.state();
    decision = runtime.dispatch(
        application::Event{application::EventType::VehicleStateUpdated}, vehicle);
    printDecision("safe snapshot", decision);

    decision = runtime.dispatch(
        application::Event{application::EventType::TimerElapsed}, vehicle);
    printDecision("preparation timer", decision);

    if (!gateway.setElapsedTime(1'800U) || !gateway.requestState()) {
        std::cerr << "Replay failed at 1800 ms\n";
        return 1;
    }
    vehicle = gateway.state();
    decision = runtime.dispatch(
        application::Event{application::EventType::VehicleStateUpdated}, vehicle);
    printDecision("engine running", decision);

    if (!gateway.setElapsedTime(5'000U) || !gateway.requestState()) {
        std::cerr << "Replay failed at 5000 ms\n";
        return 1;
    }
    vehicle = gateway.state();
    decision = runtime.dispatch(
        application::Event{application::EventType::VehicleStateUpdated}, vehicle);
    printDecision(
        injectHoodOpening
            ? "hood opened"
            : (injectDriverEntry ? "driver door opened" : "safe running update"),
        decision);

    bool expectedOutcome = false;
    if (scenario == Scenario::Nominal) {
        decision = runtime.dispatch(
            application::Event{application::EventType::RemoteStopRequested}, vehicle);
        printDecision("remote stop", decision);

        if (!gateway.setElapsedTime(6'000U) || !gateway.requestState()) {
            std::cerr << "Replay failed at 6000 ms\n";
            return 1;
        }
        vehicle = gateway.state();
        decision = runtime.dispatch(
            application::Event{application::EventType::VehicleStateUpdated}, vehicle);
        printDecision("engine stopped", decision);
        expectedOutcome =
            decision.state == application::ControllerState::Idle &&
            decision.fault == application::FaultCode::None;
    } else if (scenario == Scenario::HoodRequired) {
        expectedOutcome =
            decision.state == application::ControllerState::Fault &&
            decision.fault == application::FaultCode::SafetyInterlock &&
            decision.safety.contains(application::SafetyReason::HoodOpen);
    } else if (scenario == Scenario::HoodOptional) {
        expectedOutcome =
            decision.state == application::ControllerState::Running &&
            decision.fault == application::FaultCode::None &&
            decision.safety.approved();
    } else if (scenario == Scenario::TakeoverTimeout) {
        decision = runtime.dispatch(
            application::Event{application::EventType::TimerElapsed}, vehicle);
        printDecision("takeover timer expired", decision);
        expectedOutcome =
            decision.state == application::ControllerState::Stopping &&
            decision.fault == application::FaultCode::None;
    } else if (scenario == Scenario::TakeoverConfirmed) {
        decision = runtime.dispatch(
            application::Event{application::EventType::DriverTakeoverConfirmed},
            vehicle);
        printDecision("authenticated takeover", decision);
        expectedOutcome =
            decision.state == application::ControllerState::DriverControl &&
            decision.fault == application::FaultCode::None &&
            decision.contains(application::ActionType::ReleaseRemoteControl);
    } else {
        const application::ControllerState expectedState =
            settings.driverEntryMode == application::DriverEntryMode::StopImmediately
                ? application::ControllerState::Stopping
                : application::ControllerState::AwaitingTakeover;
        expectedOutcome =
            decision.state == expectedState &&
            decision.fault == application::FaultCode::None;
    }

    const infrastructure::AssemblyStatistics stats = gateway.statistics();
    std::cout << "replay: " << stats.consumedFrames << " frames, "
              << stats.decodedSignals << " decoded signals\n"
              << "scenario_result: " << (expectedOutcome ? "PASS" : "FAIL") << '\n';
    return expectedOutcome ? 0 : 2;
}

int runUserConfiguration(const char* const path) {
    application::UserSettings settings{};
    if (!loadUserSettings(path, settings)) {
        return 64;
    }
    return runScenario(Scenario::UserConfig, &settings);
}

int runInteractive() {
    std::cout
        << "BMW E9x Remote Control - simulateur hors vehicule\n\n"
        << "1. Demarrage et arret nominaux\n"
        << "2. Capot obligatoire : ouverture => defaut\n"
        << "3. Capot facultatif : ouverture ignoree\n"
        << "4. Portiere ouverte sans reprise : arret apres delai\n"
        << "5. Portiere ouverte avec reprise conducteur validee\n"
        << "6. Tester un fichier de configuration utilisateur\n"
        << "7. Recuperation d'une configuration persistante corrompue\n"
        << "8. Quitter\n\n"
        << "Choix: ";

    unsigned selection = 0U;
    if (!(std::cin >> selection)) {
        std::cerr << "Invalid selection\n";
        return 64;
    }

    int result = 0;
    bool inputLineConsumed = false;
    switch (selection) {
        case 1U: result = runScenario(Scenario::Nominal); break;
        case 2U: result = runScenario(Scenario::HoodRequired); break;
        case 3U: result = runScenario(Scenario::HoodOptional); break;
        case 4U: result = runScenario(Scenario::TakeoverTimeout); break;
        case 5U: result = runScenario(Scenario::TakeoverConfirmed); break;
        case 6U: {
            std::cout << "Configuration path: ";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::string path{};
            std::getline(std::cin, path);
            inputLineConsumed = true;
            result = runUserConfiguration(path.c_str());
            break;
        }
        case 7U: result = runScenario(Scenario::SettingsRecovery); break;
        case 8U: return 0;
        default:
            std::cerr << "Invalid selection\n";
            result = 64;
            break;
    }

    std::cout << "\nPress Enter to close...";
    if (!inputLineConsumed) {
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    static_cast<void>(std::cin.get());
    return result;
}

}  // namespace

int main(const int argumentCount, char* arguments[]) {
    if (argumentCount == 1) {
        return runInteractive();
    }

    const std::string_view command{arguments[1]};
    if (command == "--help" && argumentCount == 2) {
        printUsage(arguments[0]);
        return 0;
    }
    if (command == "--list-scenarios" && argumentCount == 2) {
        printScenarioList();
        return 0;
    }
    if (command == "--scenario" && argumentCount == 3) {
        Scenario scenario{};
        if (!parseScenario(arguments[2], scenario)) {
            std::cerr << "Unknown scenario: " << arguments[2] << '\n';
            printScenarioList();
            return 64;
        }
        if (scenario == Scenario::UserConfig) {
            std::cerr << "The user-config scenario requires --config <path>\n";
            return 64;
        }
        return runScenario(scenario);
    }
    if (command == "--scenario" && argumentCount == 5 &&
        std::string_view{arguments[3]} == "--config") {
        Scenario scenario{};
        if (!parseScenario(arguments[2], scenario) ||
            scenario != Scenario::UserConfig) {
            std::cerr << "Only --scenario user-config accepts --config\n";
            return 64;
        }
        return runUserConfiguration(arguments[4]);
    }
    if (command == "--show-config" && argumentCount == 3) {
        application::UserSettings settings{};
        if (!loadUserSettings(arguments[2], settings)) {
            return 64;
        }
        printUserSettings(settings);
        return 0;
    }
    if (command == "--trace" && (argumentCount == 3 || argumentCount == 5)) {
        bool requireHoodClosed = true;
        if (argumentCount == 5 &&
            (std::string_view{arguments[3]} != "--hood" ||
             !parseHoodRequirement(arguments[4], requireHoodClosed))) {
            printUsage(arguments[0]);
            return 64;
        }
        return inspectExternalTrace(arguments[2], requireHoodClosed);
    }
    if (argumentCount == 2 && !command.empty() && command.front() != '-') {
        return inspectExternalTrace(arguments[1], true);
    }

    printUsage(arguments[0]);
    return 64;
}
