#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "bmw_remote/application/controller.hpp"
#include "bmw_remote/application/lock_command_gate.hpp"
#include "bmw_remote/application/lock_sequence_detector.hpp"
#include "bmw_remote/application/profile_readiness.hpp"
#include "bmw_remote/application/safety_policy.hpp"
#include "bmw_remote/application/user_settings.hpp"
#include "bmw_remote/domain/reference_profiles.hpp"
#include "bmw_remote/domain/vehicle_profile.hpp"
#include "bmw_remote/domain/vehicle_state.hpp"
#include "bmw_remote/infrastructure/actuator_safety_supervisor.hpp"
#include "bmw_remote/infrastructure/can_lock_command_adapter.hpp"
#include "bmw_remote/infrastructure/replay_vehicle_gateway.hpp"
#include "bmw_remote/infrastructure/runtime.hpp"
#include "bmw_remote/infrastructure/settings_payload.hpp"
#include "bmw_remote/infrastructure/settings_protocol.hpp"
#include "bmw_remote/infrastructure/settings_storage.hpp"
#include "bmw_remote/infrastructure/settings_stream.hpp"
#include "bmw_remote/infrastructure/vehicle_state_assembler.hpp"
#include "bmw_remote/simulation/synthetic_can.hpp"
#include "tools/can_trace_csv.hpp"
#include "tools/sandbox_session.hpp"
#include "tools/settings_device_client.hpp"
#include "tools/user_settings_file.hpp"

namespace {

using bmw::remote::application::ActionType;
using bmw::remote::application::Controller;
using bmw::remote::application::ControllerConfig;
using bmw::remote::application::ControllerState;
using bmw::remote::application::Event;
using bmw::remote::application::EventType;
using bmw::remote::application::FaultCode;
using bmw::remote::application::FeatureCapability;
using bmw::remote::application::FeatureControlClass;
using bmw::remote::application::FeatureExecutionTarget;
using bmw::remote::application::FeatureId;
using bmw::remote::application::FeatureReleaseTier;
using bmw::remote::application::FeatureRequests;
using bmw::remote::application::FeatureResolutionStatus;
using bmw::remote::application::FeatureRuntimeContext;
using bmw::remote::application::HoodMonitoringMode;
using bmw::remote::application::LockCommandDecision;
using bmw::remote::application::LockCommandEvidence;
using bmw::remote::application::LockCommandGate;
using bmw::remote::application::LockCommandGateConfig;
using bmw::remote::application::LockCommandSource;
using bmw::remote::application::LockCommandStatus;
using bmw::remote::application::LockCommandTrust;
using bmw::remote::application::LockSequenceConfig;
using bmw::remote::application::LockSequenceDetector;
using bmw::remote::application::ProfileReadinessReason;
using bmw::remote::application::SafetyPolicy;
using bmw::remote::application::SafetyPolicyConfig;
using bmw::remote::application::SafetyReason;
using bmw::remote::application::TelemetryAlertType;
using bmw::remote::application::TelemetryConditionState;
using bmw::remote::application::TelemetryMonitor;
using bmw::remote::application::TelemetryMonitorConfig;
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
using bmw::remote::infrastructure::ActuatorFeedback;
using bmw::remote::infrastructure::ActuatorPort;
using bmw::remote::infrastructure::ActuatorSafetyConfig;
using bmw::remote::infrastructure::ActuatorSafetySupervisor;
using bmw::remote::infrastructure::ActuatorSupervisorFault;
using bmw::remote::infrastructure::CanBitMatcher;
using bmw::remote::infrastructure::CanCounterField;
using bmw::remote::infrastructure::CanFrame;
using bmw::remote::infrastructure::CanLockCommandAdapter;
using bmw::remote::infrastructure::CanLockCommandAdapterConfig;
using bmw::remote::infrastructure::CanLockCommandPipeline;
using bmw::remote::infrastructure::CanLockDecodeStatus;
using bmw::remote::infrastructure::CanLockPipelineResult;
using bmw::remote::infrastructure::DiagnosticJournal;
using bmw::remote::infrastructure::DiagnosticReason;
using bmw::remote::infrastructure::DiagnosticRecord;
using bmw::remote::infrastructure::DiagnosticRecordType;
using bmw::remote::infrastructure::NotificationSink;
using bmw::remote::infrastructure::ReplayStatus;
using bmw::remote::infrastructure::ReplayVehicleGateway;
using bmw::remote::infrastructure::Runtime;
using bmw::remote::infrastructure::JournaledUserSettingsStore;
using bmw::remote::infrastructure::SettingsByteStorage;
using bmw::remote::infrastructure::VehicleStateAssembler;
using bmw::remote::infrastructure::TimerPort;
using bmw::remote::infrastructure::VehicleGateway;
using bmw::remote::simulation::SyntheticBodyState;
using bmw::remote::simulation::SyntheticCanDecoder;
using bmw::remote::simulation::SyntheticPowertrainState;
using bmw::remote::simulation::SyntheticTelemetryState;
using bmw::remote::simulation::makeSyntheticBodyFrame;
using bmw::remote::simulation::makeSyntheticPowertrainFrame;
using bmw::remote::simulation::makeSyntheticTelemetryFrame;
using bmw::remote::simulation::syntheticVehicleProfile;
using bmw::remote::host::SandboxSession;

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

LockCommandEvidence verifiedVehicleLock(
    const std::uint32_t sequence,
    const std::uint32_t observedAtMs) {
    return LockCommandEvidence{
        LockCommandSource::VehicleAdapter,
        LockCommandTrust::Verified,
        sequence,
        observedAtMs,
        true};
}

CanLockCommandAdapterConfig testCanLockAdapterConfig(
    const LockCommandTrust trust = LockCommandTrust::Verified) {
    CanLockCommandAdapterConfig config{};
    config.enabled = true;
    config.trust = trust;
    config.identifier = 0x321U;
    config.dataLength = 2U;
    config.lockCommand = CanBitMatcher{0U, 0x01U, 0x01U};
    config.vehicleSecured = CanBitMatcher{0U, 0x02U, 0x02U};
    config.rollingCounter = CanCounterField{1U, 0x0FU};
    return config;
}

CanFrame testCanLockFrame(
    const std::uint32_t timestampMs,
    const std::uint8_t counter,
    const bool commandActive,
    const bool vehicleSecured = true) {
    CanFrame frame{};
    frame.timestampMs = timestampMs;
    frame.identifier = 0x321U;
    frame.dataLength = 2U;
    frame.data[0U] = static_cast<std::uint8_t>(
        (commandActive ? 0x01U : 0x00U) |
        (vehicleSecured ? 0x02U : 0x00U));
    frame.data[1U] = static_cast<std::uint8_t>(counter & 0x0FU);
    return frame;
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

void testVerifiedLockEvidenceTriggersOnlyAfterCompleteSequence() {
    LockCommandGate gate{};

    const LockCommandDecision first =
        gate.process(verifiedVehicleLock(10U, 1'000U), 1'000U);
    const LockCommandDecision second =
        gate.process(verifiedVehicleLock(11U, 1'600U), 1'600U);
    const LockCommandDecision third =
        gate.process(verifiedVehicleLock(12U, 2'200U), 2'200U);

    CHECK(first.status == LockCommandStatus::PressAccepted);
    CHECK(first.accepted());
    CHECK(!first.remoteStartRequested());
    CHECK(second.status == LockCommandStatus::PressAccepted);
    CHECK(third.status == LockCommandStatus::RemoteStartRequested);
    CHECK(third.accepted());
    CHECK(third.remoteStartRequested());
    CHECK(gate.pressCount() == 0U);
}

void testLockGateRejectsUntrustedSyntheticAndUnsecuredEvidence() {
    LockCommandGate gate{};
    LockCommandEvidence evidence = verifiedVehicleLock(1U, 1'000U);
    evidence.trust = LockCommandTrust::Candidate;
    CHECK(
        gate.process(evidence, 1'000U).status ==
        LockCommandStatus::RejectedUntrustedSource);

    evidence = verifiedVehicleLock(1U, 1'100U);
    evidence.source = LockCommandSource::SyntheticTest;
    CHECK(
        gate.process(evidence, 1'100U).status ==
        LockCommandStatus::RejectedSyntheticSource);

    evidence = verifiedVehicleLock(1U, 1'200U);
    evidence.vehicleSecured = false;
    CHECK(
        gate.process(evidence, 1'200U).status ==
        LockCommandStatus::RejectedVehicleNotSecured);
    CHECK(gate.pressCount() == 0U);
}

void testLockGateRejectsStaleFutureAndRegressingClocks() {
    LockCommandGate gate{};
    CHECK(
        gate.process(verifiedVehicleLock(1U, 1'000U), 1'501U).status ==
        LockCommandStatus::RejectedStaleEvidence);

    gate.reset();
    CHECK(
        gate.process(verifiedVehicleLock(1U, 2'000U), 1'900U).status ==
        LockCommandStatus::RejectedFutureEvidence);

    gate.reset();
    CHECK(gate.process(verifiedVehicleLock(1U, 3'000U), 3'000U).accepted());
    CHECK(
        gate.process(verifiedVehicleLock(2U, 3'100U), 2'999U).status ==
        LockCommandStatus::RejectedClockRegression);
    CHECK(gate.pressCount() == 0U);
}

void testLockGateRejectsDuplicateOutOfOrderAndNonMonotonicEvidence() {
    LockCommandGate gate{};
    CHECK(gate.process(verifiedVehicleLock(10U, 1'000U), 1'000U).accepted());
    CHECK(
        gate.process(verifiedVehicleLock(10U, 1'100U), 1'100U).status ==
        LockCommandStatus::RejectedDuplicateSequence);
    CHECK(gate.process(verifiedVehicleLock(11U, 1'200U), 1'200U).accepted());
    CHECK(
        gate.process(verifiedVehicleLock(9U, 1'300U), 1'300U).status ==
        LockCommandStatus::RejectedOutOfOrderSequence);
    CHECK(gate.process(verifiedVehicleLock(12U, 1'400U), 1'400U).accepted());
    CHECK(
        gate.process(verifiedVehicleLock(13U, 1'400U), 1'400U).status ==
        LockCommandStatus::RejectedNonMonotonicEvidence);
}

void testLockGateAcceptsCounterAndClockWraparound() {
    LockCommandGateConfig config{};
    config.sequence.requiredPresses = 2U;
    LockCommandGate gate{config};
    constexpr std::uint32_t BeforeWrap =
        std::numeric_limits<std::uint32_t>::max() - 100U;

    CHECK(gate.process(
              verifiedVehicleLock(
                  std::numeric_limits<std::uint32_t>::max(), BeforeWrap),
              BeforeWrap)
              .accepted());
    const LockCommandDecision completed =
        gate.process(verifiedVehicleLock(0U, 50U), 50U);
    CHECK(completed.status == LockCommandStatus::RemoteStartRequested);
}

void testLockGateResetsPartialGestureAfterRejectedEvidence() {
    LockCommandGate gate{};
    CHECK(gate.process(verifiedVehicleLock(1U, 1'000U), 1'000U).accepted());

    LockCommandEvidence rejected = verifiedVehicleLock(2U, 1'600U);
    rejected.trust = LockCommandTrust::Unverified;
    CHECK(
        gate.process(rejected, 1'600U).status ==
        LockCommandStatus::RejectedUntrustedSource);
    CHECK(gate.pressCount() == 0U);

    CHECK(gate.process(verifiedVehicleLock(2U, 1'700U), 1'700U).accepted());
    CHECK(gate.process(verifiedVehicleLock(3U, 2'300U), 2'300U).accepted());
    CHECK(
        gate.process(verifiedVehicleLock(4U, 2'900U), 2'900U).status ==
        LockCommandStatus::RemoteStartRequested);
}

void testLockGateRejectsInvalidConfigurationAndDebouncedEvidence() {
    LockCommandGateConfig invalidConfig{};
    invalidConfig.maximumEvidenceAgeMs = 0U;
    LockCommandGate invalidGate{invalidConfig};
    CHECK(
        invalidGate.process(
            verifiedVehicleLock(1U, 1'000U), 1'000U).status ==
        LockCommandStatus::RejectedInvalidConfiguration);

    invalidConfig.maximumEvidenceAgeMs = 500U;
    invalidConfig.sequence.requiredPresses = 5U;
    invalidConfig.sequence.minimumGapMs = 1'000U;
    invalidConfig.sequence.maximumGapMs = 1'000U;
    invalidConfig.sequence.maximumSequenceMs = 3'000U;
    LockCommandGate impossibleSequenceGate{invalidConfig};
    CHECK(
        impossibleSequenceGate.process(
            verifiedVehicleLock(1U, 1'000U), 1'000U).status ==
        LockCommandStatus::RejectedInvalidConfiguration);

    LockCommandGate gate{};
    CHECK(gate.process(verifiedVehicleLock(1U, 2'000U), 2'000U).accepted());
    CHECK(
        gate.process(verifiedVehicleLock(2U, 2'040U), 2'040U).status ==
        LockCommandStatus::RejectedTiming);
    CHECK(gate.pressCount() == 1U);
    CHECK(gate.process(verifiedVehicleLock(3U, 2'600U), 2'600U).accepted());
    CHECK(
        gate.process(verifiedVehicleLock(4U, 3'200U), 3'200U).status ==
        LockCommandStatus::RemoteStartRequested);
}

void testQualifiedCanLockPipelineRecognizesThreeEdgesAcrossCounterWrap() {
    CanLockCommandPipeline pipeline{testCanLockAdapterConfig()};

    CHECK(
        pipeline.process(testCanLockFrame(0U, 14U, false), 0U).decodeStatus ==
        CanLockDecodeStatus::Primed);
    const CanLockPipelineResult first =
        pipeline.process(testCanLockFrame(1'000U, 15U, true), 1'000U);
    CHECK(first.commandEvaluated);
    CHECK(first.command.status == LockCommandStatus::PressAccepted);

    CHECK(
        pipeline.process(testCanLockFrame(1'100U, 0U, false), 1'100U)
            .decodeStatus == CanLockDecodeStatus::NoCommand);
    const CanLockPipelineResult second =
        pipeline.process(testCanLockFrame(1'600U, 1U, true), 1'600U);
    CHECK(second.command.status == LockCommandStatus::PressAccepted);

    CHECK(
        pipeline.process(testCanLockFrame(1'700U, 2U, false), 1'700U)
            .decodeStatus == CanLockDecodeStatus::NoCommand);
    const CanLockPipelineResult third =
        pipeline.process(testCanLockFrame(2'200U, 3U, true), 2'200U);
    CHECK(third.command.status == LockCommandStatus::RemoteStartRequested);
    CHECK(third.command.remoteStartRequested());
}

void testCanLockAdapterRequiresRisingEdges() {
    CanLockCommandPipeline pipeline{testCanLockAdapterConfig()};
    static_cast<void>(
        pipeline.process(testCanLockFrame(0U, 0U, false), 0U));

    CHECK(
        pipeline.process(testCanLockFrame(1'000U, 1U, true), 1'000U)
            .command.status == LockCommandStatus::PressAccepted);
    const CanLockPipelineResult held =
        pipeline.process(testCanLockFrame(1'100U, 2U, true), 1'100U);
    CHECK(held.decodeStatus == CanLockDecodeStatus::NoCommand);
    CHECK(!held.commandEvaluated);
    CHECK(pipeline.pressCount() == 1U);

    static_cast<void>(
        pipeline.process(testCanLockFrame(1'200U, 3U, true), 1'200U));
    static_cast<void>(
        pipeline.process(testCanLockFrame(1'300U, 4U, false), 1'300U));
    const CanLockPipelineResult secondEdge =
        pipeline.process(testCanLockFrame(1'600U, 5U, true), 1'600U);
    CHECK(secondEdge.command.status == LockCommandStatus::PressAccepted);
    CHECK(pipeline.pressCount() == 2U);
}

void testCanLockPipelinePreservesCandidateAndSecuredTrustBoundaries() {
    CanLockCommandPipeline candidatePipeline{
        testCanLockAdapterConfig(LockCommandTrust::Candidate)};
    static_cast<void>(candidatePipeline.process(
        testCanLockFrame(0U, 0U, false), 0U));
    const CanLockPipelineResult candidate = candidatePipeline.process(
        testCanLockFrame(1'000U, 1U, true), 1'000U);
    CHECK(candidate.commandEvaluated);
    CHECK(
        candidate.command.status ==
        LockCommandStatus::RejectedUntrustedSource);

    CanLockCommandPipeline unsecuredPipeline{testCanLockAdapterConfig()};
    static_cast<void>(unsecuredPipeline.process(
        testCanLockFrame(0U, 0U, false), 0U));
    const CanLockPipelineResult unsecured = unsecuredPipeline.process(
        testCanLockFrame(1'000U, 1U, true, false), 1'000U);
    CHECK(unsecured.commandEvaluated);
    CHECK(
        unsecured.command.status ==
        LockCommandStatus::RejectedVehicleNotSecured);
}

void testCanLockPipelineResetsGestureAfterStructuralFrameRejection() {
    CanLockCommandPipeline pipeline{testCanLockAdapterConfig()};
    static_cast<void>(
        pipeline.process(testCanLockFrame(0U, 0U, false), 0U));
    static_cast<void>(
        pipeline.process(testCanLockFrame(1'000U, 1U, true), 1'000U));
    CHECK(pipeline.pressCount() == 1U);

    CanFrame unrelated = testCanLockFrame(1'050U, 1U, true);
    unrelated.identifier = 0x123U;
    CHECK(
        pipeline.process(unrelated, 1'050U).decodeStatus ==
        CanLockDecodeStatus::Ignored);
    CHECK(pipeline.pressCount() == 1U);

    const CanLockPipelineResult replay =
        pipeline.process(testCanLockFrame(1'100U, 1U, true), 1'100U);
    CHECK(
        replay.decodeStatus ==
        CanLockDecodeStatus::RejectedDuplicateCounter);
    CHECK(!replay.commandEvaluated);
    CHECK(pipeline.pressCount() == 0U);

    CanFrame malformed = testCanLockFrame(1'200U, 2U, false);
    malformed.dataLength = 1U;
    CHECK(
        pipeline.process(malformed, 1'200U).decodeStatus ==
        CanLockDecodeStatus::RejectedInvalidFrame);
    CHECK(pipeline.pressCount() == 0U);
}

void testCanLockAdapterRejectsInvalidBindingsAndCounterAnomalies() {
    CanLockCommandAdapter disabled{};
    CHECK(
        disabled.process(testCanLockFrame(0U, 0U, false)).status ==
        CanLockDecodeStatus::Disabled);

    CanLockCommandAdapter activeAtBoot{testCanLockAdapterConfig()};
    const auto primedActive =
        activeAtBoot.process(testCanLockFrame(0U, 0U, true));
    CHECK(primedActive.status == CanLockDecodeStatus::Primed);
    CHECK(!primedActive.hasEvidence());

    CanLockCommandAdapterConfig invalidConfig = testCanLockAdapterConfig();
    invalidConfig.rollingCounter.mask = 0x01U;
    CanLockCommandAdapter invalid{invalidConfig};
    CHECK(
        invalid.process(testCanLockFrame(0U, 0U, false)).status ==
        CanLockDecodeStatus::RejectedInvalidConfiguration);

    invalidConfig = testCanLockAdapterConfig();
    invalidConfig.rollingCounter = CanCounterField{0U, 0x03U};
    CanLockCommandAdapter overlappingFields{invalidConfig};
    CHECK(
        overlappingFields.process(testCanLockFrame(0U, 0U, false)).status ==
        CanLockDecodeStatus::RejectedInvalidConfiguration);

    invalidConfig = testCanLockAdapterConfig();
    invalidConfig.vehicleSecured = CanBitMatcher{0U, 0x01U, 0x01U};
    CanLockCommandAdapter overlappingPredicates{invalidConfig};
    CHECK(
        overlappingPredicates.process(testCanLockFrame(0U, 0U, false)).status ==
        CanLockDecodeStatus::RejectedInvalidConfiguration);

    CanLockCommandAdapter counterAdapter{testCanLockAdapterConfig()};
    CHECK(
        counterAdapter.process(testCanLockFrame(1'000U, 5U, false)).status ==
        CanLockDecodeStatus::Primed);
    CHECK(
        counterAdapter.process(testCanLockFrame(1'100U, 4U, false)).status ==
        CanLockDecodeStatus::RejectedOutOfOrderCounter);
    CHECK(
        counterAdapter.process(testCanLockFrame(2'000U, 6U, false)).status ==
        CanLockDecodeStatus::Primed);
    CHECK(
        counterAdapter.process(testCanLockFrame(1'999U, 7U, false)).status ==
        CanLockDecodeStatus::RejectedTimestampRegression);

    CanCounterField sparseCounter{0U, 0xA0U};
    CanFrame sparseFrame{};
    sparseFrame.dataLength = 1U;
    sparseFrame.data[0U] = 0x80U;
    CHECK(sparseCounter.bitCount() == 2U);
    CHECK(sparseCounter.extract(sparseFrame) == 2U);
}

void testFeatureCatalogHasStableCompleteIdentifiers() {
    const auto& catalog = bmw::remote::application::featureCatalog();
    CHECK(catalog.size() == 43U);

    for (std::size_t index = 0U; index < catalog.size(); ++index) {
        const auto& descriptor = catalog[index];
        CHECK(static_cast<std::size_t>(descriptor.id) == index);
        CHECK(descriptor.code != nullptr && descriptor.code[0] != '\0');
        CHECK(bmw::remote::application::findFeature(descriptor.id) ==
              &descriptor);
        CHECK(bmw::remote::application::findFeature(descriptor.code) ==
              &descriptor);
        for (std::size_t other = index + 1U; other < catalog.size(); ++other) {
            CHECK(std::string_view{descriptor.code} != catalog[other].code);
        }
    }

    CHECK(bmw::remote::application::findFeature("not_a_feature") == nullptr);
    CHECK(bmw::remote::application::findFeature(
              static_cast<FeatureId>(255U)) == nullptr);
}

void testFeatureRequestsDefaultOffAndRejectUnknownBits() {
    FeatureRequests requests{};
    CHECK(requests.mask() == 0U);
    CHECK(requests.valid());
    for (const auto& feature : bmw::remote::application::featureCatalog()) {
        CHECK(!requests.enabled(feature.id));
    }

    CHECK(requests.setEnabled(FeatureId::ColdEngineGuard, true));
    CHECK(requests.enabled(FeatureId::ColdEngineGuard));
    CHECK(requests.setEnabled(FeatureId::ColdEngineGuard, false));
    CHECK(!requests.enabled(FeatureId::ColdEngineGuard));
    CHECK(!requests.setEnabled(static_cast<FeatureId>(255U), true));
    CHECK(!FeatureRequests{std::uint64_t{1U} << 63U}.valid());
}

void testFeatureResolverSeparatesRequestCapabilityAndQualification() {
    FeatureRequests requests{};
    CHECK(requests.setEnabled(FeatureId::ColdEngineGuard, true));
    FeatureRuntimeContext context{};
    context.implementedFeatures = requests.mask();

    CHECK(bmw::remote::application::resolveFeature(
              FeatureRequests{}, FeatureId::ColdEngineGuard, context)
              .status == FeatureResolutionStatus::DisabledByUser);

    context.implementedFeatures = 0U;
    CHECK(bmw::remote::application::resolveFeature(
              requests, FeatureId::ColdEngineGuard, context)
              .status == FeatureResolutionStatus::NotImplemented);

    context.implementedFeatures = requests.mask();
    const auto missing = bmw::remote::application::resolveFeature(
        requests, FeatureId::ColdEngineGuard, context);
    CHECK(missing.status == FeatureResolutionStatus::MissingCapabilities);
    CHECK((missing.missingCapabilities &
           bmw::remote::application::featureCapabilityMask(
               FeatureCapability::VehicleStateRead)) != 0U);

    context.availableCapabilities =
        bmw::remote::application::featureCapabilityMask(
            FeatureCapability::VehicleStateRead);
    CHECK(bmw::remote::application::resolveFeature(
              requests, FeatureId::ColdEngineGuard, context)
              .status == FeatureResolutionStatus::SignalsUnqualified);

    context.vehicleSignalsQualified = true;
    CHECK(bmw::remote::application::resolveFeature(
              requests, FeatureId::ColdEngineGuard, context)
              .status == FeatureResolutionStatus::Available);
    context.target = FeatureExecutionTarget::Simulation;
    CHECK(bmw::remote::application::resolveFeature(
              requests, FeatureId::ColdEngineGuard, context)
              .status == FeatureResolutionStatus::Simulated);
}

void testFeatureResolverGatesWritesAndSupportsBothPhonePlatforms() {
    FeatureRequests requests{};
    CHECK(requests.setEnabled(FeatureId::NeedleSweep, true));
    FeatureRuntimeContext context{};
    context.implementedFeatures = requests.mask();
    context.availableCapabilities =
        bmw::remote::application::featureCapabilityMask(
            FeatureCapability::BodyBusWrite);
    CHECK(bmw::remote::application::resolveFeature(
              requests, FeatureId::NeedleSweep, context)
              .status == FeatureResolutionStatus::ComfortWritesUnqualified);
    context.comfortWritesQualified = true;
    CHECK(bmw::remote::application::resolveFeature(
              requests, FeatureId::NeedleSweep, context)
              .effective());

    requests = {};
    CHECK(requests.setEnabled(FeatureId::ForcedDpfRegeneration, true));
    context = {};
    context.implementedFeatures = requests.mask();
    context.availableCapabilities =
        bmw::remote::application::featureCapabilityMask(
            FeatureCapability::SteeringWheelInput) |
        bmw::remote::application::featureCapabilityMask(
            FeatureCapability::PowertrainBusWrite);
    context.vehicleSignalsQualified = true;
    CHECK(bmw::remote::application::resolveFeature(
              requests, FeatureId::ForcedDpfRegeneration, context)
              .status == FeatureResolutionStatus::CriticalControlBlocked);
    context.criticalControlsQualified = true;
    CHECK(bmw::remote::application::resolveFeature(
              requests, FeatureId::ForcedDpfRegeneration, context)
              .status == FeatureResolutionStatus::CriticalControlBlocked);
    context.target = FeatureExecutionTarget::Simulation;
    CHECK(bmw::remote::application::resolveFeature(
              requests, FeatureId::ForcedDpfRegeneration, context)
              .status == FeatureResolutionStatus::Simulated);

    requests = {};
    CHECK(requests.setEnabled(FeatureId::SmartphoneVoiceAssistant, true));
    context = {};
    context.implementedFeatures = requests.mask();
    const std::uint32_t commonPhoneCapabilities =
        bmw::remote::application::featureCapabilityMask(
            FeatureCapability::BleRadio) |
        bmw::remote::application::featureCapabilityMask(
            FeatureCapability::SteeringWheelInput);
    context.availableCapabilities = commonPhoneCapabilities |
        bmw::remote::application::featureCapabilityMask(
            FeatureCapability::IosCompanion);
    CHECK(bmw::remote::application::resolveFeature(
              requests, FeatureId::SmartphoneVoiceAssistant, context)
              .effective());
    context.availableCapabilities = commonPhoneCapabilities |
        bmw::remote::application::featureCapabilityMask(
            FeatureCapability::AndroidCompanion);
    CHECK(bmw::remote::application::resolveFeature(
              requests, FeatureId::SmartphoneVoiceAssistant, context)
              .effective());
}

TelemetryMonitorConfig simulatedTelemetryConfig() {
    TelemetryMonitorConfig config{};
    CHECK(config.requestedFeatures.setEnabled(FeatureId::ColdEngineGuard, true));
    CHECK(config.requestedFeatures.setEnabled(
        FeatureId::DpfRegenerationIndicator, true));
    CHECK(config.requestedFeatures.setEnabled(
        FeatureId::TransmissionOverheatAlert, true));
    config.runtime.target = FeatureExecutionTarget::Simulation;
    config.runtime.implementedFeatures = config.requestedFeatures.mask();
    config.runtime.availableCapabilities =
        bmw::remote::application::featureCapabilityMask(
            FeatureCapability::VehicleStateRead);
    config.runtime.vehicleSignalsQualified = true;
    return config;
}

void testTelemetryMonitorIsReadOnlyFeatureGatedAndEdgeTriggered() {
    VehicleState vehicle = safeAutomaticVehicle();
    vehicle.engineRpm = Observed<std::uint16_t>::fresh(2'500U);
    vehicle.coolantTemperatureC = Observed<std::int16_t>::fresh(60);
    vehicle.engineOilTemperatureC = Observed<std::int16_t>::fresh(55);
    vehicle.transmissionOilTemperatureC = Observed<std::int16_t>::fresh(111);
    vehicle.dpfRegenerationActive = Observed<bool>::fresh(true);

    TelemetryMonitor disabled{};
    const auto disabledReport = disabled.evaluate(vehicle);
    CHECK(disabledReport.coldEngineGuard == TelemetryConditionState::Disabled);
    CHECK(disabledReport.dpfRegeneration == TelemetryConditionState::Disabled);
    CHECK(disabledReport.transmissionOverheat ==
          TelemetryConditionState::Disabled);
    CHECK(disabledReport.alertCount == 0U);

    TelemetryMonitor monitor{simulatedTelemetryConfig()};
    const auto active = monitor.evaluate(vehicle);
    CHECK(active.coldEngineGuard == TelemetryConditionState::Active);
    CHECK(active.dpfRegeneration == TelemetryConditionState::Active);
    CHECK(active.transmissionOverheat == TelemetryConditionState::Active);
    CHECK(active.contains(TelemetryAlertType::ColdEngineHighRpm));
    CHECK(active.contains(TelemetryAlertType::DpfRegenerationStarted));
    CHECK(active.contains(TelemetryAlertType::TransmissionOverheat));

    const auto repeated = monitor.evaluate(vehicle);
    CHECK(repeated.alertCount == 0U);

    vehicle.engineRpm = Observed<std::uint16_t>::fresh(1'800U);
    vehicle.dpfRegenerationActive = Observed<bool>::fresh(false);
    vehicle.transmissionOilTemperatureC = Observed<std::int16_t>::fresh(104);
    const auto recovered = monitor.evaluate(vehicle);
    CHECK(recovered.coldEngineGuard == TelemetryConditionState::Normal);
    CHECK(recovered.dpfRegeneration == TelemetryConditionState::Normal);
    CHECK(recovered.transmissionOverheat == TelemetryConditionState::Normal);
    CHECK(recovered.contains(TelemetryAlertType::ColdEngineRecovered));
    CHECK(recovered.contains(TelemetryAlertType::DpfRegenerationStopped));
    CHECK(recovered.contains(
        TelemetryAlertType::TransmissionTemperatureRecovered));
}

void testTelemetryMonitorRequiresFreshSignalsAndUsesHysteresis() {
    TelemetryMonitor monitor{simulatedTelemetryConfig()};
    VehicleState vehicle = safeAutomaticVehicle();
    vehicle.engineRpm = Observed<std::uint16_t>::fresh(3'000U);
    vehicle.coolantTemperatureC = {};
    vehicle.engineOilTemperatureC = {};
    vehicle.transmissionOilTemperatureC = {};
    vehicle.dpfRegenerationActive = {};

    const auto unavailable = monitor.evaluate(vehicle);
    CHECK(unavailable.coldEngineGuard == TelemetryConditionState::Unavailable);
    CHECK(unavailable.dpfRegeneration == TelemetryConditionState::Unavailable);
    CHECK(unavailable.transmissionOverheat ==
          TelemetryConditionState::Unavailable);
    CHECK(unavailable.alertCount == 0U);

    vehicle.coolantTemperatureC = Observed<std::int16_t>::fresh(80);
    vehicle.engineOilTemperatureC = Observed<std::int16_t>::fresh(80);
    vehicle.transmissionOilTemperatureC = Observed<std::int16_t>::fresh(111);
    vehicle.dpfRegenerationActive = Observed<bool>::fresh(false);
    CHECK(monitor.evaluate(vehicle).transmissionOverheat ==
          TelemetryConditionState::Active);
    vehicle.transmissionOilTemperatureC = Observed<std::int16_t>::fresh(108);
    CHECK(monitor.evaluate(vehicle).transmissionOverheat ==
          TelemetryConditionState::Active);
    vehicle.transmissionOilTemperatureC = Observed<std::int16_t>::fresh(104);
    CHECK(monitor.evaluate(vehicle).transmissionOverheat ==
          TelemetryConditionState::Normal);

    TelemetryMonitorConfig invalidConfig = simulatedTelemetryConfig();
    invalidConfig.temperatureHysteresisC = 0U;
    TelemetryMonitor invalidMonitor{invalidConfig};
    CHECK(invalidMonitor.evaluate(vehicle).transmissionOverheat ==
          TelemetryConditionState::Unavailable);
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
    CHECK(configuration.telemetry.coldEngineMaximumRpm == 2'200U);
    CHECK(configuration.telemetry.engineWarmTemperatureC == 75);
    CHECK(configuration.telemetry.transmissionOverheatTemperatureC == 110);
    CHECK(configuration.telemetry.temperatureHysteresisC == 5U);
    CHECK(settings.features.mask() == 0U);
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
    settings.coldEngineMaximumRpm = 2'500U;
    settings.engineWarmTemperatureC = 80U;
    settings.transmissionOverheatTemperatureC = 120U;
    settings.temperatureAlertHysteresisC = 8U;
    CHECK(settings.features.setEnabled(FeatureId::ColdEngineGuard, true));

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
    CHECK(configuration.telemetry.requestedFeatures.enabled(
        FeatureId::ColdEngineGuard));
    CHECK(configuration.telemetry.coldEngineMaximumRpm == 2'500U);
    CHECK(configuration.telemetry.engineWarmTemperatureC == 80);
    CHECK(configuration.telemetry.transmissionOverheatTemperatureC == 120);
    CHECK(configuration.telemetry.temperatureHysteresisC == 8U);
}

void testUnsafeUserSettingsAreRejectedFailClosed() {
    UserSettings settings{};
    settings.maximumRemoteRunTimeMs = 24U * 60U * 60U * 1'000U;
    settings.driverTakeoverTimeoutMs = 1'000U;
    settings.lockPressCount = 1U;
    settings.lockMinimumGapMs = 2'000U;
    settings.lockMaximumGapMs = 500U;
    settings.lockMaximumSequenceMs = 400U;
    settings.coldEngineMaximumRpm = 500U;
    settings.engineWarmTemperatureC = 20U;
    settings.transmissionOverheatTemperatureC = 200U;
    settings.temperatureAlertHysteresisC = 0U;

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
    CHECK(configuration.validation.contains(
        UserSettingsReason::ColdEngineMaximumRpmOutOfRange));
    CHECK(configuration.validation.contains(
        UserSettingsReason::EngineWarmTemperatureOutOfRange));
    CHECK(configuration.validation.contains(
        UserSettingsReason::TransmissionOverheatTemperatureOutOfRange));
    CHECK(configuration.validation.contains(
        UserSettingsReason::TemperatureAlertHysteresisOutOfRange));
    CHECK(!configuration.controller.remoteStartEnabled);
}

void testUserSettingsRejectUnknownFeatureBits() {
    UserSettings settings{};
    settings.features = FeatureRequests{std::uint64_t{1U} << 63U};
    const auto validation =
        bmw::remote::application::validateUserSettings(settings);
    CHECK(!validation.valid());
    CHECK(validation.contains(UserSettingsReason::InvalidFeatureMask));
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

void testUserSettingsFileParsesIndependentFeatureToggles() {
    std::istringstream input{
        "feature.cold_engine_guard=true\n"
        "feature.virtual_obd_ble=true\n"
        "feature.forced_dpf_regeneration=false\n"};
    UserSettings settings{};
    std::string error{};

    CHECK(bmw::remote::host::parseUserSettings(input, settings, error));
    CHECK(error.empty());
    CHECK(settings.features.enabled(FeatureId::ColdEngineGuard));
    CHECK(settings.features.enabled(FeatureId::VirtualObdBle));
    CHECK(!settings.features.enabled(FeatureId::ForcedDpfRegeneration));
    CHECK(!settings.features.enabled(FeatureId::AutomaticHotspot));

    std::istringstream unknown{"feature.unknown_future_option=true\n"};
    CHECK(!bmw::remote::host::parseUserSettings(unknown, settings, error));
    CHECK(error.find("unknown setting") != std::string::npos);
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

void testUserSettingsFileWriterRoundTripsEverySetting() {
    UserSettings original{};
    original.remoteStartEnabled = false;
    original.hoodMonitoring = HoodMonitoringMode::Disabled;
    original.driverEntryMode =
        bmw::remote::application::DriverEntryMode::StopImmediately;
    original.maximumRemoteRunTimeMs = 42U * 60U * 1'000U;
    original.driverTakeoverTimeoutMs = 180'000U;
    original.lockPressCount = 5U;
    original.lockMinimumGapMs = 120U;
    original.lockMaximumGapMs = 2'200U;
    original.lockMaximumSequenceMs = 9'000U;
    original.coldEngineMaximumRpm = 2'600U;
    original.engineWarmTemperatureC = 82U;
    original.transmissionOverheatTemperatureC = 118U;
    original.temperatureAlertHysteresisC = 7U;
    CHECK(original.features.setEnabled(FeatureId::ColdEngineGuard, true));
    CHECK(original.features.setEnabled(FeatureId::VirtualObdBle, true));
    std::ostringstream output{};
    std::string error{};

    CHECK(bmw::remote::host::writeUserSettings(output, original, error));
    CHECK(error.empty());

    UserSettings loaded{};
    std::istringstream input{output.str()};
    CHECK(bmw::remote::host::parseUserSettings(input, loaded, error));
    CHECK(loaded.remoteStartEnabled == original.remoteStartEnabled);
    CHECK(loaded.hoodMonitoring == original.hoodMonitoring);
    CHECK(loaded.driverEntryMode == original.driverEntryMode);
    CHECK(loaded.maximumRemoteRunTimeMs == original.maximumRemoteRunTimeMs);
    CHECK(loaded.driverTakeoverTimeoutMs == original.driverTakeoverTimeoutMs);
    CHECK(loaded.lockPressCount == original.lockPressCount);
    CHECK(loaded.lockMinimumGapMs == original.lockMinimumGapMs);
    CHECK(loaded.lockMaximumGapMs == original.lockMaximumGapMs);
    CHECK(loaded.lockMaximumSequenceMs == original.lockMaximumSequenceMs);
    CHECK(loaded.features.mask() == original.features.mask());
    CHECK(loaded.coldEngineMaximumRpm == original.coldEngineMaximumRpm);
    CHECK(loaded.engineWarmTemperatureC == original.engineWarmTemperatureC);
    CHECK(loaded.transmissionOverheatTemperatureC ==
          original.transmissionOverheatTemperatureC);
    CHECK(loaded.temperatureAlertHysteresisC ==
          original.temperatureAlertHysteresisC);
    CHECK(output.str().find("feature.cold_engine_guard=true") !=
          std::string::npos);
    CHECK(output.str().find("feature.forced_dpf_regeneration=false") !=
          std::string::npos);
}

void testUserSettingsFileWriterRejectsPrecisionLoss() {
    UserSettings settings{};
    settings.maximumRemoteRunTimeMs = 60'001U;
    std::ostringstream output{};
    std::string error{};

    CHECK(!bmw::remote::host::writeUserSettings(output, settings, error));
    CHECK(output.str().empty());
    CHECK(error.find("whole minutes") != std::string::npos);
}

void testUserSettingsFileSaveReplacesOnlyWithValidatedContent() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        "bmw_remote_user_settings_test.conf";
    std::error_code ignored{};
    static_cast<void>(std::filesystem::remove(path, ignored));
    static_cast<void>(std::filesystem::remove(path.string() + ".tmp", ignored));
    static_cast<void>(std::filesystem::remove(path.string() + ".bak", ignored));

    UserSettings first{};
    first.hoodMonitoring = HoodMonitoringMode::Disabled;
    first.maximumRemoteRunTimeMs = 12U * 60U * 1'000U;
    UserSettings second = first;
    second.maximumRemoteRunTimeMs = 35U * 60U * 1'000U;
    std::string error{};
    const std::string pathText = path.string();

    CHECK(bmw::remote::host::saveUserSettingsFile(
        pathText.c_str(), first, error));
    CHECK(bmw::remote::host::saveUserSettingsFile(
        pathText.c_str(), second, error));

    UserSettings loaded{};
    CHECK(bmw::remote::host::loadUserSettingsFile(
        pathText.c_str(), loaded, error));
    CHECK(loaded.maximumRemoteRunTimeMs == second.maximumRemoteRunTimeMs);
    CHECK(loaded.hoodMonitoring == HoodMonitoringMode::Disabled);
    CHECK(!std::filesystem::exists(path.string() + ".tmp"));
    CHECK(!std::filesystem::exists(path.string() + ".bak"));

    UserSettings invalid = second;
    invalid.maximumRemoteRunTimeMs = 0U;
    CHECK(!bmw::remote::host::saveUserSettingsFile(
        pathText.c_str(), invalid, error));
    CHECK(bmw::remote::host::loadUserSettingsFile(
        pathText.c_str(), loaded, error));
    CHECK(loaded.maximumRemoteRunTimeMs == second.maximumRemoteRunTimeMs);

    ignored.clear();
    static_cast<void>(std::filesystem::remove(path, ignored));
}

struct MemorySettingsStorage final : SettingsByteStorage {
    std::array<std::uint8_t, JournaledUserSettingsStore::RequiredCapacity> bytes{};
    bool readSucceeds{true};
    bool writeSucceeds{true};
    bool commitSucceeds{true};
    std::size_t partialWriteBytes{0U};
    std::uint32_t writeCalls{0U};
    std::uint32_t commitCalls{0U};

    MemorySettingsStorage() {
        bytes.fill(0xFFU);
    }

    [[nodiscard]] std::size_t capacity() const noexcept override {
        return bytes.size();
    }

    bool read(
        const std::size_t offset,
        std::uint8_t* const destination,
        const std::size_t size) noexcept override {
        if (!readSucceeds || destination == nullptr ||
            offset > bytes.size() || size > bytes.size() - offset) {
            return false;
        }
        for (std::size_t index = 0U; index < size; ++index) {
            destination[index] = bytes[offset + index];
        }
        return true;
    }

    bool write(
        const std::size_t offset,
        const std::uint8_t* const source,
        const std::size_t size) noexcept override {
        ++writeCalls;
        if (source == nullptr || offset > bytes.size() ||
            size > bytes.size() - offset) {
            return false;
        }

        const std::size_t copied = writeSucceeds
                                       ? size
                                       : (partialWriteBytes < size
                                              ? partialWriteBytes
                                              : size);
        for (std::size_t index = 0U; index < copied; ++index) {
            bytes[offset + index] = source[index];
        }
        return writeSucceeds;
    }

    bool commit() noexcept override {
        ++commitCalls;
        return commitSucceeds;
    }
};

void writeTestU16(
    std::uint8_t* const destination,
    const std::uint16_t value) noexcept {
    destination[0] = static_cast<std::uint8_t>(value & 0xFFU);
    destination[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

void writeTestU32(
    std::uint8_t* const destination,
    const std::uint32_t value) noexcept {
    destination[0] = static_cast<std::uint8_t>(value & 0xFFU);
    destination[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    destination[2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    destination[3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

std::uint32_t testSettingsCrc32(
    const std::uint8_t* const data,
    const std::size_t size) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t index = 0U; index < size; ++index) {
        crc ^= data[index];
        for (std::uint8_t bit = 0U; bit < 8U; ++bit) {
            const std::uint32_t mask =
                static_cast<std::uint32_t>(0U - (crc & 1U));
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

void seedLegacySettingsRecord(
    MemorySettingsStorage& storage,
    const UserSettings& settings) {
    constexpr std::size_t PayloadOffset = 12U;
    constexpr std::size_t CrcOffset =
        PayloadOffset +
        bmw::remote::infrastructure::LegacyUserSettingsPayloadSize;
    bmw::remote::infrastructure::UserSettingsPayload payload{};
    CHECK(bmw::remote::infrastructure::encodeUserSettingsPayload(
        settings, payload));

    storage.bytes.fill(0xFFU);
    storage.bytes[0U] = 'B';
    storage.bytes[1U] = 'M';
    storage.bytes[2U] = 'R';
    storage.bytes[3U] = 'C';
    writeTestU16(storage.bytes.data() + 4U, 1U);
    writeTestU16(
        storage.bytes.data() + 6U,
        static_cast<std::uint16_t>(
            bmw::remote::infrastructure::LegacyUserSettingsPayloadSize));
    writeTestU32(storage.bytes.data() + 8U, 7U);
    for (std::size_t index = 0U;
         index < bmw::remote::infrastructure::LegacyUserSettingsPayloadSize;
         ++index) {
        storage.bytes[PayloadOffset + index] = payload[index];
    }
    writeTestU32(
        storage.bytes.data() + CrcOffset,
        testSettingsCrc32(storage.bytes.data(), CrcOffset));
}

void seedFeatureSettingsRecord(
    MemorySettingsStorage& storage,
    const UserSettings& settings) {
    constexpr std::size_t PayloadOffset = 12U;
    constexpr std::size_t CrcOffset =
        PayloadOffset +
        bmw::remote::infrastructure::FeatureUserSettingsPayloadSize;
    bmw::remote::infrastructure::UserSettingsPayload payload{};
    CHECK(bmw::remote::infrastructure::encodeUserSettingsPayload(
        settings, payload));

    storage.bytes.fill(0xFFU);
    storage.bytes[0U] = 'B';
    storage.bytes[1U] = 'M';
    storage.bytes[2U] = 'R';
    storage.bytes[3U] = 'C';
    writeTestU16(storage.bytes.data() + 4U, 2U);
    writeTestU16(
        storage.bytes.data() + 6U,
        static_cast<std::uint16_t>(
            bmw::remote::infrastructure::FeatureUserSettingsPayloadSize));
    writeTestU32(storage.bytes.data() + 8U, 8U);
    for (std::size_t index = 0U;
         index < bmw::remote::infrastructure::FeatureUserSettingsPayloadSize;
         ++index) {
        storage.bytes[PayloadOffset + index] = payload[index];
    }
    writeTestU32(
        storage.bytes.data() + CrcOffset,
        testSettingsCrc32(storage.bytes.data(), CrcOffset));
}

void testEmptySettingsStorageDisablesRemoteStart() {
    MemorySettingsStorage storage{};
    JournaledUserSettingsStore store{storage};
    UserSettings settings{};

    CHECK(!bmw::remote::infrastructure::loadUserSettingsFailSafe(store, settings));
    CHECK(!settings.remoteStartEnabled);
}

void testJournaledSettingsRoundTripUsesLatestGeneration() {
    MemorySettingsStorage storage{};
    JournaledUserSettingsStore store{storage};
    UserSettings first{};
    first.hoodMonitoring = HoodMonitoringMode::Disabled;
    first.maximumRemoteRunTimeMs = 10U * 60U * 1'000U;
    UserSettings second = first;
    second.maximumRemoteRunTimeMs = 30U * 60U * 1'000U;
    second.lockPressCount = 4U;

    CHECK(store.save(first));
    CHECK(store.save(second));

    UserSettings loaded{};
    CHECK(store.load(loaded));
    CHECK(loaded.maximumRemoteRunTimeMs == 30U * 60U * 1'000U);
    CHECK(loaded.lockPressCount == 4U);
    CHECK(loaded.hoodMonitoring == HoodMonitoringMode::Disabled);
    CHECK(storage.writeCalls == 2U);
    CHECK(storage.commitCalls == 2U);
}

void testLegacySettingsRecordMigratesToFeatureSchema() {
    MemorySettingsStorage storage{};
    UserSettings legacy{};
    legacy.hoodMonitoring = HoodMonitoringMode::Disabled;
    legacy.maximumRemoteRunTimeMs = 18U * 60U * 1'000U;
    seedLegacySettingsRecord(storage, legacy);
    JournaledUserSettingsStore store{storage};

    UserSettings migrated{};
    CHECK(store.load(migrated));
    CHECK(migrated.hoodMonitoring == HoodMonitoringMode::Disabled);
    CHECK(migrated.maximumRemoteRunTimeMs == 18U * 60U * 1'000U);
    CHECK(migrated.features.mask() == 0U);

    CHECK(migrated.features.setEnabled(FeatureId::ColdEngineGuard, true));
    CHECK(store.save(migrated));
    UserSettings reloaded{};
    CHECK(store.load(reloaded));
    CHECK(reloaded.features.enabled(FeatureId::ColdEngineGuard));
    CHECK(reloaded.maximumRemoteRunTimeMs == 18U * 60U * 1'000U);
}

void testFeatureSettingsRecordMigratesTelemetryDefaults() {
    MemorySettingsStorage storage{};
    UserSettings featureSchema{};
    CHECK(featureSchema.features.setEnabled(FeatureId::ColdEngineGuard, true));
    featureSchema.coldEngineMaximumRpm = 3'000U;
    featureSchema.transmissionOverheatTemperatureC = 130U;
    seedFeatureSettingsRecord(storage, featureSchema);
    JournaledUserSettingsStore store{storage};

    UserSettings migrated{};
    CHECK(store.load(migrated));
    CHECK(migrated.features.enabled(FeatureId::ColdEngineGuard));
    CHECK(migrated.coldEngineMaximumRpm == 2'200U);
    CHECK(migrated.engineWarmTemperatureC == 75U);
    CHECK(migrated.transmissionOverheatTemperatureC == 110U);
    CHECK(migrated.temperatureAlertHysteresisC == 5U);

    migrated.coldEngineMaximumRpm = 2'800U;
    CHECK(store.save(migrated));
    UserSettings reloaded{};
    CHECK(store.load(reloaded));
    CHECK(reloaded.coldEngineMaximumRpm == 2'800U);
}

void testCorruptedNewestSettingsFallBackToPreviousSlot() {
    MemorySettingsStorage storage{};
    JournaledUserSettingsStore store{storage};
    UserSettings first{};
    first.maximumRemoteRunTimeMs = 10U * 60U * 1'000U;
    UserSettings second = first;
    second.maximumRemoteRunTimeMs = 30U * 60U * 1'000U;

    CHECK(store.save(first));
    CHECK(store.save(second));
    storage.bytes[JournaledUserSettingsStore::SlotSize + 15U] ^= 0x01U;

    UserSettings loaded{};
    CHECK(store.load(loaded));
    CHECK(loaded.maximumRemoteRunTimeMs == 10U * 60U * 1'000U);
}

void testInterruptedSettingsWritePreservesLastValidSlot() {
    MemorySettingsStorage storage{};
    JournaledUserSettingsStore store{storage};
    UserSettings first{};
    first.maximumRemoteRunTimeMs = 10U * 60U * 1'000U;
    UserSettings second = first;
    second.maximumRemoteRunTimeMs = 20U * 60U * 1'000U;
    UserSettings interrupted = first;
    interrupted.maximumRemoteRunTimeMs = 40U * 60U * 1'000U;

    CHECK(store.save(first));
    CHECK(store.save(second));
    storage.writeSucceeds = false;
    storage.partialWriteBytes = 12U;
    CHECK(!store.save(interrupted));

    UserSettings loaded{};
    CHECK(store.load(loaded));
    CHECK(loaded.maximumRemoteRunTimeMs == 20U * 60U * 1'000U);
}

void testInvalidSettingsAreNeverPersisted() {
    MemorySettingsStorage storage{};
    JournaledUserSettingsStore store{storage};
    UserSettings invalid{};
    invalid.maximumRemoteRunTimeMs = 0U;

    CHECK(!store.save(invalid));
    CHECK(storage.writeCalls == 0U);
    CHECK(storage.commitCalls == 0U);
}

void testSettingsPayloadRoundTripsEveryField() {
    UserSettings original{};
    original.remoteStartEnabled = false;
    original.hoodMonitoring = HoodMonitoringMode::Disabled;
    original.driverEntryMode =
        bmw::remote::application::DriverEntryMode::StopImmediately;
    original.maximumRemoteRunTimeMs = 25U * 60U * 1'000U;
    original.driverTakeoverTimeoutMs = 75'000U;
    original.lockPressCount = 4U;
    original.lockMinimumGapMs = 90U;
    original.lockMaximumGapMs = 1'800U;
    original.lockMaximumSequenceMs = 5'000U;
    original.coldEngineMaximumRpm = 2'700U;
    original.engineWarmTemperatureC = 80U;
    original.transmissionOverheatTemperatureC = 120U;
    original.temperatureAlertHysteresisC = 8U;
    CHECK(original.features.setEnabled(
        FeatureId::TransmissionOverheatAlert, true));
    CHECK(original.features.setEnabled(FeatureId::VirtualObdBle, true));
    bmw::remote::infrastructure::UserSettingsPayload payload{};

    CHECK(bmw::remote::infrastructure::encodeUserSettingsPayload(
        original, payload));

    UserSettings decoded{};
    CHECK(bmw::remote::infrastructure::decodeUserSettingsPayload(
        payload, decoded));
    CHECK(bmw::remote::infrastructure::userSettingsEqual(original, decoded));
}

void testLegacySettingsPayloadMigratesWithFeaturesDisabled() {
    UserSettings original{};
    original.hoodMonitoring = HoodMonitoringMode::Disabled;
    original.maximumRemoteRunTimeMs = 28U * 60U * 1'000U;
    CHECK(original.features.setEnabled(FeatureId::ColdEngineGuard, true));
    bmw::remote::infrastructure::UserSettingsPayload payload{};
    CHECK(bmw::remote::infrastructure::encodeUserSettingsPayload(
        original, payload));

    UserSettings migrated{};
    CHECK(bmw::remote::infrastructure::decodeUserSettingsPayload(
        payload,
        bmw::remote::infrastructure::LegacyUserSettingsPayloadSize,
        migrated));
    CHECK(migrated.hoodMonitoring == HoodMonitoringMode::Disabled);
    CHECK(migrated.maximumRemoteRunTimeMs == 28U * 60U * 1'000U);
    CHECK(migrated.features.mask() == 0U);
    CHECK(migrated.coldEngineMaximumRpm == 2'200U);
    CHECK(migrated.engineWarmTemperatureC == 75U);

    CHECK(bmw::remote::infrastructure::decodeUserSettingsPayload(
        payload,
        bmw::remote::infrastructure::FeatureUserSettingsPayloadSize,
        migrated));
    CHECK(migrated.features.enabled(FeatureId::ColdEngineGuard));
    CHECK(migrated.coldEngineMaximumRpm == 2'200U);
    CHECK(migrated.transmissionOverheatTemperatureC == 110U);
    CHECK(!bmw::remote::infrastructure::decodeUserSettingsPayload(
        payload, 25U, migrated));
}

void testSettingsPayloadRejectsInvalidValues() {
    UserSettings invalid{};
    invalid.lockPressCount = 1U;
    bmw::remote::infrastructure::UserSettingsPayload payload{};

    CHECK(!bmw::remote::infrastructure::encodeUserSettingsPayload(
        invalid, payload));

    UserSettings valid{};
    CHECK(bmw::remote::infrastructure::encodeUserSettingsPayload(
        valid, payload));
    payload[0] = 2U;
    CHECK(!bmw::remote::infrastructure::decodeUserSettingsPayload(
        payload, valid));
}

void testSettingsDeviceIdentityRoundTripsAndRejectsWrongProduct() {
    const auto expected =
        bmw::remote::infrastructure::settingsPrototypeIdentity(
            bmw::remote::infrastructure::SettingsHardwareTarget::Esp32S3DevKitC1);
    bmw::remote::infrastructure::UserSettingsPayload payload{};
    CHECK(bmw::remote::infrastructure::encodeSettingsDeviceIdentity(
        expected, payload));

    bmw::remote::infrastructure::SettingsDeviceIdentity decoded{};
    CHECK(bmw::remote::infrastructure::decodeSettingsDeviceIdentity(
        payload,
        bmw::remote::infrastructure::SettingsDeviceIdentityPayloadSize,
        decoded));
    CHECK(decoded.hardwareTarget == expected.hardwareTarget);
    CHECK(decoded.firmwareMajor == expected.firmwareMajor);
    CHECK(decoded.firmwareMinor == expected.firmwareMinor);
    CHECK(decoded.firmwarePatch == expected.firmwarePatch);
    CHECK(decoded.capabilities == expected.capabilities);

    payload[0U] ^= 0x01U;
    CHECK(!bmw::remote::infrastructure::decodeSettingsDeviceIdentity(
        payload,
        bmw::remote::infrastructure::SettingsDeviceIdentityPayloadSize,
        decoded));
    CHECK(!bmw::remote::infrastructure::decodeSettingsDeviceIdentity(
        payload, 0U, decoded));
}

bmw::remote::infrastructure::SettingsProtocolFrame settingsWriteRequest(
    const UserSettings& settings,
    const std::uint16_t requestId = 42U) {
    bmw::remote::infrastructure::SettingsProtocolFrame request{};
    request.type =
        bmw::remote::infrastructure::SettingsMessageType::WriteRequest;
    request.requestId = requestId;
    request.payloadSize = static_cast<std::uint16_t>(
        bmw::remote::infrastructure::UserSettingsPayloadSize);
    CHECK(bmw::remote::infrastructure::encodeUserSettingsPayload(
        settings, request.payload));
    return request;
}

void testSettingsProtocolFrameRoundTrip() {
    UserSettings settings{};
    settings.hoodMonitoring = HoodMonitoringMode::Disabled;
    settings.maximumRemoteRunTimeMs = 20U * 60U * 1'000U;
    const auto request = settingsWriteRequest(settings, 321U);
    bmw::remote::infrastructure::SettingsProtocolCodec::EncodedFrame encoded{};
    std::size_t encodedSize = 0U;

    CHECK(bmw::remote::infrastructure::SettingsProtocolCodec::encode(
        request, encoded, encodedSize));
    CHECK(encodedSize ==
          bmw::remote::infrastructure::SettingsProtocolCodec::MaximumFrameSize);

    const auto decoded =
        bmw::remote::infrastructure::SettingsProtocolCodec::decode(
            encoded.data(), encodedSize);
    CHECK(decoded.valid());
    CHECK(decoded.frame.type == request.type);
    CHECK(decoded.frame.status == request.status);
    CHECK(decoded.frame.requestId == 321U);
    CHECK(decoded.frame.payloadSize == request.payloadSize);
    CHECK(decoded.frame.payload == request.payload);
}

void testSettingsProtocolFrameRejectsCorruption() {
    bmw::remote::infrastructure::SettingsProtocolFrame request{};
    request.type =
        bmw::remote::infrastructure::SettingsMessageType::ReadRequest;
    bmw::remote::infrastructure::SettingsProtocolCodec::EncodedFrame encoded{};
    std::size_t encodedSize = 0U;
    CHECK(bmw::remote::infrastructure::SettingsProtocolCodec::encode(
        request, encoded, encodedSize));

    encoded[8] ^= 0x01U;
    const auto corrupted =
        bmw::remote::infrastructure::SettingsProtocolCodec::decode(
            encoded.data(), encodedSize);
    CHECK(corrupted.status ==
          bmw::remote::infrastructure::SettingsFrameDecodeStatus::ChecksumMismatch);
    CHECK(!bmw::remote::infrastructure::SettingsProtocolCodec::decode(
               encoded.data(), 5U)
               .valid());
    CHECK(bmw::remote::infrastructure::SettingsProtocolCodec::decode(
              encoded.data(), encoded.size() + 1U)
              .status ==
          bmw::remote::infrastructure::SettingsFrameDecodeStatus::TooLong);
}

void testSettingsProtocolFrameRejectsHeaderViolations() {
    bmw::remote::infrastructure::SettingsProtocolFrame request{};
    request.type =
        bmw::remote::infrastructure::SettingsMessageType::ReadRequest;
    bmw::remote::infrastructure::SettingsProtocolCodec::EncodedFrame encoded{};
    std::size_t encodedSize = 0U;
    CHECK(bmw::remote::infrastructure::SettingsProtocolCodec::encode(
        request, encoded, encodedSize));

    auto modified = encoded;
    modified[4] = 2U;
    CHECK(bmw::remote::infrastructure::SettingsProtocolCodec::decode(
              modified.data(), encodedSize)
              .status ==
          bmw::remote::infrastructure::SettingsFrameDecodeStatus::UnsupportedVersion);
    modified = encoded;
    modified[7] = 1U;
    CHECK(bmw::remote::infrastructure::SettingsProtocolCodec::decode(
              modified.data(), encodedSize)
              .status ==
          bmw::remote::infrastructure::SettingsFrameDecodeStatus::ReservedFieldSet);
    modified = encoded;
    modified[10] = static_cast<std::uint8_t>(
        bmw::remote::infrastructure::UserSettingsPayloadSize + 1U);
    CHECK(bmw::remote::infrastructure::SettingsProtocolCodec::decode(
              modified.data(), encodedSize)
              .status ==
          bmw::remote::infrastructure::SettingsFrameDecodeStatus::PayloadTooLarge);
}

void testSettingsProtocolRequiresAuthorizedSession() {
    MemorySettingsStorage storage{};
    JournaledUserSettingsStore store{storage};
    bmw::remote::infrastructure::SettingsProtocolService service{store};
    const auto request = settingsWriteRequest(UserSettings{});

    const auto response = service.handle(
        request,
        bmw::remote::infrastructure::SettingsProtocolAccess{
            false,
            bmw::remote::application::ControllerState::Idle});

    CHECK(response.status ==
          bmw::remote::infrastructure::SettingsProtocolStatus::Unauthorized);
    CHECK(storage.writeCalls == 0U);
}

void testSettingsProtocolRejectsWritesWhileControllerIsActive() {
    MemorySettingsStorage storage{};
    JournaledUserSettingsStore store{storage};
    bmw::remote::infrastructure::SettingsProtocolService service{store};
    UserSettings settings{};
    settings.maximumRemoteRunTimeMs = 30U * 60U * 1'000U;

    const auto response = service.handle(
        settingsWriteRequest(settings),
        bmw::remote::infrastructure::SettingsProtocolAccess{
            true,
            bmw::remote::application::ControllerState::Running});

    CHECK(response.status ==
          bmw::remote::infrastructure::SettingsProtocolStatus::Busy);
    CHECK(storage.writeCalls == 0U);
}

void testSettingsProtocolWritesThenReadsPersistedSettings() {
    MemorySettingsStorage storage{};
    JournaledUserSettingsStore store{storage};
    bmw::remote::infrastructure::SettingsProtocolService service{store};
    const bmw::remote::infrastructure::SettingsProtocolAccess access{
        true,
        bmw::remote::application::ControllerState::Idle};
    UserSettings settings{};
    settings.hoodMonitoring = HoodMonitoringMode::Disabled;
    settings.maximumRemoteRunTimeMs = 22U * 60U * 1'000U;

    const auto writeResponse = service.handle(
        settingsWriteRequest(settings, 7U), access);
    CHECK(writeResponse.type ==
          bmw::remote::infrastructure::SettingsMessageType::WriteResponse);
    CHECK(writeResponse.status ==
          bmw::remote::infrastructure::SettingsProtocolStatus::Ok);
    CHECK(writeResponse.requestId == 7U);

    bmw::remote::infrastructure::SettingsProtocolFrame readRequest{};
    readRequest.type =
        bmw::remote::infrastructure::SettingsMessageType::ReadRequest;
    readRequest.requestId = 8U;
    const auto readResponse = service.handle(readRequest, access);
    CHECK(readResponse.type ==
          bmw::remote::infrastructure::SettingsMessageType::ReadResponse);
    CHECK(readResponse.status ==
          bmw::remote::infrastructure::SettingsProtocolStatus::Ok);
    CHECK(readResponse.requestId == 8U);
    CHECK(readResponse.payloadSize ==
          bmw::remote::infrastructure::UserSettingsPayloadSize);

    UserSettings loaded{};
    CHECK(bmw::remote::infrastructure::decodeUserSettingsPayload(
        readResponse.payload, loaded));
    CHECK(bmw::remote::infrastructure::userSettingsEqual(settings, loaded));
}

void testSettingsProtocolReportsInvalidRequestsAndStorageFailures() {
    MemorySettingsStorage storage{};
    JournaledUserSettingsStore store{storage};
    bmw::remote::infrastructure::SettingsProtocolService service{store};
    const bmw::remote::infrastructure::SettingsProtocolAccess access{
        true,
        bmw::remote::application::ControllerState::Idle};

    auto invalid = settingsWriteRequest(UserSettings{});
    invalid.payload[3] = 1U;
    CHECK(service.handle(invalid, access).status ==
          bmw::remote::infrastructure::SettingsProtocolStatus::InvalidSettings);

    invalid = settingsWriteRequest(UserSettings{});
    invalid.payloadSize = 0U;
    CHECK(service.handle(invalid, access).status ==
          bmw::remote::infrastructure::SettingsProtocolStatus::InvalidPayload);

    bmw::remote::infrastructure::SettingsProtocolFrame unsupported{};
    unsupported.type = static_cast<
        bmw::remote::infrastructure::SettingsMessageType>(0x55U);
    CHECK(service.handle(unsupported, access).status ==
          bmw::remote::infrastructure::SettingsProtocolStatus::UnsupportedMessage);

    storage.writeSucceeds = false;
    CHECK(service.handle(settingsWriteRequest(UserSettings{}), access).status ==
          bmw::remote::infrastructure::SettingsProtocolStatus::StorageFailure);

    bmw::remote::infrastructure::SettingsProtocolFrame read{};
    read.type = bmw::remote::infrastructure::SettingsMessageType::ReadRequest;
    CHECK(service.handle(read, access).status ==
          bmw::remote::infrastructure::SettingsProtocolStatus::SettingsUnavailable);
}

struct MemorySettingsTransport final
    : bmw::remote::infrastructure::SettingsTransportPort {
    bmw::remote::infrastructure::SettingsProtocolCodec::EncodedFrame bytes{};
    std::size_t size{0U};
    std::uint32_t sendCalls{0U};
    bool succeeds{true};

    bool send(
        const std::uint8_t* const data,
        const std::size_t dataSize) noexcept override {
        ++sendCalls;
        if (!succeeds || data == nullptr || dataSize > bytes.size()) {
            return false;
        }
        bytes.fill(0U);
        for (std::size_t index = 0U; index < dataSize; ++index) {
            bytes[index] = data[index];
        }
        size = dataSize;
        return true;
    }
};

bmw::remote::infrastructure::SettingsProtocolCodec::EncodedFrame
encodeSettingsFrame(
    const bmw::remote::infrastructure::SettingsProtocolFrame& frame,
    std::size_t& encodedSize) {
    bmw::remote::infrastructure::SettingsProtocolCodec::EncodedFrame encoded{};
    CHECK(bmw::remote::infrastructure::SettingsProtocolCodec::encode(
        frame, encoded, encodedSize));
    return encoded;
}

bmw::remote::infrastructure::SettingsStreamEvent feedSettingsStream(
    bmw::remote::infrastructure::SettingsStreamReceiver& receiver,
    const std::uint8_t* const data,
    const std::size_t size,
    std::uint32_t& nowMs) {
    bmw::remote::infrastructure::SettingsStreamEvent event{};
    for (std::size_t index = 0U; index < size; ++index) {
        event = receiver.consume(data[index], nowMs++);
        if (index + 1U < size) {
            CHECK(event.type ==
                  bmw::remote::infrastructure::SettingsStreamEventType::None);
        }
    }
    return event;
}

bmw::remote::infrastructure::SettingsEndpointResult feedSettingsEndpoint(
    bmw::remote::infrastructure::SettingsProtocolEndpoint& endpoint,
    const std::uint8_t* const data,
    const std::size_t size,
    std::uint32_t& nowMs,
    const bmw::remote::infrastructure::SettingsProtocolAccess access) {
    bmw::remote::infrastructure::SettingsEndpointResult result{};
    for (std::size_t index = 0U; index < size; ++index) {
        result = endpoint.consume(data[index], nowMs++, access);
        if (index + 1U < size) {
            CHECK(result.status ==
                  bmw::remote::infrastructure::SettingsEndpointStatus::None);
        }
    }
    return result;
}

void testSettingsStreamFindsFrameAfterNoise() {
    bmw::remote::infrastructure::SettingsStreamReceiver receiver{};
    std::uint32_t nowMs = 100U;
    const std::array<std::uint8_t, 4U> noise = {'X', 'B', 'X', 0x00U};
    for (const std::uint8_t byte : noise) {
        CHECK(receiver.consume(byte, nowMs++).type ==
              bmw::remote::infrastructure::SettingsStreamEventType::None);
    }

    bmw::remote::infrastructure::SettingsProtocolFrame request{};
    request.type =
        bmw::remote::infrastructure::SettingsMessageType::ReadRequest;
    request.requestId = 99U;
    std::size_t encodedSize = 0U;
    const auto encoded = encodeSettingsFrame(request, encodedSize);
    const auto event = feedSettingsStream(
        receiver, encoded.data(), encodedSize, nowMs);

    CHECK(event.type ==
          bmw::remote::infrastructure::SettingsStreamEventType::FrameReady);
    CHECK(event.frame.type == request.type);
    CHECK(event.frame.requestId == 99U);
    CHECK(receiver.bufferedSize() == 0U);
}

void testSettingsStreamTimesOutPartialFrameAndRecovers() {
    bmw::remote::infrastructure::SettingsStreamReceiver receiver{
        bmw::remote::infrastructure::SettingsStreamConfig{50U}};
    CHECK(receiver.consume('B', 1'000U).type ==
          bmw::remote::infrastructure::SettingsStreamEventType::None);
    CHECK(receiver.poll(1'050U).type ==
          bmw::remote::infrastructure::SettingsStreamEventType::None);
    CHECK(receiver.poll(1'051U).type ==
          bmw::remote::infrastructure::SettingsStreamEventType::TimedOut);
    CHECK(receiver.bufferedSize() == 0U);

    bmw::remote::infrastructure::SettingsProtocolFrame request{};
    request.type =
        bmw::remote::infrastructure::SettingsMessageType::ReadRequest;
    std::size_t encodedSize = 0U;
    const auto encoded = encodeSettingsFrame(request, encodedSize);
    std::uint32_t nowMs = 2'000U;
    CHECK(feedSettingsStream(receiver, encoded.data(), encodedSize, nowMs).type ==
          bmw::remote::infrastructure::SettingsStreamEventType::FrameReady);
}

void testSettingsStreamTimeoutHandlesClockWraparound() {
    bmw::remote::infrastructure::SettingsStreamReceiver receiver{
        bmw::remote::infrastructure::SettingsStreamConfig{20U}};
    const std::uint32_t nearWrap =
        std::numeric_limits<std::uint32_t>::max() - 10U;

    CHECK(receiver.consume('B', nearWrap).type ==
          bmw::remote::infrastructure::SettingsStreamEventType::None);
    CHECK(receiver.poll(5U).type ==
          bmw::remote::infrastructure::SettingsStreamEventType::None);
    CHECK(receiver.poll(11U).type ==
          bmw::remote::infrastructure::SettingsStreamEventType::TimedOut);
}

void testSettingsStreamRejectsCorruptionThenAcceptsNextFrame() {
    bmw::remote::infrastructure::SettingsProtocolFrame request{};
    request.type =
        bmw::remote::infrastructure::SettingsMessageType::ReadRequest;
    std::size_t encodedSize = 0U;
    auto encoded = encodeSettingsFrame(request, encodedSize);
    encoded[8U] ^= 0x01U;
    bmw::remote::infrastructure::SettingsStreamReceiver receiver{};
    std::uint32_t nowMs = 0U;

    const auto rejected = feedSettingsStream(
        receiver, encoded.data(), encodedSize, nowMs);
    CHECK(rejected.type ==
          bmw::remote::infrastructure::SettingsStreamEventType::FrameRejected);
    CHECK(rejected.decodeStatus ==
          bmw::remote::infrastructure::SettingsFrameDecodeStatus::ChecksumMismatch);

    encoded = encodeSettingsFrame(request, encodedSize);
    CHECK(feedSettingsStream(receiver, encoded.data(), encodedSize, nowMs).type ==
          bmw::remote::infrastructure::SettingsStreamEventType::FrameReady);
}

void testSettingsStreamRejectsOversizedHeaderEarly() {
    bmw::remote::infrastructure::SettingsProtocolFrame request{};
    request.type =
        bmw::remote::infrastructure::SettingsMessageType::ReadRequest;
    std::size_t encodedSize = 0U;
    auto encoded = encodeSettingsFrame(request, encodedSize);
    encoded[10U] = static_cast<std::uint8_t>(
        bmw::remote::infrastructure::UserSettingsPayloadSize + 1U);
    encoded[11U] = 0U;
    bmw::remote::infrastructure::SettingsStreamReceiver receiver{};
    std::uint32_t nowMs = 0U;

    const auto rejected = feedSettingsStream(
        receiver,
        encoded.data(),
        bmw::remote::infrastructure::SettingsProtocolCodec::HeaderSize,
        nowMs);
    CHECK(rejected.type ==
          bmw::remote::infrastructure::SettingsStreamEventType::FrameRejected);
    CHECK(rejected.decodeStatus ==
          bmw::remote::infrastructure::SettingsFrameDecodeStatus::PayloadTooLarge);
}

void testSettingsEndpointReturnsUnauthorizedResponse() {
    MemorySettingsStorage storage{};
    JournaledUserSettingsStore store{storage};
    MemorySettingsTransport transport{};
    bmw::remote::infrastructure::SettingsProtocolEndpoint endpoint{
        store, transport};
    std::size_t requestSize = 0U;
    const auto request = encodeSettingsFrame(
        settingsWriteRequest(UserSettings{}, 12U), requestSize);
    std::uint32_t nowMs = 0U;

    const auto result = feedSettingsEndpoint(
        endpoint,
        request.data(),
        requestSize,
        nowMs,
        bmw::remote::infrastructure::SettingsProtocolAccess{
            false,
            bmw::remote::application::ControllerState::Idle});

    CHECK(result.status ==
          bmw::remote::infrastructure::SettingsEndpointStatus::ResponseSent);
    CHECK(result.responseStatus ==
          bmw::remote::infrastructure::SettingsProtocolStatus::Unauthorized);
    const auto response =
        bmw::remote::infrastructure::SettingsProtocolCodec::decode(
            transport.bytes.data(), transport.size);
    CHECK(response.valid());
    CHECK(response.frame.requestId == 12U);
    CHECK(response.frame.status ==
          bmw::remote::infrastructure::SettingsProtocolStatus::Unauthorized);
    CHECK(storage.writeCalls == 0U);
}

void testSettingsEndpointPersistsOnlyIdleWritesAndReadsWhileActive() {
    MemorySettingsStorage storage{};
    JournaledUserSettingsStore store{storage};
    MemorySettingsTransport transport{};
    bmw::remote::infrastructure::SettingsProtocolEndpoint endpoint{
        store, transport};
    UserSettings settings{};
    settings.hoodMonitoring = HoodMonitoringMode::Disabled;
    settings.maximumRemoteRunTimeMs = 28U * 60U * 1'000U;
    std::size_t requestSize = 0U;
    auto request = encodeSettingsFrame(
        settingsWriteRequest(settings, 20U), requestSize);
    std::uint32_t nowMs = 0U;

    auto result = feedSettingsEndpoint(
        endpoint,
        request.data(),
        requestSize,
        nowMs,
        bmw::remote::infrastructure::SettingsProtocolAccess{
            true,
            bmw::remote::application::ControllerState::Running});
    CHECK(result.responseStatus ==
          bmw::remote::infrastructure::SettingsProtocolStatus::Busy);
    CHECK(storage.writeCalls == 0U);

    result = feedSettingsEndpoint(
        endpoint,
        request.data(),
        requestSize,
        nowMs,
        bmw::remote::infrastructure::SettingsProtocolAccess{
            true,
            bmw::remote::application::ControllerState::Idle});
    CHECK(result.responseStatus ==
          bmw::remote::infrastructure::SettingsProtocolStatus::Ok);
    CHECK(storage.writeCalls == 1U);

    bmw::remote::infrastructure::SettingsProtocolFrame read{};
    read.type = bmw::remote::infrastructure::SettingsMessageType::ReadRequest;
    read.requestId = 21U;
    request = encodeSettingsFrame(read, requestSize);
    result = feedSettingsEndpoint(
        endpoint,
        request.data(),
        requestSize,
        nowMs,
        bmw::remote::infrastructure::SettingsProtocolAccess{
            true,
            bmw::remote::application::ControllerState::Running});
    CHECK(result.responseStatus ==
          bmw::remote::infrastructure::SettingsProtocolStatus::Ok);

    const auto response =
        bmw::remote::infrastructure::SettingsProtocolCodec::decode(
            transport.bytes.data(), transport.size);
    CHECK(response.valid());
    UserSettings readBack{};
    CHECK(bmw::remote::infrastructure::decodeUserSettingsPayload(
        response.frame.payload, readBack));
    CHECK(bmw::remote::infrastructure::userSettingsEqual(settings, readBack));
}

void testSettingsEndpointDoesNotRespondToCorruptFrame() {
    MemorySettingsStorage storage{};
    JournaledUserSettingsStore store{storage};
    MemorySettingsTransport transport{};
    bmw::remote::infrastructure::SettingsProtocolEndpoint endpoint{
        store, transport};
    bmw::remote::infrastructure::SettingsProtocolFrame read{};
    read.type = bmw::remote::infrastructure::SettingsMessageType::ReadRequest;
    std::size_t requestSize = 0U;
    auto request = encodeSettingsFrame(read, requestSize);
    request[8U] ^= 0x01U;
    std::uint32_t nowMs = 0U;

    const auto result = feedSettingsEndpoint(
        endpoint,
        request.data(),
        requestSize,
        nowMs,
        bmw::remote::infrastructure::SettingsProtocolAccess{
            true,
            bmw::remote::application::ControllerState::Idle});

    CHECK(result.status ==
          bmw::remote::infrastructure::SettingsEndpointStatus::FrameRejected);
    CHECK(result.decodeStatus ==
          bmw::remote::infrastructure::SettingsFrameDecodeStatus::ChecksumMismatch);
    CHECK(transport.sendCalls == 0U);
}

void testSettingsEndpointReportsTimeoutAndTransportFailure() {
    MemorySettingsStorage storage{};
    JournaledUserSettingsStore store{storage};
    MemorySettingsTransport transport{};
    bmw::remote::infrastructure::SettingsProtocolEndpoint endpoint{
        store,
        transport,
        bmw::remote::infrastructure::SettingsStreamConfig{25U}};

    CHECK(endpoint.consume(
              'B',
              10U,
              bmw::remote::infrastructure::SettingsProtocolAccess{})
              .status ==
          bmw::remote::infrastructure::SettingsEndpointStatus::None);
    CHECK(endpoint.poll(36U).status ==
          bmw::remote::infrastructure::SettingsEndpointStatus::FrameTimedOut);

    bmw::remote::infrastructure::SettingsProtocolFrame read{};
    read.type = bmw::remote::infrastructure::SettingsMessageType::ReadRequest;
    std::size_t requestSize = 0U;
    const auto request = encodeSettingsFrame(read, requestSize);
    transport.succeeds = false;
    std::uint32_t nowMs = 100U;
    const auto result = feedSettingsEndpoint(
        endpoint,
        request.data(),
        requestSize,
        nowMs,
        bmw::remote::infrastructure::SettingsProtocolAccess{
            true,
            bmw::remote::application::ControllerState::Idle});
    CHECK(result.status ==
          bmw::remote::infrastructure::SettingsEndpointStatus::TransportFailure);
    CHECK(result.responseStatus ==
          bmw::remote::infrastructure::SettingsProtocolStatus::SettingsUnavailable);
}

struct LoopbackSettingsDeviceChannel final
    : bmw::remote::host::SettingsDeviceChannel {
    MemorySettingsStorage storage{};
    JournaledUserSettingsStore store{storage};
    bmw::remote::infrastructure::SettingsDeviceIdentity identity{
        bmw::remote::infrastructure::settingsPrototypeIdentity(
            bmw::remote::infrastructure::SettingsHardwareTarget::HostSimulation)};
    bmw::remote::infrastructure::SettingsProtocolService service;
    bmw::remote::infrastructure::SettingsProtocolAccess access{
        true,
        ControllerState::Idle};
    bmw::remote::infrastructure::SettingsProtocolCodec::EncodedFrame response{};
    std::size_t responseSize{0U};
    std::size_t readOffset{0U};
    std::size_t timeoutAfterBytes{std::numeric_limits<std::size_t>::max()};
    std::uint32_t requestCount{0U};
    bool clearSucceeds{true};
    bool writeSucceeds{true};
    bool readFails{false};
    bool corruptResponse{false};
    bool mismatchRequestId{false};
    bool dropResponse{false};

    explicit LoopbackSettingsDeviceChannel(
        const bmw::remote::infrastructure::SettingsDeviceIdentity
            deviceIdentity =
                bmw::remote::infrastructure::settingsPrototypeIdentity(
                    bmw::remote::infrastructure::SettingsHardwareTarget::
                        HostSimulation))
        : identity(deviceIdentity), service(store, identity) {}

    bool clearInput(std::string& error) override {
        if (!clearSucceeds) {
            error = "simulated clear failure";
            return false;
        }
        response.fill(0U);
        responseSize = 0U;
        readOffset = 0U;
        error.clear();
        return true;
    }

    bool writeAll(
        const std::uint8_t* const data,
        const std::size_t size,
        const std::uint32_t timeoutMs,
        std::string& error) override {
        ++requestCount;
        if (!writeSucceeds || data == nullptr || timeoutMs == 0U) {
            error = "simulated write failure";
            return false;
        }

        const auto decoded =
            bmw::remote::infrastructure::SettingsProtocolCodec::decode(
                data, size);
        if (!decoded.valid()) {
            error = "invalid simulated request";
            return false;
        }
        auto protocolResponse = service.handle(decoded.frame, access);
        if (mismatchRequestId) {
            ++protocolResponse.requestId;
        }
        if (dropResponse) {
            responseSize = 0U;
            error.clear();
            return true;
        }
        if (!bmw::remote::infrastructure::SettingsProtocolCodec::encode(
                protocolResponse, response, responseSize)) {
            error = "unable to encode simulated response";
            return false;
        }
        if (corruptResponse && responseSize != 0U) {
            response[responseSize - 1U] ^= 0x01U;
        }
        readOffset = 0U;
        error.clear();
        return true;
    }

    bmw::remote::host::SettingsChannelReadStatus readByte(
        std::uint8_t& byte,
        const std::uint32_t timeoutMs,
        std::string& error) override {
        if (readFails || timeoutMs == 0U) {
            error = "simulated read failure";
            return bmw::remote::host::SettingsChannelReadStatus::Failure;
        }
        if (readOffset >= responseSize || readOffset >= timeoutAfterBytes) {
            error.clear();
            return bmw::remote::host::SettingsChannelReadStatus::Timeout;
        }
        byte = response[readOffset];
        ++readOffset;
        error.clear();
        return bmw::remote::host::SettingsChannelReadStatus::Data;
    }
};

void testSettingsDeviceClientProbesCompatibleIdentity() {
    LoopbackSettingsDeviceChannel channel{};
    bmw::remote::host::SettingsDeviceClient client{channel};
    bmw::remote::infrastructure::SettingsDeviceIdentity identity{};
    std::string error{};

    CHECK(client.probe(identity, error));
    CHECK(error.empty());
    CHECK(identity.hardwareTarget ==
          bmw::remote::infrastructure::SettingsHardwareTarget::HostSimulation);
    CHECK(bmw::remote::infrastructure::hasCapability(
        identity,
        bmw::remote::infrastructure::SettingsDeviceCapability::SettingsRead));
    CHECK(bmw::remote::infrastructure::hasCapability(
        identity,
        bmw::remote::infrastructure::SettingsDeviceCapability::SettingsWrite));
    CHECK(channel.requestCount == 1U);
}

void testSettingsDeviceClientReadsPersistedSettings() {
    LoopbackSettingsDeviceChannel channel{};
    UserSettings expected{};
    expected.hoodMonitoring = HoodMonitoringMode::Disabled;
    expected.maximumRemoteRunTimeMs = 19U * 60U * 1'000U;
    CHECK(channel.store.save(expected));
    bmw::remote::host::SettingsDeviceClient client{channel};
    UserSettings received{};
    std::string error{};

    CHECK(client.read(received, error));
    CHECK(error.empty());
    CHECK(bmw::remote::infrastructure::userSettingsEqual(expected, received));
    CHECK(channel.requestCount == 1U);
}

void testSettingsDeviceClientWritesAndVerifiesByReadingBack() {
    LoopbackSettingsDeviceChannel channel{};
    bmw::remote::host::SettingsDeviceClient client{channel};
    UserSettings expected{};
    expected.hoodMonitoring = HoodMonitoringMode::Disabled;
    expected.maximumRemoteRunTimeMs = 31U * 60U * 1'000U;
    expected.lockPressCount = 4U;
    std::string error{};

    CHECK(client.writeAndVerify(expected, error));
    CHECK(error.empty());
    CHECK(channel.requestCount == 3U);

    UserSettings persisted{};
    CHECK(channel.store.load(persisted));
    CHECK(bmw::remote::infrastructure::userSettingsEqual(expected, persisted));
}

void testSettingsDeviceClientRequiresPersistentWriteCapability() {
    auto identity = bmw::remote::infrastructure::settingsPrototypeIdentity(
        bmw::remote::infrastructure::SettingsHardwareTarget::HostSimulation);
    identity.capabilities =
        bmw::remote::infrastructure::capabilityMask(
            bmw::remote::infrastructure::SettingsDeviceCapability::SettingsRead);
    LoopbackSettingsDeviceChannel channel{identity};
    bmw::remote::host::SettingsDeviceClient client{channel};
    std::string error{};

    CHECK(!client.writeAndVerify(UserSettings{}, error));
    CHECK(error.find("ecriture persistante") != std::string::npos);
    CHECK(channel.requestCount == 1U);
    CHECK(channel.storage.writeCalls == 0U);
}

void testSettingsDeviceClientRejectsProtocolFailures() {
    LoopbackSettingsDeviceChannel channel{};
    UserSettings persisted{};
    CHECK(channel.store.save(persisted));
    bmw::remote::host::SettingsDeviceClient client{channel};
    UserSettings received{};
    std::string error{};

    channel.mismatchRequestId = true;
    CHECK(!client.read(received, error));
    CHECK(error.find("identifiant") != std::string::npos);

    channel.mismatchRequestId = false;
    channel.corruptResponse = true;
    CHECK(!client.read(received, error));
    CHECK(error.find("checksum_mismatch") != std::string::npos);

    channel.corruptResponse = false;
    channel.access.authorized = false;
    CHECK(!client.read(received, error));
    CHECK(error.find("unauthorized") != std::string::npos);
}

void testSettingsDeviceClientBoundsTimeoutsAndTransportFailures() {
    LoopbackSettingsDeviceChannel channel{};
    UserSettings persisted{};
    CHECK(channel.store.save(persisted));
    bmw::remote::host::SettingsDeviceClient client{
        channel,
        bmw::remote::host::SettingsDeviceClientConfig{10U, 5U}};
    UserSettings received{};
    std::string error{};

    channel.timeoutAfterBytes = 5U;
    CHECK(!client.read(received, error));
    CHECK(error.find("reponse partielle") != std::string::npos);

    channel.timeoutAfterBytes = std::numeric_limits<std::size_t>::max();
    channel.dropResponse = true;
    CHECK(!client.read(received, error));
    CHECK(error.find("delai de reponse") != std::string::npos);

    channel.dropResponse = false;
    channel.readFails = true;
    CHECK(!client.read(received, error));
    CHECK(error.find("simulated read failure") != std::string::npos);
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
    bool disengageSucceeds{true};
    bool secureSucceeds{true};
    std::uint32_t secureCalls{0U};
    std::uint32_t starterReleaseCalls{0U};
    std::uint32_t remoteControlReleaseCalls{0U};

    bool enableIgnition() noexcept override { return ignitionSucceeds; }
    bool engageStarter() noexcept override { return true; }
    bool disengageStarter() noexcept override {
        ++starterReleaseCalls;
        return disengageSucceeds;
    }
    bool releaseRemoteControl() noexcept override {
        ++remoteControlReleaseCalls;
        return releaseSucceeds;
    }
    bool secureOutputs() noexcept override {
        ++secureCalls;
        return secureSucceeds;
    }
};

struct TrackingActuatorDriver final : ActuatorPort {
    bool ignitionActive{false};
    bool starterActive{false};
    bool enableSucceeds{true};
    bool engageSucceeds{true};
    bool disengageSucceeds{true};
    bool releaseSucceeds{true};
    bool secureSucceeds{true};
    std::uint32_t enableCalls{0U};
    std::uint32_t engageCalls{0U};
    std::uint32_t disengageCalls{0U};
    std::uint32_t releaseCalls{0U};
    std::uint32_t secureCalls{0U};

    bool enableIgnition() noexcept override {
        ++enableCalls;
        if (enableSucceeds) {
            ignitionActive = true;
        }
        return enableSucceeds;
    }

    bool engageStarter() noexcept override {
        ++engageCalls;
        if (engageSucceeds) {
            starterActive = true;
        }
        return engageSucceeds;
    }

    bool disengageStarter() noexcept override {
        ++disengageCalls;
        if (disengageSucceeds) {
            starterActive = false;
        }
        return disengageSucceeds;
    }

    bool releaseRemoteControl() noexcept override {
        ++releaseCalls;
        if (releaseSucceeds) {
            ignitionActive = false;
            starterActive = false;
        }
        return releaseSucceeds;
    }

    bool secureOutputs() noexcept override {
        ++secureCalls;
        if (secureSucceeds) {
            ignitionActive = false;
            starterActive = false;
        }
        return secureSucceeds;
    }
};

constexpr ActuatorFeedback actuatorFeedback(
    const bool ignitionActive,
    const bool starterActive) noexcept {
    return {true, ignitionActive, starterActive};
}

void prepareActuatorSupervisor(
    ActuatorSafetySupervisor& supervisor,
    const std::uint32_t nowMs = 0U) {
    CHECK(supervisor.heartbeat(nowMs));
    const auto status = supervisor.poll(
        nowMs, true, actuatorFeedback(false, false));
    CHECK(status.healthy());
}

void confirmIgnitionFeedback(
    ActuatorSafetySupervisor& supervisor,
    const std::uint32_t nowMs = 101U) {
    CHECK(supervisor.heartbeat(nowMs));
    const auto status = supervisor.poll(
        nowMs, true, actuatorFeedback(true, false));
    CHECK(status.healthy());
    CHECK(status.ignitionFeedbackConfirmed);
}

void testActuatorSupervisorInitializesSafeAndRejectsInvalidConfig() {
    TrackingActuatorDriver driver{};
    driver.ignitionActive = true;
    driver.starterActive = true;
    ActuatorSafetySupervisor supervisor{driver};

    CHECK(supervisor.status().healthy());
    CHECK(!driver.ignitionActive);
    CHECK(!driver.starterActive);
    CHECK(driver.disengageCalls == 1U);
    CHECK(driver.secureCalls == 1U);

    TrackingActuatorDriver invalidDriver{};
    ActuatorSafetyConfig invalidConfig{};
    invalidConfig.watchdogTimeoutMs = 0U;
    ActuatorSafetySupervisor invalid{invalidDriver, invalidConfig};
    CHECK(!invalid.status().initialized);
    CHECK(
        invalid.status().fault ==
        ActuatorSupervisorFault::InvalidConfiguration);
    CHECK(!invalid.resetFault(0U, actuatorFeedback(false, false)));
    CHECK(
        invalid.status().fault ==
        ActuatorSupervisorFault::InvalidConfiguration);
}

void testActuatorSupervisorRejectsUnsafeCommandSequences() {
    TrackingActuatorDriver missingHeartbeatDriver{};
    ActuatorSafetySupervisor missingHeartbeat{missingHeartbeatDriver};
    CHECK(!missingHeartbeat.engageStarter());
    CHECK(
        missingHeartbeat.status().fault ==
        ActuatorSupervisorFault::WatchdogExpired);

    TrackingActuatorDriver interlockDriver{};
    ActuatorSafetySupervisor interlock{interlockDriver};
    CHECK(interlock.heartbeat(0U));
    static_cast<void>(interlock.poll(
        0U, false, actuatorFeedback(false, false)));
    CHECK(!interlock.enableIgnition());
    CHECK(
        interlock.status().fault ==
        ActuatorSupervisorFault::HardwareInterlockLost);

    TrackingActuatorDriver sequenceDriver{};
    ActuatorSafetySupervisor sequence{sequenceDriver};
    prepareActuatorSupervisor(sequence);
    CHECK(!sequence.engageStarter());
    CHECK(
        sequence.status().fault ==
        ActuatorSupervisorFault::CommandSequence);

    TrackingActuatorDriver feedbackDriver{};
    ActuatorSafetySupervisor feedback{feedbackDriver};
    prepareActuatorSupervisor(feedback);
    CHECK(feedback.enableIgnition());
    CHECK(!feedback.engageStarter());
    CHECK(
        feedback.status().fault ==
        ActuatorSupervisorFault::CommandSequence);

    TrackingActuatorDriver optionalFeedbackDriver{};
    ActuatorSafetyConfig optionalFeedbackConfig{};
    optionalFeedbackConfig.requireFeedback = false;
    ActuatorSafetySupervisor optionalFeedback{
        optionalFeedbackDriver, optionalFeedbackConfig};
    prepareActuatorSupervisor(optionalFeedback);
    CHECK(optionalFeedback.enableIgnition());
    CHECK(optionalFeedback.engageStarter());
}

void testActuatorSupervisorAllowsNominalSequence() {
    TrackingActuatorDriver driver{};
    ActuatorSafetySupervisor supervisor{driver};
    prepareActuatorSupervisor(supervisor);

    CHECK(supervisor.enableIgnition());
    confirmIgnitionFeedback(supervisor);
    CHECK(supervisor.engageStarter());
    CHECK(supervisor.heartbeat(200U));
    const auto cranking = supervisor.poll(
        200U, true, actuatorFeedback(true, true));
    CHECK(cranking.healthy());
    CHECK(cranking.starterCommanded);

    CHECK(supervisor.disengageStarter());
    CHECK(supervisor.heartbeat(302U));
    const auto running = supervisor.poll(
        302U, true, actuatorFeedback(true, false));
    CHECK(running.healthy());
    CHECK(running.ignitionCommanded);
    CHECK(!running.starterCommanded);

    CHECK(supervisor.releaseRemoteControl());
    CHECK(!driver.ignitionActive);
    CHECK(!driver.starterActive);
    CHECK(supervisor.status().healthy());
}

void testActuatorSupervisorWatchdogSafesOutputs() {
    TrackingActuatorDriver driver{};
    ActuatorSafetySupervisor supervisor{driver};
    prepareActuatorSupervisor(supervisor);
    CHECK(supervisor.enableIgnition());
    confirmIgnitionFeedback(supervisor);
    CHECK(supervisor.engageStarter());

    CHECK(!supervisor.heartbeat(602U));
    CHECK(
        supervisor.status().fault ==
        ActuatorSupervisorFault::WatchdogExpired);
    CHECK(!driver.ignitionActive);
    CHECK(!driver.starterActive);
    CHECK(driver.secureCalls == 2U);

    TrackingActuatorDriver polledDriver{};
    ActuatorSafetySupervisor polled{polledDriver};
    prepareActuatorSupervisor(polled);
    CHECK(polled.enableIgnition());
    confirmIgnitionFeedback(polled);
    const auto expired = polled.poll(
        602U, true, actuatorFeedback(true, false));
    CHECK(expired.fault == ActuatorSupervisorFault::WatchdogExpired);
    CHECK(!polledDriver.ignitionActive);
}

void testActuatorSupervisorLimitsStarterDuration() {
    TrackingActuatorDriver driver{};
    ActuatorSafetyConfig config{};
    config.watchdogTimeoutMs = 1'000U;
    config.maximumStarterActiveMs = 200U;
    ActuatorSafetySupervisor supervisor{driver, config};
    prepareActuatorSupervisor(supervisor);
    CHECK(supervisor.enableIgnition());
    confirmIgnitionFeedback(supervisor);
    CHECK(supervisor.engageStarter());
    CHECK(supervisor.heartbeat(302U));

    const auto expired = supervisor.poll(
        302U, true, actuatorFeedback(true, true));
    CHECK(expired.fault == ActuatorSupervisorFault::StarterTimeout);
    CHECK(!driver.ignitionActive);
    CHECK(!driver.starterActive);
}

void testActuatorSupervisorDetectsInterlockAndFeedbackFaults() {
    TrackingActuatorDriver interlockDriver{};
    ActuatorSafetySupervisor interlock{interlockDriver};
    prepareActuatorSupervisor(interlock);
    CHECK(interlock.enableIgnition());
    const auto lost = interlock.poll(
        50U, false, actuatorFeedback(true, false));
    CHECK(
        lost.fault == ActuatorSupervisorFault::HardwareInterlockLost);

    TrackingActuatorDriver mismatchDriver{};
    ActuatorSafetySupervisor mismatch{mismatchDriver};
    prepareActuatorSupervisor(mismatch);
    CHECK(mismatch.enableIgnition());
    CHECK(mismatch.heartbeat(101U));
    const auto mismatched = mismatch.poll(
        101U, true, actuatorFeedback(false, false));
    CHECK(mismatched.fault == ActuatorSupervisorFault::FeedbackMismatch);

    TrackingActuatorDriver unavailableDriver{};
    ActuatorSafetySupervisor unavailable{unavailableDriver};
    prepareActuatorSupervisor(unavailable);
    CHECK(unavailable.enableIgnition());
    CHECK(unavailable.heartbeat(101U));
    const auto unavailableStatus = unavailable.poll(101U, true, {});
    CHECK(
        unavailableStatus.fault ==
        ActuatorSupervisorFault::FeedbackUnavailable);
}

void testActuatorSupervisorHandlesClockWrapAndGuardedReset() {
    constexpr std::uint32_t NearWrap =
        std::numeric_limits<std::uint32_t>::max() - 100U;
    TrackingActuatorDriver driver{};
    ActuatorSafetySupervisor supervisor{driver};
    prepareActuatorSupervisor(supervisor, NearWrap);
    CHECK(supervisor.enableIgnition());
    CHECK(supervisor.heartbeat(50U));
    const auto wrapped = supervisor.poll(
        50U, true, actuatorFeedback(true, false));
    CHECK(wrapped.healthy());
    CHECK(wrapped.ignitionFeedbackConfirmed);

    CHECK(!supervisor.heartbeat(49U));
    CHECK(
        supervisor.status().fault ==
        ActuatorSupervisorFault::ClockRegression);
    CHECK(!driver.ignitionActive);

    CHECK(supervisor.resetFault(100U, actuatorFeedback(false, false)));
    CHECK(supervisor.status().healthy());
    CHECK(!supervisor.enableIgnition());
    CHECK(
        supervisor.status().fault ==
        ActuatorSupervisorFault::WatchdogExpired);
}

void testActuatorSupervisorLatchesDriverAndSafingFailures() {
    TrackingActuatorDriver driver{};
    ActuatorSafetySupervisor supervisor{driver};
    prepareActuatorSupervisor(supervisor);
    driver.enableSucceeds = false;
    CHECK(!supervisor.enableIgnition());
    CHECK(
        supervisor.status().fault ==
        ActuatorSupervisorFault::DriverFailure);
    CHECK(driver.secureCalls == 2U);

    TrackingActuatorDriver unsafeDriver{};
    ActuatorSafetySupervisor unsafe{unsafeDriver};
    unsafeDriver.disengageSucceeds = false;
    unsafeDriver.secureSucceeds = false;
    CHECK(!unsafe.secureOutputs());
    CHECK(unsafe.status().fault == ActuatorSupervisorFault::SafingFailure);
    CHECK(unsafe.status().safingFailed);
}

struct FakeTimer final : TimerPort {
    bool cancelSucceeds{true};

    bool arm(std::uint32_t) noexcept override { return true; }
    bool cancel() noexcept override { return cancelSucceeds; }
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

void testRuntimePropagatesActuatorSupervisorFault() {
    FakeVehicleGateway gateway{};
    TrackingActuatorDriver driver{};
    ActuatorSafetySupervisor supervisor{driver};
    FakeTimer timer{};
    FakeNotifications notifications{};
    Runtime runtime{
        qualifiedController(), gateway, supervisor, timer, notifications};
    const VehicleState vehicle = safeAutomaticVehicle();

    prepareActuatorSupervisor(supervisor);
    CHECK(
        runtime.dispatch(
            Event{EventType::RemoteStartRequested}, vehicle, 0U)
            .state == ControllerState::Authorizing);
    CHECK(
        runtime.dispatch(
            Event{EventType::VehicleStateUpdated}, vehicle, 0U)
            .state == ControllerState::Preparing);
    CHECK(driver.ignitionActive);

    confirmIgnitionFeedback(supervisor);
    CHECK(
        runtime.dispatch(Event{EventType::TimerElapsed}, vehicle, 101U)
            .state == ControllerState::Cranking);
    CHECK(driver.starterActive);
    CHECK(supervisor.heartbeat(200U));
    CHECK(
        supervisor.poll(200U, true, actuatorFeedback(true, true)).healthy());

    const auto expired = supervisor.poll(
        701U, true, actuatorFeedback(true, true));
    CHECK(expired.fault == ActuatorSupervisorFault::WatchdogExpired);
    CHECK(!driver.ignitionActive);
    CHECK(!driver.starterActive);

    const auto fault = runtime.dispatch(
        Event::infrastructureFailure(FaultCode::ActuatorFailure),
        vehicle,
        701U);
    CHECK(fault.state == ControllerState::Fault);
    CHECK(fault.fault == FaultCode::ActuatorFailure);
    CHECK(notifications.faultNotifications == 1U);
    CHECK(
        supervisor.status().fault ==
        ActuatorSupervisorFault::WatchdogExpired);
}

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

void testDiagnosticJournalRecordsCommandsTransitionsAndRefusals() {
    FakeVehicleGateway gateway{};
    FakeActuator actuator{};
    FakeTimer timer{};
    FakeNotifications notifications{};
    DiagnosticJournal journal{};
    Runtime runtime{
        Controller{}, gateway, actuator, timer, notifications, &journal};

    static_cast<void>(runtime.dispatch(
        Event{EventType::RemoteStartRequested},
        safeAutomaticVehicle(),
        42U));

    CHECK(journal.size() == 2U);
    CHECK(journal.overwrittenCount() == 0U);

    DiagnosticRecord command{};
    DiagnosticRecord rejection{};
    CHECK(journal.read(0U, command));
    CHECK(journal.read(1U, rejection));
    CHECK(command.sequence == 1U);
    CHECK(command.timestampMs == 42U);
    CHECK(command.type == DiagnosticRecordType::CommandReceived);
    CHECK(command.trigger == EventType::RemoteStartRequested);
    CHECK(command.state == ControllerState::Idle);
    CHECK(rejection.sequence == 2U);
    CHECK(rejection.type == DiagnosticRecordType::RequestRejected);
    CHECK(rejection.reason == DiagnosticReason::ProfileNotReady);
    CHECK(
        (rejection.profileReasons &
         bmw::remote::application::mask(
             ProfileReadinessReason::ProfileNotSelected)) != 0U);
    CHECK(!journal.read(2U, rejection));

    journal.clear();
    Runtime resetRuntime{
        qualifiedController(),
        gateway,
        actuator,
        timer,
        notifications,
        &journal};
    const VehicleState safeVehicle = safeAutomaticVehicle();
    static_cast<void>(resetRuntime.dispatch(
        Event::infrastructureFailure(FaultCode::ActuatorFailure),
        safeVehicle,
        50U));
    static_cast<void>(resetRuntime.dispatch(
        Event{EventType::ResetRequested}, safeVehicle, 60U));
    CHECK(journal.size() == 4U);
    CHECK(journal.read(2U, command));
    CHECK(command.type == DiagnosticRecordType::CommandReceived);
    CHECK(command.previousState == ControllerState::Fault);
    CHECK(command.state == ControllerState::Fault);
    CHECK(command.fault == FaultCode::ActuatorFailure);
}

void testDiagnosticJournalRecordsRuntimeAndSafingFailures() {
    FakeVehicleGateway gateway{};
    gateway.succeeds = false;
    FakeActuator actuator{};
    actuator.disengageSucceeds = false;
    actuator.secureSucceeds = false;
    FakeTimer timer{};
    timer.cancelSucceeds = false;
    FakeNotifications notifications{};
    DiagnosticJournal journal{};
    Runtime runtime{
        qualifiedController(),
        gateway,
        actuator,
        timer,
        notifications,
        &journal};

    static_cast<void>(runtime.dispatch(
        Event{EventType::RemoteStartRequested},
        safeAutomaticVehicle(),
        100U));

    CHECK(journal.size() == 8U);
    DiagnosticRecord record{};
    CHECK(journal.read(2U, record));
    CHECK(record.type == DiagnosticRecordType::InfrastructureFailure);
    CHECK(record.trigger == EventType::RemoteStartRequested);
    CHECK(record.fault == FaultCode::VehicleCommunication);
    CHECK(journal.read(3U, record));
    CHECK(record.type == DiagnosticRecordType::StateTransition);
    CHECK(record.previousState == ControllerState::Authorizing);
    CHECK(record.state == ControllerState::Fault);
    CHECK(journal.read(4U, record));
    CHECK(record.type == DiagnosticRecordType::FaultEntered);
    for (std::size_t index = 5U; index < journal.size(); ++index) {
        CHECK(journal.read(index, record));
        CHECK(record.type == DiagnosticRecordType::SafingFailure);
        CHECK(record.timestampMs == 100U);
    }
}

void testDiagnosticJournalDoesNotStoreUneventfulVehicleUpdates() {
    DiagnosticJournal journal{};
    bmw::remote::application::Decision decision{};
    journal.observe(
        Event{EventType::VehicleStateUpdated}, decision, 500U);
    CHECK(journal.size() == 0U);
}

void testDiagnosticJournalOverwritesOldestRecordsDeterministically() {
    DiagnosticJournal journal{};
    for (std::uint32_t index = 0U; index < 40U; ++index) {
        journal.recordInfrastructureFailure(
            EventType::TimerElapsed,
            ControllerState::Running,
            FaultCode::TimerFailure,
            index);
    }

    CHECK(journal.size() == DiagnosticJournal::MaximumRecords);
    CHECK(journal.overwrittenCount() == 8U);
    DiagnosticRecord oldest{};
    DiagnosticRecord newest{};
    CHECK(journal.read(0U, oldest));
    CHECK(journal.read(journal.size() - 1U, newest));
    CHECK(oldest.sequence == 9U);
    CHECK(oldest.timestampMs == 8U);
    CHECK(newest.sequence == 40U);
    CHECK(newest.timestampMs == 39U);

    journal.clear();
    CHECK(journal.size() == 0U);
    CHECK(journal.overwrittenCount() == 0U);
}

void testLostBodyUpdatesBecomeStaleAndFaultRemoteRun() {
    SyntheticPowertrainState running{};
    running.engineRpm = 850U;
    const std::array<CanFrame, 5U> trace = {
        makeSyntheticPowertrainFrame(0U),
        makeSyntheticBodyFrame(0U),
        makeSyntheticPowertrainFrame(1'800U, running),
        makeSyntheticBodyFrame(1'800U),
        makeSyntheticPowertrainFrame(5'000U, running),
    };
    SyntheticCanDecoder decoder{};
    ReplayVehicleGateway gateway{trace.data(), trace.size(), decoder};
    FakeActuator actuator{};
    FakeTimer timer{};
    FakeNotifications notifications{};
    DiagnosticJournal journal{};
    Runtime runtime{
        qualifiedController(),
        gateway,
        actuator,
        timer,
        notifications,
        &journal};
    VehicleState vehicle{};

    CHECK(runtime.dispatch(
              Event{EventType::RemoteStartRequested}, vehicle, 0U)
              .state == ControllerState::Authorizing);
    vehicle = gateway.state();
    CHECK(runtime.dispatch(
              Event{EventType::VehicleStateUpdated}, vehicle, 0U)
              .state == ControllerState::Preparing);
    CHECK(runtime.dispatch(
              Event{EventType::TimerElapsed}, vehicle, 1'500U)
              .state == ControllerState::Cranking);
    CHECK(gateway.setElapsedTime(1'800U));
    CHECK(gateway.requestState());
    vehicle = gateway.state();
    CHECK(runtime.dispatch(
              Event{EventType::VehicleStateUpdated}, vehicle, 1'800U)
              .state == ControllerState::Running);

    CHECK(gateway.setElapsedTime(5'000U));
    CHECK(gateway.requestState());
    vehicle = gateway.state();
    CHECK(vehicle.engineRpm.quality == SignalQuality::Fresh);
    CHECK(vehicle.doorsClosed.quality == SignalQuality::Stale);
    const auto fault = runtime.dispatch(
        Event{EventType::VehicleStateUpdated}, vehicle, 5'000U);

    CHECK(fault.state == ControllerState::Fault);
    CHECK(fault.fault == FaultCode::SafetyInterlock);
    CHECK(fault.safety.contains(SafetyReason::SignalUnavailable));
    CHECK(fault.contains(ActionType::SecureOutputs));
    CHECK(actuator.secureCalls == 1U);
}

void testDelayedSignalsPreventPreparationFromCranking() {
    SyntheticPowertrainState running{};
    running.engineRpm = 850U;
    const std::array<CanFrame, 4U> trace = {
        makeSyntheticPowertrainFrame(0U),
        makeSyntheticBodyFrame(0U),
        makeSyntheticPowertrainFrame(1'800U, running),
        makeSyntheticBodyFrame(1'800U),
    };
    SyntheticCanDecoder decoder{};
    ReplayVehicleGateway gateway{trace.data(), trace.size(), decoder};
    FakeActuator actuator{};
    FakeTimer timer{};
    FakeNotifications notifications{};
    Runtime runtime{
        qualifiedController(), gateway, actuator, timer, notifications};
    VehicleState vehicle{};

    static_cast<void>(runtime.dispatch(
        Event{EventType::RemoteStartRequested}, vehicle, 0U));
    vehicle = gateway.state();
    CHECK(runtime.dispatch(
              Event{EventType::VehicleStateUpdated}, vehicle, 0U)
              .state == ControllerState::Preparing);
    CHECK(gateway.setElapsedTime(1'500U));
    CHECK(gateway.requestState());
    CHECK(gateway.lastBatch().emittedFrames == 0U);
    vehicle = gateway.state();
    CHECK(vehicle.engineRpm.quality == SignalQuality::Stale);
    CHECK(vehicle.transmission.quality == SignalQuality::Stale);

    const auto fault = runtime.dispatch(
        Event{EventType::TimerElapsed}, vehicle, 1'500U);
    CHECK(fault.state == ControllerState::Fault);
    CHECK(fault.fault == FaultCode::SafetyInterlock);
    CHECK(fault.safety.contains(SafetyReason::SignalUnavailable));
    CHECK(!fault.contains(ActionType::EngageStarter));
    CHECK(actuator.secureCalls == 1U);
}

void testCorruptedRecognizedFrameFaultsAuthorizationGateway() {
    CanFrame corruptedBody = makeSyntheticBodyFrame(0U);
    corruptedBody.data[7U] ^= 0x01U;
    const std::array<CanFrame, 2U> trace = {
        makeSyntheticPowertrainFrame(0U),
        corruptedBody,
    };
    SyntheticCanDecoder decoder{};
    ReplayVehicleGateway gateway{trace.data(), trace.size(), decoder};
    FakeActuator actuator{};
    FakeTimer timer{};
    FakeNotifications notifications{};
    DiagnosticJournal journal{};
    Runtime runtime{
        qualifiedController(),
        gateway,
        actuator,
        timer,
        notifications,
        &journal};

    const auto fault = runtime.dispatch(
        Event{EventType::RemoteStartRequested}, VehicleState{}, 0U);

    CHECK(fault.state == ControllerState::Fault);
    CHECK(fault.fault == FaultCode::VehicleCommunication);
    CHECK(fault.contains(ActionType::SecureOutputs));
    CHECK(gateway.lastBatch().status == ReplayStatus::ConsumerRejected);
    CHECK(gateway.statistics().rejectedFrames == 1U);
    CHECK(actuator.secureCalls == 1U);
    DiagnosticRecord infrastructureFailure{};
    CHECK(journal.read(2U, infrastructureFailure));
    CHECK(
        infrastructureFailure.type ==
        DiagnosticRecordType::InfrastructureFailure);
    CHECK(infrastructureFailure.fault == FaultCode::VehicleCommunication);
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

void testSyntheticTelemetryFrameDecodesSignedValuesAndFreshness() {
    SyntheticTelemetryState telemetry{};
    telemetry.coolantTemperatureC = -12;
    telemetry.engineOilTemperatureC = 64;
    telemetry.transmissionOilTemperatureC = 112;
    telemetry.dpfRegenerationActive = true;
    const std::array<CanFrame, 2U> trace = {
        makeSyntheticPowertrainFrame(0U),
        makeSyntheticTelemetryFrame(0U, telemetry),
    };
    SyntheticCanDecoder decoder{};
    ReplayVehicleGateway gateway{trace.data(), trace.size(), decoder};

    CHECK(gateway.requestState());
    const VehicleState fresh = gateway.state();
    CHECK(fresh.coolantTemperatureC.value == -12);
    CHECK(fresh.engineOilTemperatureC.value == 64);
    CHECK(fresh.transmissionOilTemperatureC.value == 112);
    CHECK(fresh.dpfRegenerationActive.value);
    CHECK(fresh.coolantTemperatureC.quality == SignalQuality::Fresh);

    CHECK(gateway.setElapsedTime(2'001U));
    CHECK(gateway.state().coolantTemperatureC.quality == SignalQuality::Stale);

    CanFrame malformed = makeSyntheticTelemetryFrame(0U, telemetry);
    malformed.data[4] = 1U;
    VehicleStateAssembler assembler{decoder};
    CHECK(!assembler.consume(malformed));
    CHECK(assembler.snapshot(0U).engineOilTemperatureC.quality ==
          SignalQuality::Unavailable);
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

void testSandboxNominalInteractiveFlow() {
    SandboxSession session{};

    CHECK(session.snapshot().state == ControllerState::Idle);
    CHECK(session.execute("start").snapshot.state == ControllerState::Preparing);

    const auto cranking = session.execute("timer");
    CHECK(cranking.ok);
    CHECK(cranking.snapshot.state == ControllerState::Cranking);
    CHECK(cranking.snapshot.ignitionActive);
    CHECK(cranking.snapshot.starterActive);

    const auto running = session.execute("vehicle rpm=850");
    CHECK(running.ok);
    CHECK(running.snapshot.state == ControllerState::Running);
    CHECK(running.snapshot.ignitionActive);
    CHECK(!running.snapshot.starterActive);

    const auto stopping = session.execute("stop");
    CHECK(stopping.snapshot.state == ControllerState::Stopping);
    CHECK(!stopping.snapshot.ignitionActive);
    CHECK(!stopping.snapshot.starterActive);

    const auto stopped = session.execute("vehicle rpm=0");
    CHECK(stopped.snapshot.state == ControllerState::Idle);
    CHECK(!stopped.snapshot.timerArmed);
}

void testSandboxOptionalHoodAndDriverTakeover() {
    SandboxSession session{};
    CHECK(session.execute("new optional").ok);
    CHECK(session.execute("vehicle hood=unavailable").ok);
    CHECK(session.execute("start").snapshot.state == ControllerState::Preparing);
    CHECK(session.execute("timer").snapshot.state == ControllerState::Cranking);
    CHECK(session.execute("vehicle rpm=850").snapshot.state == ControllerState::Running);
    CHECK(session.execute("vehicle doors=open").snapshot.state ==
          ControllerState::AwaitingTakeover);

    const auto takeover = session.execute("takeover");
    CHECK(takeover.snapshot.state == ControllerState::DriverControl);
    CHECK(!takeover.snapshot.ignitionActive);
    CHECK(!takeover.snapshot.timerArmed);

    CHECK(session.execute("new required").snapshot.hoodMonitoringRequired);
    CHECK(session.execute("vehicle hood=unavailable").ok);
    CHECK(session.execute("start").snapshot.state == ControllerState::Idle);
}

void testSandboxRejectsInvalidUpdatesAndPropagatesWatchdog() {
    SandboxSession session{};
    const auto invalid = session.execute("vehicle rpm=9000 doors=open");
    CHECK(!invalid.ok);
    CHECK(invalid.snapshot.vehicle.engineRpm.value == 0U);
    CHECK(invalid.snapshot.vehicle.doorsClosed.value);

    CHECK(session.execute("start").snapshot.state == ControllerState::Preparing);
    const auto watchdog = session.execute("watchdog");
    CHECK(watchdog.snapshot.state == ControllerState::Fault);
    CHECK(watchdog.snapshot.fault == FaultCode::ActuatorFailure);
    CHECK(watchdog.snapshot.supervisorFault == ActuatorSupervisorFault::WatchdogExpired);
    CHECK(!watchdog.snapshot.ignitionActive);
    CHECK(!watchdog.snapshot.starterActive);

    const auto reset = session.execute("reset");
    CHECK(reset.ok);
    CHECK(reset.snapshot.state == ControllerState::Idle);
    CHECK(reset.snapshot.fault == FaultCode::None);
    CHECK(reset.snapshot.supervisorFault == ActuatorSupervisorFault::None);

    CHECK(session.execute("start").snapshot.state == ControllerState::Preparing);
    const auto interlock = session.execute("interlock off");
    CHECK(interlock.snapshot.state == ControllerState::Fault);
    CHECK(interlock.snapshot.supervisorFault ==
          ActuatorSupervisorFault::HardwareInterlockLost);
    CHECK(!interlock.snapshot.ignitionActive);
}

void testSandboxTelemetryFeaturesAreOptionalAndProduceAlerts() {
    SandboxSession session{};
    CHECK(session.snapshot().telemetry.coldEngineGuard ==
          TelemetryConditionState::Disabled);
    CHECK(session.execute("feature cold_engine_guard on").ok);
    CHECK(session.execute("feature dpf_regeneration_indicator on").ok);
    CHECK(session.execute("feature transmission_overheat_alert on").ok);

    const auto active = session.execute(
        "vehicle rpm=2600 coolant=50 oil=55 "
        "transmission_temperature=112 dpf=on");
    CHECK(active.ok);
    CHECK(active.snapshot.telemetry.coldEngineGuard ==
          TelemetryConditionState::Active);
    CHECK(active.snapshot.telemetry.dpfRegeneration ==
          TelemetryConditionState::Active);
    CHECK(active.snapshot.telemetry.transmissionOverheat ==
          TelemetryConditionState::Active);
    CHECK(active.snapshot.telemetry.contains(
        TelemetryAlertType::ColdEngineHighRpm));
    CHECK(active.snapshot.telemetry.contains(
        TelemetryAlertType::DpfRegenerationStarted));
    CHECK(active.snapshot.telemetry.contains(
        TelemetryAlertType::TransmissionOverheat));

    const std::string json = bmw::remote::host::encodeSandboxResult(active);
    CHECK(json.find("\"cold_engine_guard\":\"active\"") !=
          std::string::npos);
    CHECK(json.find("\"dpf_regeneration_active\":true") !=
          std::string::npos);
    CHECK(json.find("\"transmission_overheat\":\"active\"") !=
          std::string::npos);

    CHECK(!session.execute("vehicle coolant=-41").ok);
    CHECK(!session.execute("feature no_such_feature on").ok);
    CHECK(session.execute("feature cold_engine_guard off")
              .snapshot.telemetry.coldEngineGuard ==
          TelemetryConditionState::Disabled);
}

void testSandboxLineProtocolReturnsOneJsonObjectPerCommand() {
    std::istringstream input{
        "\xEF\xBB\xBF" "status\nstart\nquit\n"};
    std::ostringstream output{};
    CHECK(bmw::remote::host::runSandboxProtocol(input, output) == 0);

    std::istringstream lines{output.str()};
    std::string line{};
    std::size_t lineCount = 0U;
    while (std::getline(lines, line)) {
        ++lineCount;
        CHECK(!line.empty() && line.front() == '{' && line.back() == '}');
        CHECK(line.find("\"ok\":true") != std::string::npos);
    }
    CHECK(lineCount == 3U);
    CHECK(output.str().find("\"state\":\"preparing\"") != std::string::npos);
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
        {"verified lock evidence", testVerifiedLockEvidenceTriggersOnlyAfterCompleteSequence},
        {"lock evidence trust", testLockGateRejectsUntrustedSyntheticAndUnsecuredEvidence},
        {"lock evidence freshness", testLockGateRejectsStaleFutureAndRegressingClocks},
        {"lock evidence ordering", testLockGateRejectsDuplicateOutOfOrderAndNonMonotonicEvidence},
        {"lock evidence wraparound", testLockGateAcceptsCounterAndClockWraparound},
        {"lock evidence reset", testLockGateResetsPartialGestureAfterRejectedEvidence},
        {"lock evidence configuration", testLockGateRejectsInvalidConfigurationAndDebouncedEvidence},
        {"qualified CAN lock edges", testQualifiedCanLockPipelineRecognizesThreeEdgesAcrossCounterWrap},
        {"CAN lock held command", testCanLockAdapterRequiresRisingEdges},
        {"CAN lock trust boundaries", testCanLockPipelinePreservesCandidateAndSecuredTrustBoundaries},
        {"CAN lock structural reset", testCanLockPipelineResetsGestureAfterStructuralFrameRejection},
        {"CAN lock binding validation", testCanLockAdapterRejectsInvalidBindingsAndCounterAnomalies},
        {"default user settings", testDefaultUserSettingsAreValidAndPreserved},
        {"feature catalog stability", testFeatureCatalogHasStableCompleteIdentifiers},
        {"feature request mask", testFeatureRequestsDefaultOffAndRejectUnknownBits},
        {"feature capability resolution", testFeatureResolverSeparatesRequestCapabilityAndQualification},
        {"feature platform and write gates", testFeatureResolverGatesWritesAndSupportsBothPhonePlatforms},
        {"telemetry feature gates and alerts", testTelemetryMonitorIsReadOnlyFeatureGatedAndEdgeTriggered},
        {"telemetry freshness and hysteresis", testTelemetryMonitorRequiresFreshSignalsAndUsesHysteresis},
        {"custom user settings", testUserSettingsConfigureHoodTimersEntryAndLocks},
        {"invalid user settings", testUnsafeUserSettingsAreRejectedFailClosed},
        {"invalid feature mask", testUserSettingsRejectUnknownFeatureBits},
        {"remote start disabled", testUserCanDisableRemoteStart},
        {"door opens stop mode", testUserCanStopImmediatelyWhenDoorOpens},
        {"settings file", testUserSettingsFileLoadsStrictConfiguration},
        {"settings file strict keys", testUserSettingsFileRejectsUnknownAndDuplicateKeys},
        {"settings file safety bounds", testUserSettingsFileRejectsUnsafeDurations},
        {"settings file feature toggles", testUserSettingsFileParsesIndependentFeatureToggles},
        {"settings file writer round trip", testUserSettingsFileWriterRoundTripsEverySetting},
        {"settings file writer precision", testUserSettingsFileWriterRejectsPrecisionLoss},
        {"settings file safe replacement", testUserSettingsFileSaveReplacesOnlyWithValidatedContent},
        {"empty settings storage", testEmptySettingsStorageDisablesRemoteStart},
        {"settings journal round trip", testJournaledSettingsRoundTripUsesLatestGeneration},
        {"legacy settings storage migration", testLegacySettingsRecordMigratesToFeatureSchema},
        {"feature settings storage migration", testFeatureSettingsRecordMigratesTelemetryDefaults},
        {"settings corruption fallback", testCorruptedNewestSettingsFallBackToPreviousSlot},
        {"settings interrupted write", testInterruptedSettingsWritePreservesLastValidSlot},
        {"invalid settings persistence", testInvalidSettingsAreNeverPersisted},
        {"settings payload round trip", testSettingsPayloadRoundTripsEveryField},
        {"legacy settings payload migration", testLegacySettingsPayloadMigratesWithFeaturesDisabled},
        {"settings payload rejection", testSettingsPayloadRejectsInvalidValues},
        {"settings device identity", testSettingsDeviceIdentityRoundTripsAndRejectsWrongProduct},
        {"settings protocol frame", testSettingsProtocolFrameRoundTrip},
        {"settings protocol corruption", testSettingsProtocolFrameRejectsCorruption},
        {"settings protocol header", testSettingsProtocolFrameRejectsHeaderViolations},
        {"settings protocol authorization", testSettingsProtocolRequiresAuthorizedSession},
        {"settings protocol busy", testSettingsProtocolRejectsWritesWhileControllerIsActive},
        {"settings protocol read write", testSettingsProtocolWritesThenReadsPersistedSettings},
        {"settings protocol failures", testSettingsProtocolReportsInvalidRequestsAndStorageFailures},
        {"settings stream synchronization", testSettingsStreamFindsFrameAfterNoise},
        {"settings stream timeout", testSettingsStreamTimesOutPartialFrameAndRecovers},
        {"settings stream clock wrap", testSettingsStreamTimeoutHandlesClockWraparound},
        {"settings stream corruption recovery", testSettingsStreamRejectsCorruptionThenAcceptsNextFrame},
        {"settings stream oversized header", testSettingsStreamRejectsOversizedHeaderEarly},
        {"settings endpoint authorization", testSettingsEndpointReturnsUnauthorizedResponse},
        {"settings endpoint idle writes", testSettingsEndpointPersistsOnlyIdleWritesAndReadsWhileActive},
        {"settings endpoint corrupt frame", testSettingsEndpointDoesNotRespondToCorruptFrame},
        {"settings endpoint failures", testSettingsEndpointReportsTimeoutAndTransportFailure},
        {"settings device probe", testSettingsDeviceClientProbesCompatibleIdentity},
        {"settings device read", testSettingsDeviceClientReadsPersistedSettings},
        {"settings device write verify", testSettingsDeviceClientWritesAndVerifiesByReadingBack},
        {"settings device write capability", testSettingsDeviceClientRequiresPersistentWriteCapability},
        {"settings device protocol failures", testSettingsDeviceClientRejectsProtocolFailures},
        {"settings device transport failures", testSettingsDeviceClientBoundsTimeoutsAndTransportFailures},
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
        {"actuator supervisor initialization", testActuatorSupervisorInitializesSafeAndRejectsInvalidConfig},
        {"actuator supervisor sequence guards", testActuatorSupervisorRejectsUnsafeCommandSequences},
        {"actuator supervisor nominal sequence", testActuatorSupervisorAllowsNominalSequence},
        {"actuator supervisor watchdog", testActuatorSupervisorWatchdogSafesOutputs},
        {"actuator supervisor starter limit", testActuatorSupervisorLimitsStarterDuration},
        {"actuator supervisor interlocks", testActuatorSupervisorDetectsInterlockAndFeedbackFaults},
        {"actuator supervisor clock reset", testActuatorSupervisorHandlesClockWrapAndGuardedReset},
        {"actuator supervisor driver faults", testActuatorSupervisorLatchesDriverAndSafingFailures},
        {"runtime actuator supervisor fault", testRuntimePropagatesActuatorSupervisorFault},
        {"runtime missing profile", testRuntimeRejectsMissingProfileWithoutRequestingVehicle},
        {"gateway failure", testRuntimeConvertsGatewayFailureIntoSafeFault},
        {"actuator failure", testRuntimeConvertsActuatorFailureIntoSafeFault},
        {"takeover release failure", testRuntimeSafesOutputsWhenTakeoverReleaseFails},
        {"diagnostic journal decisions", testDiagnosticJournalRecordsCommandsTransitionsAndRefusals},
        {"diagnostic journal failures", testDiagnosticJournalRecordsRuntimeAndSafingFailures},
        {"diagnostic journal quiet updates", testDiagnosticJournalDoesNotStoreUneventfulVehicleUpdates},
        {"diagnostic journal rollover", testDiagnosticJournalOverwritesOldestRecordsDeterministically},
        {"lost body updates", testLostBodyUpdatesBecomeStaleAndFaultRemoteRun},
        {"delayed safety signals", testDelayedSignalsPreventPreparationFromCranking},
        {"corrupted authorization frame", testCorruptedRecognizedFrameFaultsAuthorizationGateway},
        {"non-monotonic trace", testReplayRejectsNonMonotonicTrace},
        {"time-bounded replay", testReplayOnlyEmitsFramesDueAtCurrentTime},
        {"signal freshness", testAssemblerMarksOldSignalsStale},
        {"synthetic telemetry frame", testSyntheticTelemetryFrameDecodesSignedValuesAndFreshness},
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
        {"sandbox nominal flow", testSandboxNominalInteractiveFlow},
        {"sandbox optional hood takeover", testSandboxOptionalHoodAndDriverTakeover},
        {"sandbox watchdog", testSandboxRejectsInvalidUpdatesAndPropagatesWatchdog},
        {"sandbox telemetry alerts", testSandboxTelemetryFeaturesAreOptionalAndProduceAlerts},
        {"sandbox line protocol", testSandboxLineProtocolReturnsOneJsonObjectPerCommand},
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
