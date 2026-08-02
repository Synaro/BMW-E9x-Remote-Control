#include <array>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "bmw_remote/application/controller.hpp"
#include "bmw_remote/application/lock_sequence_detector.hpp"
#include "bmw_remote/application/profile_readiness.hpp"
#include "bmw_remote/application/safety_policy.hpp"
#include "bmw_remote/application/user_settings.hpp"
#include "bmw_remote/domain/reference_profiles.hpp"
#include "bmw_remote/domain/vehicle_profile.hpp"
#include "bmw_remote/domain/vehicle_state.hpp"
#include "bmw_remote/infrastructure/replay_vehicle_gateway.hpp"
#include "bmw_remote/infrastructure/runtime.hpp"
#include "bmw_remote/infrastructure/vehicle_state_assembler.hpp"
#include "bmw_remote/simulation/synthetic_can.hpp"
#include "tools/can_trace_csv.hpp"
#include "tools/user_settings_file.hpp"

namespace {

using bmw::remote::application::ActionType;
using bmw::remote::application::Controller;
using bmw::remote::application::ControllerConfig;
using bmw::remote::application::ControllerState;
using bmw::remote::application::Event;
using bmw::remote::application::EventType;
using bmw::remote::application::FaultCode;
using bmw::remote::application::HoodMonitoringMode;
using bmw::remote::application::LockSequenceConfig;
using bmw::remote::application::LockSequenceDetector;
using bmw::remote::application::ProfileReadinessReason;
using bmw::remote::application::SafetyPolicy;
using bmw::remote::application::SafetyPolicyConfig;
using bmw::remote::application::SafetyReason;
using bmw::remote::application::UserSettings;
using bmw::remote::application::UserSettingsReason;
using bmw::remote::domain::Gear;
using bmw::remote::domain::Observed;
using bmw::remote::domain::SignalQuality;
using bmw::remote::domain::SignalSupport;
using bmw::remote::domain::Transmission;
using bmw::remote::domain::VehicleProfile;
using bmw::remote::domain::VehicleSignal;
using bmw::remote::domain::VehicleState;
using bmw::remote::infrastructure::ActuatorPort;
using bmw::remote::infrastructure::CanFrame;
using bmw::remote::infrastructure::NotificationSink;
using bmw::remote::infrastructure::ReplayStatus;
using bmw::remote::infrastructure::ReplayVehicleGateway;
using bmw::remote::infrastructure::Runtime;
using bmw::remote::infrastructure::VehicleStateAssembler;
using bmw::remote::infrastructure::TimerPort;
using bmw::remote::infrastructure::VehicleGateway;
using bmw::remote::simulation::SyntheticBodyState;
using bmw::remote::simulation::SyntheticCanDecoder;
using bmw::remote::simulation::SyntheticPowertrainState;
using bmw::remote::simulation::makeSyntheticBodyFrame;
using bmw::remote::simulation::makeSyntheticPowertrainFrame;
using bmw::remote::simulation::syntheticVehicleProfile;

int failures = 0;

void check(const bool condition, const char* expression, const int line) {
    if (!condition) {
        std::cerr << "FAIL line " << line << ": " << expression << '\n';
        ++failures;
    }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

const bmw::remote::application::Action* findAction(
    const bmw::remote::application::Decision& decision,
    const ActionType type) {
    for (std::size_t index = 0U; index < decision.actionCount; ++index) {
        if (decision.actions[index].type == type) {
            return &decision.actions[index];
        }
    }
    return nullptr;
}

VehicleState safeAutomaticVehicle() {
    VehicleState vehicle{};
    vehicle.batteryMillivolts = Observed<std::uint16_t>::fresh(12'500U);
    vehicle.engineRpm = Observed<std::uint16_t>::fresh(0U);
    vehicle.hoodClosed = Observed<bool>::fresh(true);
    vehicle.doorsClosed = Observed<bool>::fresh(true);
    vehicle.trunkClosed = Observed<bool>::fresh(true);
    vehicle.brakePressed = Observed<bool>::fresh(false);
    vehicle.parkingBrakeApplied = Observed<bool>::fresh(true);
    vehicle.transmission = Observed<Transmission>::fresh(Transmission::Automatic);
    vehicle.gear = Observed<Gear>::fresh(Gear::Park);
    vehicle.criticalFaultPresent = Observed<bool>::fresh(false);
    return vehicle;
}

Controller qualifiedController(ControllerConfig config = {}) {
    config.vehicleProfile = &syntheticVehicleProfile();
    return Controller{config};
}

void advanceToRunning(Controller& controller, VehicleState& vehicle) {
    static_cast<void>(controller.handle(Event{EventType::RemoteStartRequested}, vehicle));
    static_cast<void>(controller.handle(Event{EventType::VehicleStateUpdated}, vehicle));
    static_cast<void>(controller.handle(Event{EventType::TimerElapsed}, vehicle));
    vehicle.engineRpm = Observed<std::uint16_t>::fresh(800U);
    static_cast<void>(controller.handle(Event{EventType::VehicleStateUpdated}, vehicle));
}

void testThreeLockPressesTriggerRemoteStartGesture() {
    LockSequenceDetector detector{};

    CHECK(!detector.observeLockPress(1'000U));
    CHECK(!detector.observeLockPress(1'600U));
    CHECK(detector.observeLockPress(2'200U));
    CHECK(detector.pressCount() == 0U);
}

void testSlowLockSequenceDoesNotTrigger() {
    LockSequenceDetector detector{};

    CHECK(!detector.observeLockPress(0U));
    CHECK(!detector.observeLockPress(2'000U));
    CHECK(!detector.observeLockPress(2'500U));
    CHECK(detector.pressCount() == 2U);
}

void testLockButtonBounceIsIgnored() {
    LockSequenceConfig config{};
    config.minimumGapMs = 100U;
    LockSequenceDetector detector{config};

    CHECK(!detector.observeLockPress(1'000U));
    CHECK(!detector.observeLockPress(1'020U));
    CHECK(detector.pressCount() == 1U);
    CHECK(!detector.observeLockPress(1'500U));
    CHECK(detector.observeLockPress(2'000U));
}

void testDefaultUserSettingsAreValidAndPreserved() {
    const UserSettings settings{};
    const auto configuration = bmw::remote::application::makeUserConfiguration(
        settings,
        &syntheticVehicleProfile());

    CHECK(configuration.validation.valid());
    CHECK(configuration.controller.remoteStartEnabled);
    CHECK(configuration.controller.safety.requireHoodClosed);
    CHECK(configuration.controller.maximumRemoteRunTimeMs == 900'000U);
    CHECK(configuration.controller.driverTakeoverTimeoutMs == 60'000U);
    CHECK(configuration.lockSequence.requiredPresses == 3U);
}

void testUserSettingsConfigureHoodTimersEntryAndLocks() {
    UserSettings settings{};
    settings.hoodMonitoring = HoodMonitoringMode::Disabled;
    settings.driverEntryMode =
        bmw::remote::application::DriverEntryMode::StopImmediately;
    settings.maximumRemoteRunTimeMs = 30U * 60U * 1'000U;
    settings.driverTakeoverTimeoutMs = 120'000U;
    settings.lockPressCount = 4U;
    settings.lockMinimumGapMs = 100U;
    settings.lockMaximumGapMs = 2'000U;
    settings.lockMaximumSequenceMs = 6'000U;

    const auto configuration = bmw::remote::application::makeUserConfiguration(
        settings,
        &syntheticVehicleProfile());

    CHECK(configuration.validation.valid());
    CHECK(!configuration.controller.safety.requireHoodClosed);
    CHECK(configuration.controller.driverEntryMode ==
          bmw::remote::application::DriverEntryMode::StopImmediately);
    CHECK(configuration.controller.maximumRemoteRunTimeMs == 1'800'000U);
    CHECK(configuration.controller.driverTakeoverTimeoutMs == 120'000U);
    CHECK(configuration.lockSequence.requiredPresses == 4U);
    CHECK(configuration.lockSequence.maximumSequenceMs == 6'000U);
}

void testUnsafeUserSettingsAreRejectedFailClosed() {
    UserSettings settings{};
    settings.maximumRemoteRunTimeMs = 24U * 60U * 60U * 1'000U;
    settings.driverTakeoverTimeoutMs = 1'000U;
    settings.lockPressCount = 1U;
    settings.lockMinimumGapMs = 2'000U;
    settings.lockMaximumGapMs = 500U;
    settings.lockMaximumSequenceMs = 400U;

    const auto configuration = bmw::remote::application::makeUserConfiguration(
        settings,
        &syntheticVehicleProfile());

    CHECK(!configuration.validation.valid());
    CHECK(configuration.validation.contains(
        UserSettingsReason::RemoteRunTimeOutOfRange));
    CHECK(configuration.validation.contains(
        UserSettingsReason::TakeoverTimeoutOutOfRange));
    CHECK(configuration.validation.contains(
        UserSettingsReason::LockPressCountOutOfRange));
    CHECK(configuration.validation.contains(
        UserSettingsReason::InconsistentLockTiming));
    CHECK(!configuration.controller.remoteStartEnabled);
}

void testUserCanDisableRemoteStart() {
    UserSettings settings{};
    settings.remoteStartEnabled = false;
    const auto configuration = bmw::remote::application::makeUserConfiguration(
        settings,
        &syntheticVehicleProfile());
    Controller controller{configuration.controller};

    const auto disabled = controller.handle(
        Event{EventType::RemoteStartRequested}, safeAutomaticVehicle());

    CHECK(disabled.state == ControllerState::Idle);
    CHECK(disabled.contains(ActionType::SecureOutputs));
    CHECK(disabled.contains(ActionType::NotifyRemoteStartDisabled));
    CHECK(!disabled.contains(ActionType::RequestVehicleState));
}

void testUserCanStopImmediatelyWhenDoorOpens() {
    UserSettings settings{};
    settings.driverEntryMode =
        bmw::remote::application::DriverEntryMode::StopImmediately;
    const auto configuration = bmw::remote::application::makeUserConfiguration(
        settings,
        &syntheticVehicleProfile());
    Controller controller{configuration.controller};
    VehicleState vehicle = safeAutomaticVehicle();
    advanceToRunning(controller, vehicle);
    vehicle.doorsClosed.value = false;

    const auto stopping = controller.handle(
        Event{EventType::VehicleStateUpdated}, vehicle);

    CHECK(stopping.state == ControllerState::Stopping);
    CHECK(stopping.contains(ActionType::SecureOutputs));
    CHECK(!stopping.contains(ActionType::NotifyTakeoverPending));
}

void testUserSettingsFileLoadsStrictConfiguration() {
    std::istringstream input{
        "remote_start_enabled=true\n"
        "hood_monitoring=optional\n"
        "remote_run_minutes=30\n"
        "driver_entry_mode=stop_immediately\n"
        "takeover_timeout_seconds=120\n"
        "lock_press_count=4\n"
        "lock_minimum_gap_ms=100\n"
        "lock_maximum_gap_ms=1800\n"
        "lock_sequence_window_ms=6000\n"};
    UserSettings settings{};
    std::string error{};

    CHECK(bmw::remote::host::parseUserSettings(input, settings, error));
    CHECK(error.empty());
    CHECK(settings.hoodMonitoring == HoodMonitoringMode::Disabled);
    CHECK(settings.maximumRemoteRunTimeMs == 1'800'000U);
    CHECK(settings.driverEntryMode ==
          bmw::remote::application::DriverEntryMode::StopImmediately);
    CHECK(settings.driverTakeoverTimeoutMs == 120'000U);
    CHECK(settings.lockPressCount == 4U);
}

void testUserSettingsFileRejectsUnknownAndDuplicateKeys() {
    std::istringstream unknown{"mystery_setting=true\n"};
    std::istringstream duplicate{
        "remote_run_minutes=10\n"
        "remote_run_minutes=20\n"};
    UserSettings settings{};
    std::string error{};

    CHECK(!bmw::remote::host::parseUserSettings(unknown, settings, error));
    CHECK(error.find("unknown setting") != std::string::npos);
    error.clear();
    CHECK(!bmw::remote::host::parseUserSettings(duplicate, settings, error));
    CHECK(error.find("duplicate setting") != std::string::npos);
}

void testUserSettingsFileRejectsUnsafeDurations() {
    std::istringstream input{
        "remote_run_minutes=0\n"
        "takeover_timeout_seconds=1\n"};
    UserSettings settings{};
    std::string error{};

    CHECK(!bmw::remote::host::parseUserSettings(input, settings, error));
    CHECK(error.find("safety bounds") != std::string::npos);
}

void testSafeAutomaticVehicleIsApproved() {
    const SafetyPolicy policy{};
    const auto assessment = policy.assessStart(safeAutomaticVehicle());
    CHECK(assessment.approved());
}

void testUnavailableSignalFailsClosed() {
    SafetyPolicy policy{};
    VehicleState vehicle = safeAutomaticVehicle();
    vehicle.hoodClosed.quality = SignalQuality::Stale;

    const auto assessment = policy.assessStart(vehicle);
    CHECK(!assessment.approved());
    CHECK(assessment.contains(SafetyReason::SignalUnavailable));
}

void testUnsafeVehicleReportsAllDetectedReasons() {
    SafetyPolicy policy{};
    VehicleState vehicle = safeAutomaticVehicle();
    vehicle.batteryMillivolts.value = 10'900U;
    vehicle.hoodClosed.value = false;
    vehicle.brakePressed.value = true;
    vehicle.parkingBrakeApplied.value = false;
    vehicle.gear.value = Gear::Drive;

    const auto assessment = policy.assessStart(vehicle);
    CHECK(assessment.contains(SafetyReason::BatteryTooLow));
    CHECK(assessment.contains(SafetyReason::HoodOpen));
    CHECK(assessment.contains(SafetyReason::BrakePressed));
    CHECK(assessment.contains(SafetyReason::ParkingBrakeReleased));
    CHECK(assessment.contains(SafetyReason::TransmissionUnsafe));
}

void testManualTransmissionIsDeniedByDefault() {
    SafetyPolicy policy{};
    VehicleState vehicle = safeAutomaticVehicle();
    vehicle.transmission.value = Transmission::Manual;
    vehicle.gear.value = Gear::Neutral;

    const auto assessment = policy.assessStart(vehicle);
    CHECK(assessment.contains(SafetyReason::TransmissionUnsafe));
}

void testManualTransmissionRequiresExplicitOptIn() {
    SafetyPolicyConfig config{};
    config.allowManualTransmission = true;
    SafetyPolicy policy{config};
    VehicleState vehicle = safeAutomaticVehicle();
    vehicle.transmission.value = Transmission::Manual;
    vehicle.gear.value = Gear::Neutral;

    CHECK(policy.assessStart(vehicle).approved());
}

void testHoodSignalCanBeDisabledByExplicitSafetyPolicy() {
    SafetyPolicyConfig config{};
    config.requireHoodClosed = false;
    const SafetyPolicy policy{config};
    VehicleState vehicle = safeAutomaticVehicle();
    vehicle.hoodClosed = {};

    CHECK(policy.assessStart(vehicle).approved());
    CHECK(policy.assessCranking(vehicle).approved());

    vehicle.engineRpm.value = 800U;
    CHECK(policy.assessRemoteRun(vehicle).approved());
}

void testControllerRejectsStartWhenNoProfileIsSelected() {
    Controller controller{};

    const auto decision = controller.handle(
        Event{EventType::RemoteStartRequested}, safeAutomaticVehicle());

    CHECK(decision.state == ControllerState::Idle);
    CHECK(decision.profileReadiness.contains(
        ProfileReadinessReason::ProfileNotSelected));
    CHECK(decision.contains(ActionType::SecureOutputs));
    CHECK(decision.contains(ActionType::NotifyProfileRejected));
    CHECK(!decision.contains(ActionType::RequestVehicleState));
}

void testControllerRejectsUnqualifiedReferenceProfile() {
    ControllerConfig config{};
    config.vehicleProfile =
        &bmw::remote::domain::profiles::e90_2009_n47d20c_automatic();
    Controller controller{config};

    const auto decision = controller.handle(
        Event{EventType::RemoteStartRequested}, safeAutomaticVehicle());

    CHECK(decision.state == ControllerState::Idle);
    CHECK(decision.profileReadiness.contains(
        ProfileReadinessReason::UnverifiedRequiredSignal));
    CHECK(decision.profileReadiness.contains(ProfileReadinessReason::QualificationTooLow));
    CHECK(decision.contains(ActionType::NotifyProfileRejected));
    CHECK(!decision.contains(ActionType::EnableIgnition));
}

void testNominalStartSequence() {
    Controller controller = qualifiedController();
    VehicleState vehicle = safeAutomaticVehicle();

    const auto authorization = controller.handle(
        Event{EventType::RemoteStartRequested}, vehicle);
    CHECK(authorization.previousState == ControllerState::Idle);
    CHECK(authorization.state == ControllerState::Authorizing);
    CHECK(authorization.contains(ActionType::RequestVehicleState));
    CHECK(authorization.contains(ActionType::ArmTimer));

    const auto preparation = controller.handle(
        Event{EventType::VehicleStateUpdated}, vehicle);
    CHECK(preparation.state == ControllerState::Preparing);
    CHECK(preparation.contains(ActionType::EnableIgnition));
    CHECK(preparation.contains(ActionType::NotifyStartAccepted));

    const auto cranking = controller.handle(Event{EventType::TimerElapsed}, vehicle);
    CHECK(cranking.state == ControllerState::Cranking);
    CHECK(cranking.contains(ActionType::EngageStarter));

    vehicle.engineRpm.value = 850U;
    const auto running = controller.handle(Event{EventType::VehicleStateUpdated}, vehicle);
    CHECK(running.state == ControllerState::Running);
    CHECK(running.contains(ActionType::DisengageStarter));
    CHECK(running.contains(ActionType::NotifyRunning));
}

void testUnsafeStartReturnsToIdleWithoutLatchingFault() {
    Controller controller = qualifiedController();
    VehicleState vehicle = safeAutomaticVehicle();
    vehicle.hoodClosed.value = false;

    static_cast<void>(controller.handle(Event{EventType::RemoteStartRequested}, vehicle));
    const auto rejected = controller.handle(Event{EventType::VehicleStateUpdated}, vehicle);

    CHECK(rejected.state == ControllerState::Idle);
    CHECK(rejected.fault == FaultCode::None);
    CHECK(rejected.safety.contains(SafetyReason::HoodOpen));
    CHECK(rejected.contains(ActionType::NotifyStartRejected));
    CHECK(rejected.contains(ActionType::SecureOutputs));
}

void testDuplicateStartRequestIsIgnored() {
    Controller controller = qualifiedController();
    const VehicleState vehicle = safeAutomaticVehicle();
    static_cast<void>(controller.handle(Event{EventType::RemoteStartRequested}, vehicle));

    const auto duplicate = controller.handle(Event{EventType::RemoteStartRequested}, vehicle);
    CHECK(duplicate.state == ControllerState::Authorizing);
    CHECK(duplicate.contains(ActionType::NotifyRequestIgnored));
    CHECK(!duplicate.contains(ActionType::EngageStarter));
}

void testCrankingTimeoutLatchesFaultAndSafesOutputs() {
    Controller controller = qualifiedController();
    VehicleState vehicle = safeAutomaticVehicle();
    static_cast<void>(controller.handle(Event{EventType::RemoteStartRequested}, vehicle));
    static_cast<void>(controller.handle(Event{EventType::VehicleStateUpdated}, vehicle));
    static_cast<void>(controller.handle(Event{EventType::TimerElapsed}, vehicle));

    const auto timeout = controller.handle(Event{EventType::TimerElapsed}, vehicle);
    CHECK(timeout.state == ControllerState::Fault);
    CHECK(timeout.fault == FaultCode::CrankTimeout);
    CHECK(timeout.contains(ActionType::DisengageStarter));
    CHECK(timeout.contains(ActionType::SecureOutputs));
    CHECK(timeout.contains(ActionType::NotifyFault));
}

void testSafetyViolationWhileRunningLatchesFault() {
    Controller controller = qualifiedController();
    VehicleState vehicle = safeAutomaticVehicle();
    advanceToRunning(controller, vehicle);
    CHECK(controller.state() == ControllerState::Running);

    vehicle.hoodClosed.value = false;
    const auto decision = controller.handle(Event{EventType::VehicleStateUpdated}, vehicle);
    CHECK(decision.state == ControllerState::Fault);
    CHECK(decision.fault == FaultCode::SafetyInterlock);
    CHECK(decision.safety.contains(SafetyReason::HoodOpen));
    CHECK(decision.contains(ActionType::SecureOutputs));
}

void testRemoteStopRequiresStoppedEngineConfirmation() {
    Controller controller = qualifiedController();
    VehicleState vehicle = safeAutomaticVehicle();
    advanceToRunning(controller, vehicle);

    const auto stopping = controller.handle(Event{EventType::RemoteStopRequested}, vehicle);
    CHECK(stopping.state == ControllerState::Stopping);
    CHECK(stopping.contains(ActionType::SecureOutputs));
    CHECK(stopping.contains(ActionType::ArmTimer));

    vehicle.engineRpm.value = 0U;
    const auto stopped = controller.handle(Event{EventType::VehicleStateUpdated}, vehicle);
    CHECK(stopped.state == ControllerState::Idle);
    CHECK(stopped.contains(ActionType::NotifyStopped));
}

void testRemoteRunTimerInitiatesStop() {
    Controller controller = qualifiedController();
    VehicleState vehicle = safeAutomaticVehicle();
    advanceToRunning(controller, vehicle);

    const auto expired = controller.handle(Event{EventType::TimerElapsed}, vehicle);
    CHECK(expired.state == ControllerState::Stopping);
    CHECK(expired.contains(ActionType::SecureOutputs));
    CHECK(expired.contains(ActionType::NotifyStopping));
}

void testRemoteRunIsLimitedToFifteenMinutesByDefault() {
    Controller controller = qualifiedController();
    VehicleState vehicle = safeAutomaticVehicle();

    static_cast<void>(controller.handle(Event{EventType::RemoteStartRequested}, vehicle));
    static_cast<void>(controller.handle(Event{EventType::VehicleStateUpdated}, vehicle));
    static_cast<void>(controller.handle(Event{EventType::TimerElapsed}, vehicle));
    vehicle.engineRpm.value = 800U;
    const auto running = controller.handle(
        Event{EventType::VehicleStateUpdated}, vehicle);

    const auto* const timer = findAction(running, ActionType::ArmTimer);
    CHECK(timer != nullptr);
    if (timer != nullptr) {
        CHECK(timer->durationMs == 15U * 60U * 1'000U);
    }
}

void testDoorOpeningStartsBoundedDriverTakeover() {
    Controller controller = qualifiedController();
    VehicleState vehicle = safeAutomaticVehicle();
    advanceToRunning(controller, vehicle);

    vehicle.doorsClosed.value = false;
    vehicle.brakePressed.value = true;
    const auto pending = controller.handle(
        Event{EventType::VehicleStateUpdated}, vehicle);

    CHECK(pending.state == ControllerState::AwaitingTakeover);
    CHECK(pending.safety.approved());
    CHECK(pending.contains(ActionType::NotifyTakeoverPending));
    const auto* const timer = findAction(pending, ActionType::ArmTimer);
    CHECK(timer != nullptr);
    if (timer != nullptr) {
        CHECK(timer->durationMs == 60'000U);
    }
}

void testUnconfirmedDriverTakeoverStopsRemoteEngine() {
    Controller controller = qualifiedController();
    VehicleState vehicle = safeAutomaticVehicle();
    advanceToRunning(controller, vehicle);
    vehicle.doorsClosed.value = false;
    static_cast<void>(controller.handle(
        Event{EventType::VehicleStateUpdated}, vehicle));

    const auto expired = controller.handle(Event{EventType::TimerElapsed}, vehicle);

    CHECK(expired.state == ControllerState::Stopping);
    CHECK(expired.contains(ActionType::SecureOutputs));
    CHECK(expired.contains(ActionType::NotifyStopping));
}

void testConfirmedDriverTakeoverReleasesRemoteControl() {
    Controller controller = qualifiedController();
    VehicleState vehicle = safeAutomaticVehicle();
    advanceToRunning(controller, vehicle);
    vehicle.doorsClosed.value = false;
    vehicle.brakePressed.value = true;
    static_cast<void>(controller.handle(
        Event{EventType::VehicleStateUpdated}, vehicle));

    const auto takeover = controller.handle(
        Event{EventType::DriverTakeoverConfirmed}, vehicle);

    CHECK(takeover.state == ControllerState::DriverControl);
    CHECK(takeover.safety.approved());
    CHECK(takeover.contains(ActionType::ReleaseRemoteControl));
    CHECK(takeover.contains(ActionType::NotifyTakeoverComplete));

    const auto remoteStop = controller.handle(
        Event{EventType::RemoteStopRequested}, vehicle);
    CHECK(remoteStop.state == ControllerState::DriverControl);
    CHECK(remoteStop.contains(ActionType::NotifyRequestIgnored));

    vehicle.engineRpm.value = 0U;
    const auto ready = controller.handle(
        Event{EventType::VehicleStateUpdated}, vehicle);
    CHECK(ready.state == ControllerState::Idle);
    CHECK(ready.contains(ActionType::NotifyReady));
}

void testUnsafeDriverEntryLatchesSafetyFault() {
    Controller controller = qualifiedController();
    VehicleState vehicle = safeAutomaticVehicle();
    advanceToRunning(controller, vehicle);
    vehicle.doorsClosed.value = false;
    vehicle.trunkClosed.value = false;
    vehicle.parkingBrakeApplied.value = false;

    const auto unsafe = controller.handle(
        Event{EventType::VehicleStateUpdated}, vehicle);

    CHECK(unsafe.state == ControllerState::Fault);
    CHECK(unsafe.fault == FaultCode::SafetyInterlock);
    CHECK(unsafe.safety.contains(SafetyReason::TrunkOpen));
    CHECK(unsafe.safety.contains(SafetyReason::ParkingBrakeReleased));
}

void testFaultResetRequiresStoppedEngineAndNoCriticalFault() {
    Controller controller = qualifiedController();
    VehicleState vehicle = safeAutomaticVehicle();
    static_cast<void>(controller.handle(Event{EventType::InfrastructureFailure, FaultCode::ActuatorFailure}, vehicle));

    vehicle.engineRpm.value = 900U;
    const auto denied = controller.handle(Event{EventType::ResetRequested}, vehicle);
    CHECK(denied.state == ControllerState::Fault);
    CHECK(denied.contains(ActionType::NotifyResetRejected));

    vehicle.engineRpm.value = 0U;
    const auto reset = controller.handle(Event{EventType::ResetRequested}, vehicle);
    CHECK(reset.state == ControllerState::Idle);
    CHECK(reset.fault == FaultCode::None);
    CHECK(reset.contains(ActionType::NotifyReady));
}

struct FakeVehicleGateway final : VehicleGateway {
    bool succeeds{true};
    std::uint32_t requests{0U};

    bool requestState() noexcept override {
        ++requests;
        return succeeds;
    }
};

struct FakeActuator final : ActuatorPort {
    bool ignitionSucceeds{true};
    bool releaseSucceeds{true};
    std::uint32_t secureCalls{0U};
    std::uint32_t starterReleaseCalls{0U};
    std::uint32_t remoteControlReleaseCalls{0U};

    bool enableIgnition() noexcept override { return ignitionSucceeds; }
    bool engageStarter() noexcept override { return true; }
    bool disengageStarter() noexcept override {
        ++starterReleaseCalls;
        return true;
    }
    bool releaseRemoteControl() noexcept override {
        ++remoteControlReleaseCalls;
        return releaseSucceeds;
    }
    bool secureOutputs() noexcept override {
        ++secureCalls;
        return true;
    }
};

struct FakeTimer final : TimerPort {
    bool arm(std::uint32_t) noexcept override { return true; }
    bool cancel() noexcept override { return true; }
};

struct FakeNotifications final : NotificationSink {
    std::uint32_t faultNotifications{0U};
    std::uint32_t profileRejectedNotifications{0U};
    std::uint8_t lastProfileReasons{0U};

    void publish(
        const ActionType notification,
        ControllerState,
        FaultCode,
        bmw::remote::application::SafetyAssessment,
        const bmw::remote::application::ProfileReadinessAssessment
            profileReadiness) noexcept override {
        lastProfileReasons = profileReadiness.reasons;
        if (notification == ActionType::NotifyFault) {
            ++faultNotifications;
        } else if (notification == ActionType::NotifyProfileRejected) {
            ++profileRejectedNotifications;
        }
    }
};

void testRuntimeRejectsMissingProfileWithoutRequestingVehicle() {
    FakeVehicleGateway gateway{};
    FakeActuator actuator{};
    FakeTimer timer{};
    FakeNotifications notifications{};
    Runtime runtime{Controller{}, gateway, actuator, timer, notifications};

    const auto result = runtime.dispatch(
        Event{EventType::RemoteStartRequested}, safeAutomaticVehicle());

    CHECK(result.state == ControllerState::Idle);
    CHECK(gateway.requests == 0U);
    CHECK(actuator.secureCalls == 1U);
    CHECK(notifications.profileRejectedNotifications == 1U);
    CHECK(
        (notifications.lastProfileReasons &
         bmw::remote::application::mask(ProfileReadinessReason::ProfileNotSelected)) != 0U);
}

void testRuntimeConvertsGatewayFailureIntoSafeFault() {
    FakeVehicleGateway gateway{};
    gateway.succeeds = false;
    FakeActuator actuator{};
    FakeTimer timer{};
    FakeNotifications notifications{};
    Runtime runtime{qualifiedController(), gateway, actuator, timer, notifications};

    const auto result = runtime.dispatch(
        Event{EventType::RemoteStartRequested}, safeAutomaticVehicle());

    CHECK(result.state == ControllerState::Fault);
    CHECK(result.fault == FaultCode::VehicleCommunication);
    CHECK(gateway.requests == 1U);
    CHECK(actuator.starterReleaseCalls == 1U);
    CHECK(actuator.secureCalls == 1U);
    CHECK(notifications.faultNotifications == 1U);
}

void testRuntimeConvertsActuatorFailureIntoSafeFault() {
    FakeVehicleGateway gateway{};
    FakeActuator actuator{};
    actuator.ignitionSucceeds = false;
    FakeTimer timer{};
    FakeNotifications notifications{};
    Runtime runtime{qualifiedController(), gateway, actuator, timer, notifications};
    VehicleState vehicle = safeAutomaticVehicle();

    static_cast<void>(runtime.dispatch(Event{EventType::RemoteStartRequested}, vehicle));
    const auto result = runtime.dispatch(Event{EventType::VehicleStateUpdated}, vehicle);

    CHECK(result.state == ControllerState::Fault);
    CHECK(result.fault == FaultCode::ActuatorFailure);
    CHECK(actuator.starterReleaseCalls == 1U);
    CHECK(actuator.secureCalls == 1U);
}

void testRuntimeSafesOutputsWhenTakeoverReleaseFails() {
    FakeVehicleGateway gateway{};
    FakeActuator actuator{};
    actuator.releaseSucceeds = false;
    FakeTimer timer{};
    FakeNotifications notifications{};
    Runtime runtime{qualifiedController(), gateway, actuator, timer, notifications};
    VehicleState vehicle = safeAutomaticVehicle();

    static_cast<void>(runtime.dispatch(Event{EventType::RemoteStartRequested}, vehicle));
    static_cast<void>(runtime.dispatch(Event{EventType::VehicleStateUpdated}, vehicle));
    static_cast<void>(runtime.dispatch(Event{EventType::TimerElapsed}, vehicle));
    vehicle.engineRpm.value = 800U;
    static_cast<void>(runtime.dispatch(Event{EventType::VehicleStateUpdated}, vehicle));
    vehicle.doorsClosed.value = false;
    static_cast<void>(runtime.dispatch(Event{EventType::VehicleStateUpdated}, vehicle));

    const auto failed = runtime.dispatch(
        Event{EventType::DriverTakeoverConfirmed}, vehicle);

    CHECK(failed.state == ControllerState::Fault);
    CHECK(failed.fault == FaultCode::ActuatorFailure);
    CHECK(actuator.remoteControlReleaseCalls == 1U);
    CHECK(actuator.secureCalls == 1U);
    CHECK(notifications.faultNotifications == 1U);
}

void testReplayRejectsNonMonotonicTrace() {
    const std::array<CanFrame, 2U> trace = {
        makeSyntheticPowertrainFrame(10U),
        makeSyntheticBodyFrame(5U),
    };
    SyntheticCanDecoder decoder{};
    ReplayVehicleGateway gateway{trace.data(), trace.size(), decoder};

    CHECK(!gateway.requestState());
    CHECK(gateway.lastBatch().status == ReplayStatus::InvalidTrace);
    CHECK(gateway.lastBatch().emittedFrames == 0U);
}

void testReplayOnlyEmitsFramesDueAtCurrentTime() {
    SyntheticPowertrainState running{};
    running.engineRpm = 850U;
    const std::array<CanFrame, 4U> trace = {
        makeSyntheticPowertrainFrame(0U),
        makeSyntheticBodyFrame(0U),
        makeSyntheticPowertrainFrame(1'000U, running),
        makeSyntheticBodyFrame(1'000U),
    };
    SyntheticCanDecoder decoder{};
    ReplayVehicleGateway gateway{trace.data(), trace.size(), decoder};

    CHECK(gateway.setElapsedTime(0U));
    CHECK(gateway.requestState());
    CHECK(gateway.lastBatch().status == ReplayStatus::Ready);
    CHECK(gateway.lastBatch().emittedFrames == 2U);
    CHECK(gateway.state().engineRpm.value == 0U);

    CHECK(gateway.setElapsedTime(999U));
    CHECK(gateway.requestState());
    CHECK(gateway.lastBatch().emittedFrames == 0U);

    CHECK(gateway.setElapsedTime(1'000U));
    CHECK(gateway.requestState());
    CHECK(gateway.lastBatch().status == ReplayStatus::Complete);
    CHECK(gateway.lastBatch().emittedFrames == 2U);
    CHECK(gateway.state().engineRpm.value == 850U);
}

void testAssemblerMarksOldSignalsStale() {
    const std::array<CanFrame, 2U> trace = {
        makeSyntheticPowertrainFrame(0U),
        makeSyntheticBodyFrame(0U),
    };
    SyntheticCanDecoder decoder{};
    ReplayVehicleGateway gateway{trace.data(), trace.size(), decoder};

    CHECK(gateway.requestState());
    CHECK(gateway.setElapsedTime(2'501U));
    const VehicleState stale = gateway.state();

    CHECK(stale.batteryMillivolts.quality == SignalQuality::Stale);
    CHECK(stale.engineRpm.quality == SignalQuality::Stale);
    CHECK(stale.hoodClosed.quality == SignalQuality::Stale);
    CHECK(stale.transmission.quality == SignalQuality::Stale);
    CHECK(stale.criticalFaultPresent.quality == SignalQuality::Stale);
}

void testRecognizedMalformedFrameStopsReplay() {
    CanFrame malformed = makeSyntheticPowertrainFrame(0U);
    malformed.data[7] = 0U;
    const std::array<CanFrame, 1U> trace = {malformed};
    SyntheticCanDecoder decoder{};
    ReplayVehicleGateway gateway{trace.data(), trace.size(), decoder};

    CHECK(!gateway.requestState());
    CHECK(gateway.lastBatch().status == ReplayStatus::ConsumerRejected);
    CHECK(gateway.statistics().rejectedFrames == 1U);
}

void testUnknownFrameIsIgnoredWithoutCreatingSignals() {
    CanFrame unknown{};
    unknown.identifier = 0x123U;
    unknown.dataLength = 8U;
    const std::array<CanFrame, 1U> trace = {unknown};
    SyntheticCanDecoder decoder{};
    ReplayVehicleGateway gateway{trace.data(), trace.size(), decoder};

    CHECK(gateway.requestState());
    CHECK(gateway.lastBatch().status == ReplayStatus::Complete);
    CHECK(gateway.statistics().ignoredFrames == 1U);
    CHECK(gateway.state().batteryMillivolts.quality == SignalQuality::Unavailable);
}

void testInvalidDecodedBatchIsAppliedAtomically() {
    SyntheticPowertrainState invalid{};
    invalid.transmission = static_cast<Transmission>(99U);
    const CanFrame frame = makeSyntheticPowertrainFrame(0U, invalid);
    SyntheticCanDecoder decoder{};
    VehicleStateAssembler assembler{decoder};

    CHECK(!assembler.consume(frame));
    const VehicleState vehicle = assembler.snapshot(0U);
    CHECK(vehicle.engineRpm.quality == SignalQuality::Unavailable);
    CHECK(vehicle.batteryMillivolts.quality == SignalQuality::Unavailable);
    CHECK(vehicle.transmission.quality == SignalQuality::Unavailable);
}

void testReplayRejectsClockMovingBackwards() {
    const std::array<CanFrame, 1U> trace = {
        makeSyntheticPowertrainFrame(0U),
    };
    SyntheticCanDecoder decoder{};
    ReplayVehicleGateway gateway{trace.data(), trace.size(), decoder};

    CHECK(gateway.setElapsedTime(100U));
    CHECK(gateway.requestState());
    CHECK(!gateway.setElapsedTime(99U));
    CHECK(!gateway.requestState());
    CHECK(gateway.lastBatch().status == ReplayStatus::NonMonotonicTime);

    gateway.reset();
    CHECK(gateway.setElapsedTime(0U));
    CHECK(gateway.requestState());
}

void testReplayedVehicleStateFeedsSafetyPolicy() {
    SyntheticBodyState hoodOpen{};
    hoodOpen.hoodClosed = false;
    const std::array<CanFrame, 4U> trace = {
        makeSyntheticPowertrainFrame(0U),
        makeSyntheticBodyFrame(0U),
        makeSyntheticPowertrainFrame(100U),
        makeSyntheticBodyFrame(100U, hoodOpen),
    };
    SyntheticCanDecoder decoder{};
    ReplayVehicleGateway gateway{trace.data(), trace.size(), decoder};
    SafetyPolicy policy{};

    CHECK(gateway.requestState());
    CHECK(policy.assessStart(gateway.state()).approved());

    CHECK(gateway.setElapsedTime(100U));
    CHECK(gateway.requestState());
    const auto assessment = policy.assessStart(gateway.state());
    CHECK(!assessment.approved());
    CHECK(assessment.contains(SafetyReason::HoodOpen));
}

void testReferenceProfileDescribesReferenceVehicleWithoutClaimingQualification() {
    const VehicleProfile& profile =
        bmw::remote::domain::profiles::e90_2009_n47d20c_automatic();

    CHECK(std::string_view{profile.id} == "bmw-e90-2009-n47d20c-automatic");
    CHECK(profile.body == bmw::remote::domain::BodyVariant::E90);
    CHECK(profile.firstModelYear == 2009U);
    CHECK(profile.lastModelYear == 2009U);
    CHECK(std::string_view{profile.engineCode} == "N47D20C");
    CHECK(profile.fuel == bmw::remote::domain::FuelType::Diesel);
    CHECK(profile.transmission == Transmission::Automatic);
    CHECK(profile.qualification == bmw::remote::domain::QualificationStage::Discovery);
    CHECK(profile.hoodInterlockSource ==
          bmw::remote::domain::HoodInterlockSource::NotAvailable);

    for (std::size_t index = 0U;
         index < bmw::remote::domain::vehicleSignalCount();
         ++index) {
        const VehicleSignal signal = static_cast<VehicleSignal>(index);
        const SignalSupport expected = signal == VehicleSignal::HoodClosed
                                           ? SignalSupport::Unavailable
                                           : SignalSupport::Candidate;
        CHECK(profile.support(signal) == expected);
    }
}

void testReferenceProfileRegistryFindsOnlyKnownIdentifiers() {
    const auto& registry = bmw::remote::domain::profiles::registry();
    CHECK(registry.size() == 1U);
    CHECK(registry.find("bmw-e90-2009-n47d20c-automatic") != nullptr);
    CHECK(registry.find("bmw-e92-other") == nullptr);
    CHECK(registry.find(nullptr) == nullptr);
}

void testDiscoveryProfileCannotEnableRemoteStart() {
    const auto readiness = bmw::remote::application::assessRemoteStartReadiness(
        bmw::remote::domain::profiles::e90_2009_n47d20c_automatic());

    CHECK(!readiness.ready());
    CHECK(readiness.contains(ProfileReadinessReason::MissingRequiredSignal));
    CHECK(readiness.contains(ProfileReadinessReason::UnverifiedRequiredSignal));
    CHECK(readiness.contains(ProfileReadinessReason::QualificationTooLow));
}

void testValidatedProfileWithVerifiedSignalsIsReady() {
    VehicleProfile profile =
        bmw::remote::domain::profiles::e90_2009_n47d20c_automatic();
    profile.signals.fill(SignalSupport::Verified);
    profile.qualification = bmw::remote::domain::QualificationStage::ReadOnlyValidated;
    profile.hoodInterlockSource = bmw::remote::domain::HoodInterlockSource::VehicleSignal;

    CHECK(bmw::remote::application::assessRemoteStartReadiness(profile).ready());
}

void testValidatedProfileStillFailsClosedWhenSignalIsMissing() {
    VehicleProfile profile =
        bmw::remote::domain::profiles::e90_2009_n47d20c_automatic();
    profile.signals.fill(SignalSupport::Verified);
    profile.qualification = bmw::remote::domain::QualificationStage::ReadOnlyValidated;
    profile.hoodInterlockSource = bmw::remote::domain::HoodInterlockSource::VehicleSignal;
    profile.signals[bmw::remote::domain::signalIndex(VehicleSignal::HoodClosed)] =
        SignalSupport::Unavailable;

    const auto readiness =
        bmw::remote::application::assessRemoteStartReadiness(profile);
    CHECK(!readiness.ready());
    CHECK(readiness.contains(ProfileReadinessReason::MissingRequiredSignal));
}

void testValidatedProfileRejectsUnavailableHoodInterlockWhenRequired() {
    VehicleProfile profile =
        bmw::remote::domain::profiles::e90_2009_n47d20c_automatic();
    profile.signals.fill(SignalSupport::Verified);
    profile.qualification = bmw::remote::domain::QualificationStage::ReadOnlyValidated;
    profile.hoodInterlockSource = bmw::remote::domain::HoodInterlockSource::NotAvailable;

    const auto readiness =
        bmw::remote::application::assessRemoteStartReadiness(profile);
    CHECK(!readiness.ready());
    CHECK(readiness.contains(ProfileReadinessReason::HoodInterlockUnavailable));
}

void testControllerAllowsExplicitOptionalHoodSignal() {
    VehicleProfile profile =
        bmw::remote::domain::profiles::e90_2009_n47d20c_automatic();
    profile.signals.fill(SignalSupport::Verified);
    profile.signals[bmw::remote::domain::signalIndex(VehicleSignal::HoodClosed)] =
        SignalSupport::Unavailable;
    profile.qualification = bmw::remote::domain::QualificationStage::ReadOnlyValidated;
    profile.hoodInterlockSource = bmw::remote::domain::HoodInterlockSource::NotAvailable;

    ControllerConfig config{};
    config.vehicleProfile = &profile;
    config.safety.requireHoodClosed = false;
    Controller controller{config};
    VehicleState vehicle = safeAutomaticVehicle();
    vehicle.hoodClosed = {};

    const auto request = controller.handle(
        Event{EventType::RemoteStartRequested}, vehicle);
    CHECK(request.state == ControllerState::Authorizing);
    CHECK(request.profileReadiness.ready());

    const auto authorized = controller.handle(
        Event{EventType::VehicleStateUpdated}, vehicle);
    CHECK(authorized.state == ControllerState::Preparing);
    CHECK(authorized.safety.approved());
}

void testCanonicalTraceParserLoadsValidClassicFrames() {
    std::istringstream input{
        "timestamp_ms,identifier,extended,dlc,data_hex\n"
        "0,0x123,0,2,0AFF\n"
        "125,0x1FFFFF00,1,1,A5\n"};
    std::vector<CanFrame> frames{};
    std::string error{};

    CHECK(bmw::remote::host::parseCanonicalCanTrace(input, frames, 10U, error));
    CHECK(error.empty());
    CHECK(frames.size() == 2U);
    CHECK(frames[0].timestampMs == 0U);
    CHECK(frames[0].identifier == 0x123U);
    CHECK(frames[0].data[0] == 0x0AU);
    CHECK(frames[0].data[1] == 0xFFU);
    CHECK(frames[1].timestampMs == 125U);
    CHECK(frames[1].extended);
}

void testCanonicalTraceParserRejectsNonMonotonicInputAtomically() {
    std::istringstream input{
        "timestamp_ms,identifier,extended,dlc,data_hex\n"
        "0,0x123,0,1,01\n"
        "99,0x123,0,1,02\n"
        "98,0x123,0,1,03\n"};
    std::vector<CanFrame> frames(1U);
    frames.front().identifier = 0x456U;
    std::string error{};

    CHECK(!bmw::remote::host::parseCanonicalCanTrace(input, frames, 10U, error));
    CHECK(error.find("monotonic") != std::string::npos);
    CHECK(frames.size() == 1U);
    CHECK(frames.front().identifier == 0x456U);
}

void testCanonicalTraceParserRejectsMalformedAndEmptyTraces() {
    std::istringstream malformed{
        "timestamp_ms,identifier,extended,dlc,data_hex\n"
        "0,0x123,0,2,AA\n"};
    std::istringstream empty{
        "timestamp_ms,identifier,extended,dlc,data_hex\n"};
    std::vector<CanFrame> frames{};
    std::string error{};

    CHECK(!bmw::remote::host::parseCanonicalCanTrace(malformed, frames, 10U, error));
    CHECK(error.find("invalid CAN frame") != std::string::npos);
    CHECK(!bmw::remote::host::parseCanonicalCanTrace(empty, frames, 10U, error));
    CHECK(error.find("no CAN frames") != std::string::npos);
    CHECK(frames.empty());
}

using TestFunction = void (*)();

struct TestCase final {
    const char* name;
    TestFunction run;
};

}  // namespace

int main() {
    const TestCase tests[] = {
        {"three-lock gesture", testThreeLockPressesTriggerRemoteStartGesture},
        {"slow lock sequence", testSlowLockSequenceDoesNotTrigger},
        {"lock button debounce", testLockButtonBounceIsIgnored},
        {"default user settings", testDefaultUserSettingsAreValidAndPreserved},
        {"custom user settings", testUserSettingsConfigureHoodTimersEntryAndLocks},
        {"invalid user settings", testUnsafeUserSettingsAreRejectedFailClosed},
        {"remote start disabled", testUserCanDisableRemoteStart},
        {"door opens stop mode", testUserCanStopImmediatelyWhenDoorOpens},
        {"settings file", testUserSettingsFileLoadsStrictConfiguration},
        {"settings file strict keys", testUserSettingsFileRejectsUnknownAndDuplicateKeys},
        {"settings file safety bounds", testUserSettingsFileRejectsUnsafeDurations},
        {"safe automatic vehicle", testSafeAutomaticVehicleIsApproved},
        {"unavailable signal", testUnavailableSignalFailsClosed},
        {"multiple safety reasons", testUnsafeVehicleReportsAllDetectedReasons},
        {"manual denied by default", testManualTransmissionIsDeniedByDefault},
        {"manual explicit opt-in", testManualTransmissionRequiresExplicitOptIn},
        {"optional hood safety policy", testHoodSignalCanBeDisabledByExplicitSafetyPolicy},
        {"missing profile rejection", testControllerRejectsStartWhenNoProfileIsSelected},
        {"unqualified profile rejection", testControllerRejectsUnqualifiedReferenceProfile},
        {"nominal start sequence", testNominalStartSequence},
        {"unsafe start rejection", testUnsafeStartReturnsToIdleWithoutLatchingFault},
        {"duplicate start", testDuplicateStartRequestIsIgnored},
        {"cranking timeout", testCrankingTimeoutLatchesFaultAndSafesOutputs},
        {"running safety violation", testSafetyViolationWhileRunningLatchesFault},
        {"remote stop confirmation", testRemoteStopRequiresStoppedEngineConfirmation},
        {"remote run timeout", testRemoteRunTimerInitiatesStop},
        {"fifteen minute remote run", testRemoteRunIsLimitedToFifteenMinutesByDefault},
        {"driver entry takeover", testDoorOpeningStartsBoundedDriverTakeover},
        {"takeover timeout", testUnconfirmedDriverTakeoverStopsRemoteEngine},
        {"confirmed takeover", testConfirmedDriverTakeoverReleasesRemoteControl},
        {"unsafe driver entry", testUnsafeDriverEntryLatchesSafetyFault},
        {"fault reset guards", testFaultResetRequiresStoppedEngineAndNoCriticalFault},
        {"runtime missing profile", testRuntimeRejectsMissingProfileWithoutRequestingVehicle},
        {"gateway failure", testRuntimeConvertsGatewayFailureIntoSafeFault},
        {"actuator failure", testRuntimeConvertsActuatorFailureIntoSafeFault},
        {"takeover release failure", testRuntimeSafesOutputsWhenTakeoverReleaseFails},
        {"non-monotonic trace", testReplayRejectsNonMonotonicTrace},
        {"time-bounded replay", testReplayOnlyEmitsFramesDueAtCurrentTime},
        {"signal freshness", testAssemblerMarksOldSignalsStale},
        {"malformed synthetic frame", testRecognizedMalformedFrameStopsReplay},
        {"unknown frame", testUnknownFrameIsIgnoredWithoutCreatingSignals},
        {"atomic decoded batch", testInvalidDecodedBatchIsAppliedAtomically},
        {"non-monotonic replay clock", testReplayRejectsClockMovingBackwards},
        {"replayed safety state", testReplayedVehicleStateFeedsSafetyPolicy},
        {"reference profile metadata", testReferenceProfileDescribesReferenceVehicleWithoutClaimingQualification},
        {"reference profile registry", testReferenceProfileRegistryFindsOnlyKnownIdentifiers},
        {"discovery profile readiness", testDiscoveryProfileCannotEnableRemoteStart},
        {"validated profile readiness", testValidatedProfileWithVerifiedSignalsIsReady},
        {"missing profile signal", testValidatedProfileStillFailsClosedWhenSignalIsMissing},
        {"required hood interlock", testValidatedProfileRejectsUnavailableHoodInterlockWhenRequired},
        {"optional hood controller", testControllerAllowsExplicitOptionalHoodSignal},
        {"canonical trace parsing", testCanonicalTraceParserLoadsValidClassicFrames},
        {"canonical trace monotonicity", testCanonicalTraceParserRejectsNonMonotonicInputAtomically},
        {"canonical trace rejection", testCanonicalTraceParserRejectsMalformedAndEmptyTraces},
    };

    for (const TestCase& test : tests) {
        const int failuresBefore = failures;
        test.run();
        std::cout << (failures == failuresBefore ? "PASS " : "FAIL ")
                  << test.name << '\n';
    }

    if (failures != 0) {
        std::cerr << failures << " assertion(s) failed\n";
        return 1;
    }

    std::cout << "All " << (sizeof(tests) / sizeof(tests[0])) << " tests passed\n";
    return 0;
}
