#include <cstdint>
#include <iostream>

#include "bmw_remote/application/controller.hpp"
#include "bmw_remote/application/safety_policy.hpp"
#include "bmw_remote/domain/vehicle_state.hpp"
#include "bmw_remote/infrastructure/runtime.hpp"

namespace {

using bmw::remote::application::ActionType;
using bmw::remote::application::Controller;
using bmw::remote::application::ControllerConfig;
using bmw::remote::application::ControllerState;
using bmw::remote::application::Event;
using bmw::remote::application::EventType;
using bmw::remote::application::FaultCode;
using bmw::remote::application::SafetyPolicy;
using bmw::remote::application::SafetyPolicyConfig;
using bmw::remote::application::SafetyReason;
using bmw::remote::domain::Gear;
using bmw::remote::domain::Observed;
using bmw::remote::domain::SignalQuality;
using bmw::remote::domain::Transmission;
using bmw::remote::domain::VehicleState;
using bmw::remote::infrastructure::ActuatorPort;
using bmw::remote::infrastructure::NotificationSink;
using bmw::remote::infrastructure::Runtime;
using bmw::remote::infrastructure::TimerPort;
using bmw::remote::infrastructure::VehicleGateway;

int failures = 0;

void check(const bool condition, const char* expression, const int line) {
    if (!condition) {
        std::cerr << "FAIL line " << line << ": " << expression << '\n';
        ++failures;
    }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

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

void advanceToRunning(Controller& controller, VehicleState& vehicle) {
    static_cast<void>(controller.handle(Event{EventType::RemoteStartRequested}, vehicle));
    static_cast<void>(controller.handle(Event{EventType::VehicleStateUpdated}, vehicle));
    static_cast<void>(controller.handle(Event{EventType::TimerElapsed}, vehicle));
    vehicle.engineRpm = Observed<std::uint16_t>::fresh(800U);
    static_cast<void>(controller.handle(Event{EventType::VehicleStateUpdated}, vehicle));
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

void testNominalStartSequence() {
    Controller controller{};
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
    Controller controller{};
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
    Controller controller{};
    const VehicleState vehicle = safeAutomaticVehicle();
    static_cast<void>(controller.handle(Event{EventType::RemoteStartRequested}, vehicle));

    const auto duplicate = controller.handle(Event{EventType::RemoteStartRequested}, vehicle);
    CHECK(duplicate.state == ControllerState::Authorizing);
    CHECK(duplicate.contains(ActionType::NotifyRequestIgnored));
    CHECK(!duplicate.contains(ActionType::EngageStarter));
}

void testCrankingTimeoutLatchesFaultAndSafesOutputs() {
    Controller controller{};
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
    Controller controller{};
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
    Controller controller{};
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
    Controller controller{};
    VehicleState vehicle = safeAutomaticVehicle();
    advanceToRunning(controller, vehicle);

    const auto expired = controller.handle(Event{EventType::TimerElapsed}, vehicle);
    CHECK(expired.state == ControllerState::Stopping);
    CHECK(expired.contains(ActionType::SecureOutputs));
    CHECK(expired.contains(ActionType::NotifyStopping));
}

void testFaultResetRequiresStoppedEngineAndNoCriticalFault() {
    Controller controller{};
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
    std::uint32_t secureCalls{0U};
    std::uint32_t starterReleaseCalls{0U};

    bool enableIgnition() noexcept override { return ignitionSucceeds; }
    bool engageStarter() noexcept override { return true; }
    bool disengageStarter() noexcept override {
        ++starterReleaseCalls;
        return true;
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

    void publish(
        const ActionType notification,
        ControllerState,
        FaultCode,
        bmw::remote::application::SafetyAssessment) noexcept override {
        if (notification == ActionType::NotifyFault) {
            ++faultNotifications;
        }
    }
};

void testRuntimeConvertsGatewayFailureIntoSafeFault() {
    FakeVehicleGateway gateway{};
    gateway.succeeds = false;
    FakeActuator actuator{};
    FakeTimer timer{};
    FakeNotifications notifications{};
    Runtime runtime{Controller{}, gateway, actuator, timer, notifications};

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
    Runtime runtime{Controller{}, gateway, actuator, timer, notifications};
    VehicleState vehicle = safeAutomaticVehicle();

    static_cast<void>(runtime.dispatch(Event{EventType::RemoteStartRequested}, vehicle));
    const auto result = runtime.dispatch(Event{EventType::VehicleStateUpdated}, vehicle);

    CHECK(result.state == ControllerState::Fault);
    CHECK(result.fault == FaultCode::ActuatorFailure);
    CHECK(actuator.starterReleaseCalls == 1U);
    CHECK(actuator.secureCalls == 1U);
}

using TestFunction = void (*)();

struct TestCase final {
    const char* name;
    TestFunction run;
};

}  // namespace

int main() {
    const TestCase tests[] = {
        {"safe automatic vehicle", testSafeAutomaticVehicleIsApproved},
        {"unavailable signal", testUnavailableSignalFailsClosed},
        {"multiple safety reasons", testUnsafeVehicleReportsAllDetectedReasons},
        {"manual denied by default", testManualTransmissionIsDeniedByDefault},
        {"manual explicit opt-in", testManualTransmissionRequiresExplicitOptIn},
        {"nominal start sequence", testNominalStartSequence},
        {"unsafe start rejection", testUnsafeStartReturnsToIdleWithoutLatchingFault},
        {"duplicate start", testDuplicateStartRequestIsIgnored},
        {"cranking timeout", testCrankingTimeoutLatchesFaultAndSafesOutputs},
        {"running safety violation", testSafetyViolationWhileRunningLatchesFault},
        {"remote stop confirmation", testRemoteStopRequiresStoppedEngineConfirmation},
        {"remote run timeout", testRemoteRunTimerInitiatesStop},
        {"fault reset guards", testFaultResetRequiresStoppedEngineAndNoCriticalFault},
        {"gateway failure", testRuntimeConvertsGatewayFailureIntoSafeFault},
        {"actuator failure", testRuntimeConvertsActuatorFailureIntoSafeFault},
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
