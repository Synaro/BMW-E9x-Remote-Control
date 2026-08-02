#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "bmw_remote/application/controller.hpp"
#include "bmw_remote/application/safety_policy.hpp"
#include "bmw_remote/infrastructure/replay_vehicle_gateway.hpp"
#include "bmw_remote/infrastructure/runtime.hpp"
#include "bmw_remote/simulation/synthetic_can.hpp"
#include "tools/can_trace_csv.hpp"

namespace {

using namespace bmw::remote;

enum class Scenario : std::uint8_t {
    Nominal,
    HoodRequired,
    HoodOptional,
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
        << "hood-optional hood opens while running and is explicitly ignored\n";
}

void printUsage(const char* const executable) {
    std::cerr
        << "Usage:\n"
        << "  " << executable << " --scenario <name>\n"
        << "  " << executable
        << " --trace <trace.cantrace.csv> [--hood required|optional]\n"
        << "  " << executable << " --list-scenarios\n"
        << "  " << executable << " --help\n"
        << "  " << executable << " <trace.cantrace.csv>  (legacy form)\n";
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

int runScenario(const Scenario scenario) {
    simulation::SyntheticPowertrainState stopped{};
    simulation::SyntheticPowertrainState running{};
    running.engineRpm = 850U;

    simulation::SyntheticBodyState safeBody{};
    simulation::SyntheticBodyState finalBody = safeBody;
    const bool injectHoodOpening = scenario != Scenario::Nominal;
    if (injectHoodOpening) {
        finalBody.hoodClosed = false;
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
    application::ControllerConfig controllerConfig{};
    controllerConfig.vehicleProfile = &simulation::syntheticVehicleProfile();
    controllerConfig.safety.requireHoodClosed =
        scenario != Scenario::HoodOptional;
    infrastructure::Runtime runtime{
        application::Controller{controllerConfig},
        gateway,
        actuators,
        timer,
        notifications};

    std::cout << "scenario: " << scenarioName(scenario) << '\n'
              << "hood_requirement: "
              << (controllerConfig.safety.requireHoodClosed ? "required" : "optional")
              << '\n';

    domain::VehicleState vehicle{};
    if (!gateway.setElapsedTime(0U)) {
        return 1;
    }
    auto decision = runtime.dispatch(
        application::Event{application::EventType::RemoteStartRequested}, vehicle);
    printDecision("remote start", decision);

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
    printDecision(injectHoodOpening ? "hood opened" : "safe running update", decision);

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
    } else {
        expectedOutcome =
            decision.state == application::ControllerState::Running &&
            decision.fault == application::FaultCode::None &&
            decision.safety.approved();
    }

    const infrastructure::AssemblyStatistics stats = gateway.statistics();
    std::cout << "replay: " << stats.consumedFrames << " frames, "
              << stats.decodedSignals << " decoded signals\n"
              << "scenario_result: " << (expectedOutcome ? "PASS" : "FAIL") << '\n';
    return expectedOutcome ? 0 : 2;
}

int runInteractive() {
    std::cout
        << "BMW E9x Remote Control - simulateur hors vehicule\n\n"
        << "1. Demarrage et arret nominaux\n"
        << "2. Capot obligatoire : ouverture => defaut\n"
        << "3. Capot facultatif : ouverture ignoree\n"
        << "4. Quitter\n\n"
        << "Choix: ";

    unsigned selection = 0U;
    if (!(std::cin >> selection)) {
        std::cerr << "Invalid selection\n";
        return 64;
    }

    int result = 0;
    switch (selection) {
        case 1U: result = runScenario(Scenario::Nominal); break;
        case 2U: result = runScenario(Scenario::HoodRequired); break;
        case 3U: result = runScenario(Scenario::HoodOptional); break;
        case 4U: return 0;
        default:
            std::cerr << "Invalid selection\n";
            result = 64;
            break;
    }

    std::cout << "\nPress Enter to close...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
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
        return runScenario(scenario);
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
