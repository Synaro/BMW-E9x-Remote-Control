#include "tools/sandbox_session.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

#include "bmw_remote/application/user_settings.hpp"
#include "bmw_remote/infrastructure/diagnostic_journal.hpp"
#include "bmw_remote/infrastructure/ports.hpp"
#include "bmw_remote/infrastructure/runtime.hpp"
#include "bmw_remote/simulation/synthetic_can.hpp"

namespace bmw::remote::host {
namespace {

constexpr std::uint32_t SupervisorServicePeriodMs = 250U;

class SandboxGateway final : public infrastructure::VehicleGateway {
public:
    bool requestState() noexcept override {
        ++requestCount;
        return true;
    }

    std::uint32_t requestCount{0U};
};

class SandboxActuatorDriver final : public infrastructure::ActuatorPort {
public:
    bool enableIgnition() noexcept override {
        ignitionActive = true;
        return true;
    }

    bool engageStarter() noexcept override {
        starterActive = true;
        return true;
    }

    bool disengageStarter() noexcept override {
        starterActive = false;
        return true;
    }

    bool releaseRemoteControl() noexcept override {
        reset();
        return true;
    }

    bool secureOutputs() noexcept override {
        reset();
        return true;
    }

    [[nodiscard]] infrastructure::ActuatorFeedback feedback() const noexcept {
        return {true, ignitionActive, starterActive};
    }

    void reset() noexcept {
        ignitionActive = false;
        starterActive = false;
    }

    bool ignitionActive{false};
    bool starterActive{false};
};

class SandboxTimer final : public infrastructure::TimerPort {
public:
    bool arm(const std::uint32_t durationMs) noexcept override {
        armed = true;
        duration = durationMs;
        changed = true;
        return true;
    }

    bool cancel() noexcept override {
        armed = false;
        duration = 0U;
        due = 0U;
        changed = false;
        return true;
    }

    void settle(const std::uint32_t nowMs) noexcept {
        if (armed && changed) {
            due = nowMs + duration;
            changed = false;
        }
    }

    void reset() noexcept {
        armed = false;
        changed = false;
        duration = 0U;
        due = 0U;
    }

    bool armed{false};
    bool changed{false};
    std::uint32_t duration{0U};
    std::uint32_t due{0U};
};

class SandboxNotifications final : public infrastructure::NotificationSink {
public:
    void publish(
        const application::ActionType notification,
        const application::ControllerState,
        const application::FaultCode,
        const application::SafetyAssessment,
        const application::ProfileReadinessAssessment) noexcept override {
        lastNotification = notification;
        available = true;
    }

    application::ActionType lastNotification{application::ActionType::NotifyReady};
    bool available{false};
};

[[nodiscard]] domain::VehicleState makeSafeVehicle() noexcept {
    domain::VehicleState vehicle{};
    vehicle.batteryMillivolts = domain::Observed<std::uint16_t>::fresh(12'500U);
    vehicle.engineRpm = domain::Observed<std::uint16_t>::fresh(0U);
    vehicle.hoodClosed = domain::Observed<bool>::fresh(true);
    vehicle.doorsClosed = domain::Observed<bool>::fresh(true);
    vehicle.trunkClosed = domain::Observed<bool>::fresh(true);
    vehicle.brakePressed = domain::Observed<bool>::fresh(false);
    vehicle.parkingBrakeApplied = domain::Observed<bool>::fresh(true);
    vehicle.transmission =
        domain::Observed<domain::Transmission>::fresh(domain::Transmission::Automatic);
    vehicle.gear = domain::Observed<domain::Gear>::fresh(domain::Gear::Park);
    vehicle.criticalFaultPresent = domain::Observed<bool>::fresh(false);
    return vehicle;
}

[[nodiscard]] std::string trim(const std::string& value) {
    const auto first = std::find_if_not(
        value.begin(), value.end(),
        [](const unsigned char character) { return std::isspace(character) != 0; });
    const auto last = std::find_if_not(
        value.rbegin(), value.rend(),
        [](const unsigned char character) { return std::isspace(character) != 0; })
                          .base();
    return first < last ? std::string(first, last) : std::string{};
}

[[nodiscard]] bool parseOnOff(const std::string& value, bool& result) noexcept {
    if (value == "on") {
        result = true;
        return true;
    }
    if (value == "off") {
        result = false;
        return true;
    }
    return false;
}

[[nodiscard]] bool parseClosedOpen(const std::string& value, bool& result) noexcept {
    if (value == "closed") {
        result = true;
        return true;
    }
    if (value == "open") {
        result = false;
        return true;
    }
    return false;
}

[[nodiscard]] bool parseUnsigned(
    const std::string& value,
    const std::uint32_t maximum,
    std::uint32_t& result) noexcept {
    if (value.empty()) {
        return false;
    }
    std::uint64_t parsed = 0U;
    for (const char character : value) {
        if (character < '0' || character > '9') {
            return false;
        }
        parsed = parsed * 10U + static_cast<unsigned>(character - '0');
        if (parsed > maximum) {
            return false;
        }
    }
    result = static_cast<std::uint32_t>(parsed);
    return true;
}

[[nodiscard]] const char* gearToString(const domain::Gear gear) noexcept {
    switch (gear) {
        case domain::Gear::Park: return "park";
        case domain::Gear::Neutral: return "neutral";
        case domain::Gear::Reverse: return "reverse";
        case domain::Gear::Drive: return "drive";
        case domain::Gear::Unknown: return "unknown";
    }
    return "unknown";
}

[[nodiscard]] std::string escapeJson(const std::string& value) {
    std::string escaped{};
    escaped.reserve(value.size());
    for (const unsigned char character : value) {
        switch (character) {
            case '\"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (character < 0x20U) {
                    escaped += "?";
                } else {
                    escaped += static_cast<char>(character);
                }
                break;
        }
    }
    return escaped;
}

}  // namespace

class SandboxSession::Implementation final {
public:
    Implementation() {
        restart(true);
    }

    void restart(const bool requireHood) {
        runtime_.reset();
        supervisor_.reset();
        driver_.reset();
        gateway_.requestCount = 0U;
        timer_.reset();
        notifications_ = {};
        journal_.clear();
        vehicle_ = makeSafeVehicle();
        nowMs_ = 0U;
        lastSupervisorServiceMs_ = 0U;
        hardwareStartPermitted_ = true;
        hoodMonitoringRequired_ = requireHood;
        lastEvent_ = application::EventType::VehicleStateUpdated;
        lastDecision_ = {};

        application::UserSettings settings{};
        settings.hoodMonitoring = requireHood
                                      ? application::HoodMonitoringMode::Required
                                      : application::HoodMonitoringMode::Disabled;
        const application::UserConfiguration userConfiguration =
            application::makeUserConfiguration(
                settings, &simulation::syntheticVehicleProfile());
        supervisor_ = std::make_unique<infrastructure::ActuatorSafetySupervisor>(driver_);
        runtime_ = std::make_unique<infrastructure::Runtime>(
            application::Controller{userConfiguration.controller},
            gateway_,
            *supervisor_,
            timer_,
            notifications_,
            &journal_);
        static_cast<void>(supervisor_->heartbeat(0U));
        static_cast<void>(supervisor_->poll(
            0U, hardwareStartPermitted_, driver_.feedback()));
    }

    [[nodiscard]] SandboxResult execute(const std::string& rawCommand) {
        const std::string command = trim(rawCommand);
        if (command.empty()) {
            return failure("empty command");
        }

        std::istringstream input{command};
        std::string verb{};
        input >> verb;

        if (verb == "status" || verb == "quit") {
            std::string extra{};
            if (input >> extra) {
                return failure("unexpected argument for " + verb);
            }
            return success();
        }
        if (verb == "new") {
            std::string mode{};
            std::string extra{};
            if (!(input >> mode) || (input >> extra) ||
                (mode != "required" && mode != "optional")) {
                return failure("usage: new required|optional");
            }
            restart(mode == "required");
            return success();
        }
        if (verb == "start") {
            return dispatchWithoutArguments(input, application::EventType::RemoteStartRequested, true);
        }
        if (verb == "stop") {
            return dispatchWithoutArguments(input, application::EventType::RemoteStopRequested, false);
        }
        if (verb == "takeover") {
            return dispatchWithoutArguments(input, application::EventType::DriverTakeoverConfirmed, false);
        }
        if (verb == "timer") {
            std::string extra{};
            if (input >> extra) {
                return failure("timer does not accept arguments");
            }
            if (!timer_.armed) {
                return failure("no timer is armed");
            }
            serviceSupervisor(timer_.due);
            nowMs_ = timer_.due;
            propagateSupervisorFault();
            if (runtime_->state() != application::ControllerState::Fault) {
                dispatch(application::Event{application::EventType::TimerElapsed});
            }
            return success();
        }
        if (verb == "reset") {
            std::string extra{};
            if (input >> extra) {
                return failure("reset does not accept arguments");
            }
            if (supervisor_->status().fault !=
                infrastructure::ActuatorSupervisorFault::None) {
                if (!supervisor_->resetFault(nowMs_, driver_.feedback())) {
                    return failure("actuator supervisor cannot be reset while outputs are unsafe");
                }
                static_cast<void>(supervisor_->heartbeat(nowMs_));
                static_cast<void>(supervisor_->poll(
                    nowMs_, hardwareStartPermitted_, driver_.feedback()));
                lastSupervisorServiceMs_ = nowMs_;
            }
            dispatch(application::Event{application::EventType::ResetRequested});
            return success();
        }
        if (verb == "watchdog") {
            std::string extra{};
            if (input >> extra) {
                return failure("watchdog does not accept arguments");
            }
            nowMs_ = lastSupervisorServiceMs_ + 501U;
            static_cast<void>(supervisor_->poll(
                nowMs_, hardwareStartPermitted_, driver_.feedback()));
            lastSupervisorServiceMs_ = nowMs_;
            propagateSupervisorFault();
            return success();
        }
        if (verb == "interlock") {
            std::string value{};
            std::string extra{};
            bool enabled = false;
            if (!(input >> value) || (input >> extra) || !parseOnOff(value, enabled)) {
                return failure("usage: interlock on|off");
            }
            hardwareStartPermitted_ = enabled;
            static_cast<void>(supervisor_->heartbeat(nowMs_));
            static_cast<void>(supervisor_->poll(
                nowMs_, hardwareStartPermitted_, driver_.feedback()));
            lastSupervisorServiceMs_ = nowMs_;
            propagateSupervisorFault();
            return success();
        }
        if (verb == "vehicle") {
            return updateVehicle(input);
        }
        return failure("unknown command: " + verb);
    }

    [[nodiscard]] SandboxSnapshot snapshot() const noexcept {
        SandboxSnapshot result{};
        result.timeMs = nowMs_;
        result.state = runtime_->state();
        result.fault = lastDecision_.fault;
        result.supervisorFault = supervisor_->status().fault;
        result.timerArmed = timer_.armed;
        result.timerDueMs = timer_.due;
        result.timerDurationMs = timer_.duration;
        result.ignitionActive = driver_.ignitionActive;
        result.starterActive = driver_.starterActive;
        result.hardwareStartPermitted = hardwareStartPermitted_;
        result.hoodMonitoringRequired = hoodMonitoringRequired_;
        result.vehicle = vehicle_;
        result.lastEvent = lastEvent_;
        result.lastDecision = lastDecision_;
        result.diagnosticRecords = journal_.size();
        return result;
    }

private:
    [[nodiscard]] SandboxResult success() const {
        return {true, {}, snapshot()};
    }

    [[nodiscard]] SandboxResult failure(std::string error) const {
        return {false, std::move(error), snapshot()};
    }

    [[nodiscard]] SandboxResult dispatchWithoutArguments(
        std::istringstream& input,
        const application::EventType type,
        const bool feedAuthorizationState) {
        std::string extra{};
        if (input >> extra) {
            return failure("command does not accept arguments");
        }
        serviceSupervisor(nowMs_);
        propagateSupervisorFault();
        if (runtime_->state() != application::ControllerState::Fault) {
            dispatch(application::Event{type});
            if (feedAuthorizationState &&
                runtime_->state() == application::ControllerState::Authorizing) {
                dispatch(application::Event{application::EventType::VehicleStateUpdated});
            }
        }
        return success();
    }

    void dispatch(const application::Event event) {
        lastEvent_ = event.type;
        lastDecision_ = runtime_->dispatch(event, vehicle_, nowMs_);
        timer_.settle(nowMs_);
    }

    void serviceSupervisor(const std::uint32_t targetMs) {
        while (targetMs - lastSupervisorServiceMs_ > SupervisorServicePeriodMs) {
            const std::uint32_t serviceAt =
                lastSupervisorServiceMs_ + SupervisorServicePeriodMs;
            static_cast<void>(supervisor_->heartbeat(serviceAt));
            static_cast<void>(supervisor_->poll(
                serviceAt, hardwareStartPermitted_, driver_.feedback()));
            lastSupervisorServiceMs_ = serviceAt;
            if (supervisor_->status().fault !=
                infrastructure::ActuatorSupervisorFault::None) {
                return;
            }
        }
        static_cast<void>(supervisor_->heartbeat(targetMs));
        static_cast<void>(supervisor_->poll(
            targetMs, hardwareStartPermitted_, driver_.feedback()));
        lastSupervisorServiceMs_ = targetMs;
    }

    void propagateSupervisorFault() {
        if (supervisor_->status().fault ==
                infrastructure::ActuatorSupervisorFault::None ||
            runtime_->state() == application::ControllerState::Fault) {
            return;
        }
        dispatch(application::Event::infrastructureFailure(
            application::FaultCode::ActuatorFailure));
    }

    [[nodiscard]] SandboxResult updateVehicle(std::istringstream& input) {
        domain::VehicleState candidate = vehicle_;
        std::string token{};
        bool found = false;
        while (input >> token) {
            found = true;
            const std::size_t separator = token.find('=');
            if (separator == std::string::npos || separator == 0U ||
                separator + 1U >= token.size()) {
                return failure("vehicle values must use key=value");
            }
            const std::string key = token.substr(0U, separator);
            const std::string value = token.substr(separator + 1U);
            bool booleanValue = false;
            std::uint32_t numericValue = 0U;

            if (key == "rpm") {
                if (!parseUnsigned(value, 8'000U, numericValue)) {
                    return failure("rpm must be between 0 and 8000");
                }
                candidate.engineRpm = domain::Observed<std::uint16_t>::fresh(
                    static_cast<std::uint16_t>(numericValue));
            } else if (key == "battery") {
                if (!parseUnsigned(value, 16'000U, numericValue) ||
                    numericValue < 9'000U) {
                    return failure("battery must be between 9000 and 16000 millivolts");
                }
                candidate.batteryMillivolts = domain::Observed<std::uint16_t>::fresh(
                    static_cast<std::uint16_t>(numericValue));
            } else if (key == "doors") {
                if (!parseClosedOpen(value, booleanValue)) {
                    return failure("doors must be closed or open");
                }
                candidate.doorsClosed = domain::Observed<bool>::fresh(booleanValue);
            } else if (key == "hood") {
                if (value == "unavailable") {
                    candidate.hoodClosed = {};
                } else if (parseClosedOpen(value, booleanValue)) {
                    candidate.hoodClosed = domain::Observed<bool>::fresh(booleanValue);
                } else {
                    return failure("hood must be closed, open or unavailable");
                }
            } else if (key == "trunk") {
                if (!parseClosedOpen(value, booleanValue)) {
                    return failure("trunk must be closed or open");
                }
                candidate.trunkClosed = domain::Observed<bool>::fresh(booleanValue);
            } else if (key == "brake") {
                if (value == "pressed") {
                    booleanValue = true;
                } else if (value == "released") {
                    booleanValue = false;
                } else {
                    return failure("brake must be pressed or released");
                }
                candidate.brakePressed = domain::Observed<bool>::fresh(booleanValue);
            } else if (key == "parking") {
                if (value == "applied") {
                    booleanValue = true;
                } else if (value == "released") {
                    booleanValue = false;
                } else {
                    return failure("parking must be applied or released");
                }
                candidate.parkingBrakeApplied =
                    domain::Observed<bool>::fresh(booleanValue);
            } else if (key == "gear") {
                domain::Gear gear = domain::Gear::Unknown;
                if (value == "park") {
                    gear = domain::Gear::Park;
                } else if (value == "neutral") {
                    gear = domain::Gear::Neutral;
                } else if (value == "reverse") {
                    gear = domain::Gear::Reverse;
                } else if (value == "drive") {
                    gear = domain::Gear::Drive;
                } else {
                    return failure("gear must be park, neutral, reverse or drive");
                }
                candidate.gear = domain::Observed<domain::Gear>::fresh(gear);
            } else if (key == "critical") {
                if (!parseOnOff(value, booleanValue)) {
                    return failure("critical must be on or off");
                }
                candidate.criticalFaultPresent =
                    domain::Observed<bool>::fresh(booleanValue);
            } else {
                return failure("unknown vehicle field: " + key);
            }
        }
        if (!found) {
            return failure("vehicle requires at least one key=value");
        }

        vehicle_ = candidate;
        serviceSupervisor(nowMs_);
        propagateSupervisorFault();
        if (runtime_->state() != application::ControllerState::Fault) {
            dispatch(application::Event{application::EventType::VehicleStateUpdated});
        }
        return success();
    }

    SandboxGateway gateway_{};
    SandboxActuatorDriver driver_{};
    SandboxTimer timer_{};
    SandboxNotifications notifications_{};
    infrastructure::DiagnosticJournal journal_{};
    domain::VehicleState vehicle_{};
    std::unique_ptr<infrastructure::ActuatorSafetySupervisor> supervisor_{};
    std::unique_ptr<infrastructure::Runtime> runtime_{};
    application::Decision lastDecision_{};
    application::EventType lastEvent_{application::EventType::VehicleStateUpdated};
    std::uint32_t nowMs_{0U};
    std::uint32_t lastSupervisorServiceMs_{0U};
    bool hardwareStartPermitted_{true};
    bool hoodMonitoringRequired_{true};
};

SandboxSession::SandboxSession()
    : implementation_(std::make_unique<Implementation>()) {}

SandboxSession::~SandboxSession() = default;
SandboxSession::SandboxSession(SandboxSession&&) noexcept = default;
SandboxSession& SandboxSession::operator=(SandboxSession&&) noexcept = default;

SandboxResult SandboxSession::execute(const std::string& command) {
    return implementation_->execute(command);
}

SandboxSnapshot SandboxSession::snapshot() const noexcept {
    return implementation_->snapshot();
}

std::string encodeSandboxResult(const SandboxResult& result) {
    const SandboxSnapshot& snapshot = result.snapshot;
    const domain::VehicleState& vehicle = snapshot.vehicle;
    std::ostringstream output{};
    output << std::boolalpha
           << "{\"ok\":" << result.ok
           << ",\"error\":\"" << escapeJson(result.error) << "\""
           << ",\"time_ms\":" << snapshot.timeMs
           << ",\"state\":\"" << application::toString(snapshot.state) << "\""
           << ",\"fault\":\"" << application::toString(snapshot.fault) << "\""
           << ",\"supervisor_fault\":\""
           << infrastructure::toString(snapshot.supervisorFault) << "\""
           << ",\"timer_armed\":" << snapshot.timerArmed
           << ",\"timer_due_ms\":" << snapshot.timerDueMs
           << ",\"timer_duration_ms\":" << snapshot.timerDurationMs
           << ",\"ignition_active\":" << snapshot.ignitionActive
           << ",\"starter_active\":" << snapshot.starterActive
           << ",\"hardware_start_permitted\":"
           << snapshot.hardwareStartPermitted
           << ",\"hood_monitoring_required\":"
           << snapshot.hoodMonitoringRequired
           << ",\"vehicle\":{"
           << "\"battery_mv\":" << vehicle.batteryMillivolts.value
           << ",\"rpm\":" << vehicle.engineRpm.value
           << ",\"hood_available\":"
           << (vehicle.hoodClosed.quality != domain::SignalQuality::Unavailable)
           << ",\"hood_closed\":" << vehicle.hoodClosed.value
           << ",\"doors_closed\":" << vehicle.doorsClosed.value
           << ",\"trunk_closed\":" << vehicle.trunkClosed.value
           << ",\"brake_pressed\":" << vehicle.brakePressed.value
           << ",\"parking_applied\":" << vehicle.parkingBrakeApplied.value
           << ",\"gear\":\"" << gearToString(vehicle.gear.value) << "\""
           << ",\"critical_fault\":" << vehicle.criticalFaultPresent.value
           << "}"
           << ",\"last_event\":\"" << application::toString(snapshot.lastEvent)
           << "\""
           << ",\"safety_reasons\":" << snapshot.lastDecision.safety.reasons
           << ",\"diagnostic_records\":" << snapshot.diagnosticRecords
           << ",\"last_actions\":[";
    for (std::size_t index = 0U; index < snapshot.lastDecision.actionCount; ++index) {
        if (index != 0U) {
            output << ',';
        }
        output << '\"'
               << application::toString(snapshot.lastDecision.actions[index].type)
               << '\"';
    }
    output << "]}";
    return output.str();
}

int runSandboxProtocol(std::istream& input, std::ostream& output) {
    SandboxSession session{};
    std::string line{};
    while (std::getline(input, line)) {
        const std::string command = trim(line);
        const SandboxResult result = session.execute(command);
        output << encodeSandboxResult(result) << '\n' << std::flush;
        if (command == "quit") {
            return result.ok ? 0 : 64;
        }
    }
    return 0;
}

}  // namespace bmw::remote::host
