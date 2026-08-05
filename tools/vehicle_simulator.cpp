#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "bmw_remote/application/controller.hpp"
#include "bmw_remote/application/feature_catalog.hpp"
#include "bmw_remote/application/lock_command_gate.hpp"
#include "bmw_remote/application/safety_policy.hpp"
#include "bmw_remote/application/user_settings.hpp"
#include "bmw_remote/infrastructure/actuator_safety_supervisor.hpp"
#include "bmw_remote/infrastructure/can_lock_command_adapter.hpp"
#include "bmw_remote/infrastructure/replay_vehicle_gateway.hpp"
#include "bmw_remote/infrastructure/runtime.hpp"
#include "bmw_remote/infrastructure/settings_payload.hpp"
#include "bmw_remote/infrastructure/settings_protocol.hpp"
#include "bmw_remote/infrastructure/settings_storage.hpp"
#include "bmw_remote/infrastructure/settings_stream.hpp"
#include "bmw_remote/simulation/synthetic_can.hpp"
#include "tools/can_trace_csv.hpp"
#include "tools/sandbox_session.hpp"
#include "tools/user_settings_file.hpp"

namespace {

using namespace bmw::remote;

enum class Scenario : std::uint8_t {
    Nominal,
    HoodRequired,
    HoodOptional,
    TakeoverTimeout,
    TakeoverConfirmed,
    SignalLoss,
    SignalDelay,
    FrameCorruption,
    UserConfig,
    SettingsRecovery,
    SettingsLink,
    LockReplayGuard,
    QualifiedLockAdapter,
    ActuatorSupervisor,
    SupervisedRuntime,
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

class SimulatedActuatorDriver final : public infrastructure::ActuatorPort {
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
        ignitionActive = false;
        starterActive = false;
        return true;
    }

    bool secureOutputs() noexcept override {
        ignitionActive = false;
        starterActive = false;
        return true;
    }

    [[nodiscard]] infrastructure::ActuatorFeedback feedback() const noexcept {
        return {true, ignitionActive, starterActive};
    }

    bool ignitionActive{false};
    bool starterActive{false};
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

void printDiagnosticJournal(
    const infrastructure::DiagnosticJournal& journal) {
    std::cout << "diagnostic_log: records=" << journal.size()
              << " overwritten=" << journal.overwrittenCount() << '\n';
    infrastructure::DiagnosticRecord record{};
    for (std::size_t index = 0U; index < journal.size(); ++index) {
        if (!journal.read(index, record)) {
            continue;
        }
        std::cout
            << "  #" << record.sequence
            << " time_ms=" << record.timestampMs
            << " type=" << infrastructure::toString(record.type)
            << " trigger=" << application::toString(record.trigger)
            << " state=" << application::toString(record.previousState)
            << "->" << application::toString(record.state)
            << " fault=" << application::toString(record.fault)
            << " reason=" << infrastructure::toString(record.reason)
            << " safety_mask=" << record.safetyReasons
            << " profile_mask="
            << static_cast<unsigned int>(record.profileReasons)
            << '\n';
    }
}

int finishRuntimeScenario(
    const bool expectedOutcome,
    const infrastructure::ReplayVehicleGateway& gateway,
    const infrastructure::DiagnosticJournal& diagnosticJournal) {
    const infrastructure::AssemblyStatistics stats = gateway.statistics();
    printDiagnosticJournal(diagnosticJournal);
    std::cout << "replay: " << stats.consumedFrames << " frames, "
              << stats.decodedSignals << " decoded signals, "
              << stats.rejectedFrames << " rejected frames\n"
              << "scenario_result: " << (expectedOutcome ? "PASS" : "FAIL")
              << '\n';
    return expectedOutcome ? 0 : 2;
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
        case Scenario::SignalLoss: return "signal-loss";
        case Scenario::SignalDelay: return "signal-delay";
        case Scenario::FrameCorruption: return "frame-corruption";
        case Scenario::UserConfig: return "user-config";
        case Scenario::SettingsRecovery: return "settings-recovery";
        case Scenario::SettingsLink: return "settings-link";
        case Scenario::LockReplayGuard: return "lock-replay-guard";
        case Scenario::QualifiedLockAdapter: return "qualified-lock-adapter";
        case Scenario::ActuatorSupervisor: return "actuator-supervisor";
        case Scenario::SupervisedRuntime: return "supervised-runtime";
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
    if (text == "signal-loss") {
        scenario = Scenario::SignalLoss;
        return true;
    }
    if (text == "signal-delay") {
        scenario = Scenario::SignalDelay;
        return true;
    }
    if (text == "frame-corruption") {
        scenario = Scenario::FrameCorruption;
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
    if (text == "settings-link") {
        scenario = Scenario::SettingsLink;
        return true;
    }
    if (text == "lock-replay-guard") {
        scenario = Scenario::LockReplayGuard;
        return true;
    }
    if (text == "qualified-lock-adapter") {
        scenario = Scenario::QualifiedLockAdapter;
        return true;
    }
    if (text == "actuator-supervisor") {
        scenario = Scenario::ActuatorSupervisor;
        return true;
    }
    if (text == "supervised-runtime") {
        scenario = Scenario::SupervisedRuntime;
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
        << "signal-loss omits body updates until freshness forces a safe fault\n"
        << "signal-delay withholds updates until preparation is rejected\n"
        << "frame-corruption rejects a recognized frame before authorization\n"
        << "user-config uses every value from a supplied configuration file\n"
        << "settings-recovery corrupts the newest slot and restores the previous one\n"
        << "settings-link exercises framed, authorized and idle-only configuration\n"
        << "lock-replay-guard rejects untrusted, stale and replayed lock evidence\n"
        << "qualified-lock-adapter checks edges and a rolling frame counter\n"
        << "actuator-supervisor injects watchdog and feedback failures\n"
        << "supervised-runtime connects controller, runtime and supervisor\n";
}

void printFeatureList() {
    std::cout
        << "code | categorie | classe | jalon | defaut\n"
        << "-----|-----------|--------|-------|-------\n";
    for (const application::FeatureDescriptor& feature :
         application::featureCatalog()) {
        std::cout << feature.code << " | "
                  << application::toString(feature.category) << " | "
                  << application::toString(feature.controlClass) << " | "
                  << application::toString(feature.releaseTier)
                  << " | disabled\n";
    }
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
        << "  " << executable << " --list-features\n"
        << "  " << executable << " --sandbox\n"
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

    std::cout << "enabled_features:";
    bool hasEnabledFeature = false;
    for (const application::FeatureDescriptor& feature :
         application::featureCatalog()) {
        if (settings.features.enabled(feature.id)) {
            std::cout << (hasEnabledFeature ? "," : " ") << feature.code;
            hasEnabledFeature = true;
        }
    }
    std::cout << (hasEnabledFeature ? "\n" : " none\n");
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

class SimulatorSettingsTransport final
    : public infrastructure::SettingsTransportPort {
public:
    bool send(
        const std::uint8_t* const data,
        const std::size_t size) noexcept override {
        if (data == nullptr || size > bytes_.size()) {
            return false;
        }
        bytes_.fill(0U);
        for (std::size_t index = 0U; index < size; ++index) {
            bytes_[index] = data[index];
        }
        size_ = size;
        ++sendCount_;
        return true;
    }

    [[nodiscard]] const infrastructure::SettingsProtocolCodec::EncodedFrame&
    bytes() const noexcept {
        return bytes_;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] std::uint32_t sendCount() const noexcept {
        return sendCount_;
    }

private:
    infrastructure::SettingsProtocolCodec::EncodedFrame bytes_{};
    std::size_t size_{0U};
    std::uint32_t sendCount_{0U};
};

bool exchangeSettingsFrame(
    infrastructure::SettingsProtocolEndpoint& endpoint,
    SimulatorSettingsTransport& transport,
    const infrastructure::SettingsProtocolFrame& request,
    const infrastructure::SettingsProtocolAccess access,
    infrastructure::SettingsProtocolFrame& response,
    std::uint32_t& nowMs) noexcept {
    infrastructure::SettingsProtocolCodec::EncodedFrame requestBytes{};
    std::size_t requestSize = 0U;
    if (!infrastructure::SettingsProtocolCodec::encode(
            request, requestBytes, requestSize)) {
        return false;
    }

    infrastructure::SettingsEndpointResult endpointResult{};
    for (std::size_t index = 0U; index < requestSize; ++index) {
        endpointResult = endpoint.consume(requestBytes[index], nowMs++, access);
    }
    if (endpointResult.status !=
        infrastructure::SettingsEndpointStatus::ResponseSent) {
        return false;
    }

    const auto decodedResponse = infrastructure::SettingsProtocolCodec::decode(
        transport.bytes().data(), transport.size());
    if (!decodedResponse.valid()) {
        return false;
    }
    response = decodedResponse.frame;
    return true;
}

int runSettingsLinkScenario() {
    SimulatorSettingsStorage storage{};
    infrastructure::JournaledUserSettingsStore store{storage};
    SimulatorSettingsTransport transport{};
    infrastructure::SettingsProtocolEndpoint endpoint{store, transport};
    std::uint32_t nowMs = 0U;

    application::UserSettings requestedSettings{};
    requestedSettings.hoodMonitoring =
        application::HoodMonitoringMode::Disabled;
    requestedSettings.maximumRemoteRunTimeMs = 20U * 60U * 1'000U;
    requestedSettings.driverTakeoverTimeoutMs = 90'000U;

    infrastructure::SettingsProtocolFrame writeRequest{};
    writeRequest.type = infrastructure::SettingsMessageType::WriteRequest;
    writeRequest.requestId = 100U;
    writeRequest.payloadSize = static_cast<std::uint16_t>(
        infrastructure::UserSettingsPayloadSize);
    const bool payloadEncoded = infrastructure::encodeUserSettingsPayload(
        requestedSettings, writeRequest.payload);

    infrastructure::SettingsProtocolFrame unauthorizedResponse{};
    const bool unauthorizedExchanged = exchangeSettingsFrame(
        endpoint,
        transport,
        writeRequest,
        infrastructure::SettingsProtocolAccess{
            false,
            application::ControllerState::Idle},
        unauthorizedResponse,
        nowMs);

    infrastructure::SettingsProtocolFrame busyResponse{};
    const bool busyExchanged = exchangeSettingsFrame(
        endpoint,
        transport,
        writeRequest,
        infrastructure::SettingsProtocolAccess{
            true,
            application::ControllerState::Running},
        busyResponse,
        nowMs);

    infrastructure::SettingsProtocolFrame writeResponse{};
    const bool writeExchanged = exchangeSettingsFrame(
        endpoint,
        transport,
        writeRequest,
        infrastructure::SettingsProtocolAccess{
            true,
            application::ControllerState::Idle},
        writeResponse,
        nowMs);

    infrastructure::SettingsProtocolFrame readRequest{};
    readRequest.type = infrastructure::SettingsMessageType::ReadRequest;
    readRequest.requestId = 101U;
    infrastructure::SettingsProtocolFrame readResponse{};
    const bool readExchanged = exchangeSettingsFrame(
        endpoint,
        transport,
        readRequest,
        infrastructure::SettingsProtocolAccess{
            true,
            application::ControllerState::Running},
        readResponse,
        nowMs);

    application::UserSettings readBack{};
    const bool settingsDecoded =
        readResponse.payloadSize == infrastructure::UserSettingsPayloadSize &&
        infrastructure::decodeUserSettingsPayload(
            readResponse.payload, readBack);

    infrastructure::SettingsProtocolCodec::EncodedFrame corruptedBytes{};
    std::size_t corruptedSize = 0U;
    const bool corruptedEncoded = infrastructure::SettingsProtocolCodec::encode(
        writeRequest, corruptedBytes, corruptedSize);
    if (corruptedEncoded) {
        corruptedBytes[8U] ^= 0x01U;
    }
    const std::uint32_t sendsBeforeCorruption = transport.sendCount();
    infrastructure::SettingsEndpointResult corrupted{};
    for (std::size_t index = 0U; index < corruptedSize; ++index) {
        corrupted = endpoint.consume(
            corruptedBytes[index],
            nowMs++,
            infrastructure::SettingsProtocolAccess{
                true,
                application::ControllerState::Idle});
    }

    const bool expectedOutcome =
        payloadEncoded && unauthorizedExchanged && busyExchanged &&
        writeExchanged && readExchanged && settingsDecoded &&
        unauthorizedResponse.status ==
            infrastructure::SettingsProtocolStatus::Unauthorized &&
        busyResponse.status == infrastructure::SettingsProtocolStatus::Busy &&
        writeResponse.status == infrastructure::SettingsProtocolStatus::Ok &&
        readResponse.status == infrastructure::SettingsProtocolStatus::Ok &&
        infrastructure::userSettingsEqual(requestedSettings, readBack) &&
        corrupted.status == infrastructure::SettingsEndpointStatus::FrameRejected &&
        corrupted.decodeStatus ==
            infrastructure::SettingsFrameDecodeStatus::ChecksumMismatch &&
        transport.sendCount() == sendsBeforeCorruption;

    std::cout
        << "scenario: settings-link\n"
        << "unauthorized_write: "
        << infrastructure::toString(unauthorizedResponse.status) << '\n'
        << "active_controller_write: "
        << infrastructure::toString(busyResponse.status) << '\n'
        << "idle_write: " << infrastructure::toString(writeResponse.status) << '\n'
        << "read_while_running: "
        << infrastructure::toString(readResponse.status) << '\n'
        << "stored_remote_run_ms: " << readBack.maximumRemoteRunTimeMs << '\n'
        << "corrupted_frame: "
        << infrastructure::toString(corrupted.decodeStatus) << '\n'
        << "activation: next_boot\n"
        << "scenario_result: " << (expectedOutcome ? "PASS" : "FAIL") << '\n';
    return expectedOutcome ? 0 : 2;
}

int runLockReplayGuardScenario() {
    application::LockCommandGate productionGate{};
    application::LockCommandEvidence syntheticEvidence{
        application::LockCommandSource::SyntheticTest,
        application::LockCommandTrust::Verified,
        1U,
        1'000U,
        true};
    const application::LockCommandDecision syntheticRejected =
        productionGate.process(syntheticEvidence, 1'000U);

    application::LockCommandGateConfig simulationConfig{};
    simulationConfig.allowSyntheticSource = true;
    application::LockCommandGate gate{simulationConfig};

    application::LockCommandEvidence candidateEvidence = syntheticEvidence;
    candidateEvidence.trust = application::LockCommandTrust::Candidate;
    const application::LockCommandDecision candidateRejected =
        gate.process(candidateEvidence, 1'000U);

    syntheticEvidence.observedAtMs = 1'000U;
    const application::LockCommandDecision staleRejected =
        gate.process(syntheticEvidence, 1'600U);

    syntheticEvidence.observedAtMs = 2'000U;
    const application::LockCommandDecision firstAccepted =
        gate.process(syntheticEvidence, 2'000U);

    syntheticEvidence.observedAtMs = 2'100U;
    const application::LockCommandDecision duplicateRejected =
        gate.process(syntheticEvidence, 2'100U);

    syntheticEvidence.sourceSequence = 0U;
    syntheticEvidence.observedAtMs = 2'200U;
    const application::LockCommandDecision outOfOrderRejected =
        gate.process(syntheticEvidence, 2'200U);

    gate.reset();
    application::LockCommandDecision completed{};
    for (std::uint32_t press = 0U; press < 3U; ++press) {
        const std::uint32_t timestampMs = 3'000U + press * 600U;
        syntheticEvidence.sourceSequence = 10U + press;
        syntheticEvidence.observedAtMs = timestampMs;
        completed = gate.process(syntheticEvidence, timestampMs);
    }

    const bool expectedOutcome =
        syntheticRejected.status ==
            application::LockCommandStatus::RejectedSyntheticSource &&
        candidateRejected.status ==
            application::LockCommandStatus::RejectedUntrustedSource &&
        staleRejected.status ==
            application::LockCommandStatus::RejectedStaleEvidence &&
        firstAccepted.status == application::LockCommandStatus::PressAccepted &&
        duplicateRejected.status ==
            application::LockCommandStatus::RejectedDuplicateSequence &&
        outOfOrderRejected.status ==
            application::LockCommandStatus::RejectedOutOfOrderSequence &&
        completed.remoteStartRequested();

    std::cout
        << "scenario: lock-replay-guard\n"
        << "production_synthetic: "
        << application::toString(syntheticRejected.status) << '\n'
        << "candidate_source: "
        << application::toString(candidateRejected.status) << '\n'
        << "stale_evidence: " << application::toString(staleRejected.status) << '\n'
        << "duplicate_counter: "
        << application::toString(duplicateRejected.status) << '\n'
        << "out_of_order_counter: "
        << application::toString(outOfOrderRejected.status) << '\n'
        << "valid_sequence: " << application::toString(completed.status) << '\n'
        << "scenario_result: " << (expectedOutcome ? "PASS" : "FAIL") << '\n';
    return expectedOutcome ? 0 : 2;
}

infrastructure::CanLockCommandAdapterConfig simulatedLockAdapterConfig(
    const application::LockCommandTrust trust) noexcept {
    infrastructure::CanLockCommandAdapterConfig config{};
    config.enabled = true;
    config.trust = trust;
    config.identifier = 0x321U;
    config.dataLength = 2U;
    config.lockCommand = infrastructure::CanBitMatcher{0U, 0x01U, 0x01U};
    config.vehicleSecured =
        infrastructure::CanBitMatcher{0U, 0x02U, 0x02U};
    config.rollingCounter = infrastructure::CanCounterField{1U, 0x0FU};
    return config;
}

infrastructure::CanFrame simulatedLockFrame(
    const std::uint32_t timestampMs,
    const std::uint8_t counter,
    const bool commandActive) noexcept {
    infrastructure::CanFrame frame{};
    frame.timestampMs = timestampMs;
    frame.identifier = 0x321U;
    frame.dataLength = 2U;
    frame.data[0U] = static_cast<std::uint8_t>(
        0x02U | (commandActive ? 0x01U : 0x00U));
    frame.data[1U] = static_cast<std::uint8_t>(counter & 0x0FU);
    return frame;
}

int runQualifiedLockAdapterScenario() {
    infrastructure::CanLockCommandPipeline candidate{
        simulatedLockAdapterConfig(application::LockCommandTrust::Candidate)};
    static_cast<void>(candidate.process(simulatedLockFrame(0U, 0U, false), 0U));
    const infrastructure::CanLockPipelineResult candidateResult =
        candidate.process(simulatedLockFrame(1'000U, 1U, true), 1'000U);

    infrastructure::CanLockCommandPipeline verified{
        simulatedLockAdapterConfig(application::LockCommandTrust::Verified)};
    static_cast<void>(verified.process(simulatedLockFrame(0U, 14U, false), 0U));
    const infrastructure::CanLockPipelineResult first =
        verified.process(simulatedLockFrame(1'000U, 15U, true), 1'000U);
    const infrastructure::CanLockPipelineResult held =
        verified.process(simulatedLockFrame(1'100U, 0U, true), 1'100U);
    static_cast<void>(
        verified.process(simulatedLockFrame(1'200U, 1U, false), 1'200U));
    const infrastructure::CanLockPipelineResult second =
        verified.process(simulatedLockFrame(1'600U, 2U, true), 1'600U);
    const infrastructure::CanLockPipelineResult replay =
        verified.process(simulatedLockFrame(1'700U, 2U, true), 1'700U);
    const std::uint8_t pressesAfterReplay = verified.pressCount();

    static_cast<void>(
        verified.process(simulatedLockFrame(2'000U, 3U, false), 2'000U));
    static_cast<void>(
        verified.process(simulatedLockFrame(3'000U, 4U, true), 3'000U));
    static_cast<void>(
        verified.process(simulatedLockFrame(3'100U, 5U, false), 3'100U));
    static_cast<void>(
        verified.process(simulatedLockFrame(3'600U, 6U, true), 3'600U));
    static_cast<void>(
        verified.process(simulatedLockFrame(3'700U, 7U, false), 3'700U));
    const infrastructure::CanLockPipelineResult completed =
        verified.process(simulatedLockFrame(4'200U, 8U, true), 4'200U);

    const bool expectedOutcome =
        candidateResult.commandEvaluated &&
        candidateResult.command.status ==
            application::LockCommandStatus::RejectedUntrustedSource &&
        first.command.status == application::LockCommandStatus::PressAccepted &&
        held.decodeStatus == infrastructure::CanLockDecodeStatus::NoCommand &&
        !held.commandEvaluated &&
        second.command.status == application::LockCommandStatus::PressAccepted &&
        replay.decodeStatus ==
            infrastructure::CanLockDecodeStatus::RejectedDuplicateCounter &&
        pressesAfterReplay == 0U && completed.command.remoteStartRequested();

    std::cout
        << "scenario: qualified-lock-adapter\n"
        << "binding: simulated_test_vector_not_bmw\n"
        << "candidate_binding: "
        << application::toString(candidateResult.command.status) << '\n'
        << "first_edge: " << application::toString(first.command.status) << '\n'
        << "held_command: " << infrastructure::toString(held.decodeStatus) << '\n'
        << "second_edge: " << application::toString(second.command.status) << '\n'
        << "replayed_counter: " << infrastructure::toString(replay.decodeStatus)
        << '\n'
        << "presses_after_replay: "
        << static_cast<unsigned int>(pressesAfterReplay) << '\n'
        << "fresh_three_edges: "
        << application::toString(completed.command.status) << '\n'
        << "scenario_result: " << (expectedOutcome ? "PASS" : "FAIL") << '\n';
    return expectedOutcome ? 0 : 2;
}

int runActuatorSupervisorScenario() {
    SimulatedActuatorDriver driver{};
    infrastructure::ActuatorSafetySupervisor supervisor{driver};

    const bool initializedSafe =
        supervisor.status().healthy() && !driver.ignitionActive &&
        !driver.starterActive;
    const bool armed =
        supervisor.heartbeat(0U) &&
        supervisor.poll(
            0U,
            true,
            infrastructure::ActuatorFeedback{true, false, false})
            .healthy();
    const bool ignitionEnabled = supervisor.enableIgnition();
    const bool ignitionConfirmed =
        supervisor.heartbeat(101U) &&
        supervisor.poll(
            101U,
            true,
            infrastructure::ActuatorFeedback{true, true, false})
            .ignitionFeedbackConfirmed;
    const bool starterEngaged = supervisor.engageStarter();
    const bool crankingConfirmed =
        supervisor.heartbeat(200U) &&
        supervisor.poll(
            200U,
            true,
            infrastructure::ActuatorFeedback{true, true, true})
            .healthy();

    const infrastructure::ActuatorSupervisorStatus watchdog = supervisor.poll(
        701U,
        true,
        infrastructure::ActuatorFeedback{true, true, true});
    const bool watchdogSafed =
        watchdog.fault ==
            infrastructure::ActuatorSupervisorFault::WatchdogExpired &&
        !driver.ignitionActive && !driver.starterActive;

    const bool reset = supervisor.resetFault(
        800U, infrastructure::ActuatorFeedback{true, false, false});
    const bool rearmed =
        supervisor.heartbeat(900U) &&
        supervisor.poll(
            900U,
            true,
            infrastructure::ActuatorFeedback{true, false, false})
            .healthy() &&
        supervisor.enableIgnition();
    const bool heartbeatBeforeMismatch = supervisor.heartbeat(1'001U);
    const infrastructure::ActuatorSupervisorStatus mismatch = supervisor.poll(
        1'001U,
        true,
        infrastructure::ActuatorFeedback{true, false, false});
    const bool mismatchSafed =
        heartbeatBeforeMismatch &&
        mismatch.fault ==
            infrastructure::ActuatorSupervisorFault::FeedbackMismatch &&
        !driver.ignitionActive && !driver.starterActive;

    const bool expectedOutcome =
        initializedSafe && armed && ignitionEnabled && ignitionConfirmed &&
        starterEngaged && crankingConfirmed && watchdogSafed && reset &&
        rearmed && mismatchSafed;

    std::cout
        << "scenario: actuator-supervisor\n"
        << "driver: simulated_no_gpio\n"
        << "initial_outputs: " << (initializedSafe ? "safe" : "unsafe") << '\n'
        << "nominal_sequence: "
        << (crankingConfirmed ? "confirmed" : "rejected") << '\n'
        << "watchdog_injection: "
        << infrastructure::toString(watchdog.fault) << '\n'
        << "watchdog_outputs: "
        << (watchdogSafed ? "safe" : "unsafe") << '\n'
        << "guarded_reset: " << (reset ? "accepted" : "rejected") << '\n'
        << "feedback_injection: "
        << infrastructure::toString(mismatch.fault) << '\n'
        << "feedback_outputs: "
        << (mismatchSafed ? "safe" : "unsafe") << '\n'
        << "scenario_result: " << (expectedOutcome ? "PASS" : "FAIL") << '\n';
    return expectedOutcome ? 0 : 2;
}

int runSupervisedRuntimeScenario() {
    const application::UserConfiguration configuration =
        application::makeUserConfiguration(
            application::UserSettings{},
            &simulation::syntheticVehicleProfile());

    simulation::SyntheticPowertrainState stopped{};
    simulation::SyntheticPowertrainState running{};
    running.engineRpm = 850U;
    const simulation::SyntheticBodyState safeBody{};
    const std::array<infrastructure::CanFrame, 4U> trace = {
        simulation::makeSyntheticPowertrainFrame(0U, stopped),
        simulation::makeSyntheticBodyFrame(0U, safeBody),
        simulation::makeSyntheticPowertrainFrame(1'800U, running),
        simulation::makeSyntheticBodyFrame(1'800U, safeBody),
    };

    simulation::SyntheticCanDecoder decoder{};
    infrastructure::ReplayVehicleGateway gateway{
        trace.data(), trace.size(), decoder};
    SimulatedActuatorDriver driver{};
    infrastructure::ActuatorSafetySupervisor supervisor{driver};
    ConsoleTimer timer{};
    ConsoleNotifications notifications{};
    infrastructure::DiagnosticJournal diagnosticJournal{};
    infrastructure::Runtime runtime{
        application::Controller{configuration.controller},
        gateway,
        supervisor,
        timer,
        notifications,
        &diagnosticJournal};

    std::cout
        << "scenario: supervised-runtime\n"
        << "driver: simulated_no_gpio\n";

    if (!configuration.validation.valid() || !gateway.setElapsedTime(0U) ||
        !supervisor.heartbeat(0U) ||
        !supervisor.poll(0U, true, driver.feedback()).healthy()) {
        std::cout << "scenario_result: FAIL\n";
        return 2;
    }

    domain::VehicleState vehicle{};
    auto decision = runtime.dispatch(
        application::Event{application::EventType::RemoteStartRequested},
        vehicle,
        0U);
    printDecision("remote start", decision);
    const bool authorizing =
        decision.state == application::ControllerState::Authorizing;

    vehicle = gateway.state();
    decision = runtime.dispatch(
        application::Event{application::EventType::VehicleStateUpdated},
        vehicle,
        0U);
    printDecision("safe snapshot", decision);
    const bool ignitionRequested =
        decision.state == application::ControllerState::Preparing &&
        driver.ignitionActive && !driver.starterActive;

    const bool ignitionConfirmed =
        supervisor.heartbeat(101U) &&
        supervisor.poll(101U, true, driver.feedback())
            .ignitionFeedbackConfirmed;
    const std::array<std::uint32_t, 4U> preparationServiceTimes = {
        500U, 900U, 1'300U, 1'500U};
    bool supervisionHealthy = true;
    for (const std::uint32_t nowMs : preparationServiceTimes) {
        supervisionHealthy =
            supervisor.heartbeat(nowMs) &&
            supervisor.poll(nowMs, true, driver.feedback()).healthy() &&
            supervisionHealthy;
    }
    decision = runtime.dispatch(
        application::Event{application::EventType::TimerElapsed},
        vehicle,
        1'500U);
    printDecision("preparation timer", decision);
    const bool starterRequested =
        decision.state == application::ControllerState::Cranking &&
        driver.ignitionActive && driver.starterActive;

    const bool starterConfirmed =
        supervisor.heartbeat(1'601U) &&
        supervisor.poll(1'601U, true, driver.feedback()).healthy();
    const std::array<std::uint32_t, 2U> crankingServiceTimes = {
        1'700U, 1'800U};
    for (const std::uint32_t nowMs : crankingServiceTimes) {
        supervisionHealthy =
            supervisor.heartbeat(nowMs) &&
            supervisor.poll(nowMs, true, driver.feedback()).healthy() &&
            supervisionHealthy;
    }

    if (!gateway.setElapsedTime(1'800U) || !gateway.requestState()) {
        std::cout << "scenario_result: FAIL\n";
        return 2;
    }
    vehicle = gateway.state();
    decision = runtime.dispatch(
        application::Event{application::EventType::VehicleStateUpdated},
        vehicle,
        1'800U);
    printDecision("engine running", decision);
    const bool runningConfirmed =
        decision.state == application::ControllerState::Running &&
        driver.ignitionActive && !driver.starterActive;

    const bool runningOutputsConfirmed =
        supervisor.heartbeat(1'901U) &&
        supervisor.poll(1'901U, true, driver.feedback()).healthy();
    const infrastructure::ActuatorSupervisorStatus watchdog = supervisor.poll(
        2'402U, true, driver.feedback());
    decision = runtime.dispatch(
        application::Event::infrastructureFailure(
            application::FaultCode::ActuatorFailure),
        vehicle,
        2'402U);
    printDecision("supervisor fault", decision);

    const bool faultPropagated =
        watchdog.fault ==
            infrastructure::ActuatorSupervisorFault::WatchdogExpired &&
        decision.state == application::ControllerState::Fault &&
        decision.fault == application::FaultCode::ActuatorFailure &&
        !driver.ignitionActive && !driver.starterActive;
    const bool expectedOutcome =
        authorizing && ignitionRequested && ignitionConfirmed &&
        starterRequested && starterConfirmed && supervisionHealthy &&
        runningConfirmed && runningOutputsConfirmed && faultPropagated;

    std::cout
        << "ignition_feedback: "
        << (ignitionConfirmed ? "confirmed" : "missing") << '\n'
        << "starter_feedback: "
        << (starterConfirmed ? "confirmed" : "missing") << '\n'
        << "watchdog_injection: "
        << infrastructure::toString(watchdog.fault) << '\n'
        << "controller_fault: "
        << application::toString(decision.fault) << '\n'
        << "final_outputs: "
        << (!driver.ignitionActive && !driver.starterActive
                ? "safe"
                : "unsafe")
        << '\n';
    return finishRuntimeScenario(
        expectedOutcome, gateway, diagnosticJournal);
}

int runScenario(
    const Scenario scenario,
    const application::UserSettings* const suppliedSettings = nullptr) {
    if (scenario == Scenario::SettingsRecovery) {
        return runSettingsRecoveryScenario();
    }
    if (scenario == Scenario::SettingsLink) {
        return runSettingsLinkScenario();
    }
    if (scenario == Scenario::LockReplayGuard) {
        return runLockReplayGuardScenario();
    }
    if (scenario == Scenario::QualifiedLockAdapter) {
        return runQualifiedLockAdapterScenario();
    }
    if (scenario == Scenario::ActuatorSupervisor) {
        return runActuatorSupervisorScenario();
    }
    if (scenario == Scenario::SupervisedRuntime) {
        return runSupervisedRuntimeScenario();
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
    const bool injectSignalLoss = scenario == Scenario::SignalLoss;
    const bool injectFrameCorruption = scenario == Scenario::FrameCorruption;
    if (injectHoodOpening) {
        finalBody.hoodClosed = false;
    }
    if (injectDriverEntry) {
        finalBody.doorsClosed = false;
    }

    std::array<infrastructure::CanFrame, 8U> trace{};
    std::size_t traceSize = 0U;
    trace[traceSize++] =
        simulation::makeSyntheticPowertrainFrame(0U, stopped);
    infrastructure::CanFrame initialBody =
        simulation::makeSyntheticBodyFrame(0U, safeBody);
    if (injectFrameCorruption) {
        initialBody.data[7U] ^= 0x01U;
    }
    trace[traceSize++] = initialBody;
    trace[traceSize++] =
        simulation::makeSyntheticPowertrainFrame(1'800U, running);
    trace[traceSize++] =
        simulation::makeSyntheticBodyFrame(1'800U, safeBody);
    trace[traceSize++] =
        simulation::makeSyntheticPowertrainFrame(5'000U, running);
    if (!injectSignalLoss) {
        trace[traceSize++] =
            simulation::makeSyntheticBodyFrame(5'000U, finalBody);
    }
    trace[traceSize++] =
        simulation::makeSyntheticPowertrainFrame(6'000U, stopped);
    trace[traceSize++] =
        simulation::makeSyntheticBodyFrame(6'000U, safeBody);

    simulation::SyntheticCanDecoder decoder{};
    infrastructure::ReplayVehicleGateway gateway{
        trace.data(), traceSize, decoder};
    ConsoleActuators actuators{};
    ConsoleTimer timer{};
    ConsoleNotifications notifications{};
    infrastructure::DiagnosticJournal diagnosticJournal{};
    infrastructure::Runtime runtime{
        application::Controller{configuration.controller},
        gateway,
        actuators,
        timer,
        notifications,
        &diagnosticJournal};

    std::cout << "scenario: " << scenarioName(scenario) << '\n';
    printUserSettings(settings);

    domain::VehicleState vehicle{};
    if (!gateway.setElapsedTime(0U)) {
        return 1;
    }
    if (scenario == Scenario::UserConfig) {
        application::LockCommandGateConfig gateConfig{};
        gateConfig.sequence = configuration.lockSequence;
        gateConfig.allowSyntheticSource = true;
        application::LockCommandGate gate{gateConfig};
        std::uint32_t timestampMs = 1'000U;
        application::LockCommandDecision lockDecision{};
        for (std::uint8_t press = 0U; press < settings.lockPressCount; ++press) {
            lockDecision = gate.process(
                application::LockCommandEvidence{
                    application::LockCommandSource::SyntheticTest,
                    application::LockCommandTrust::Verified,
                    static_cast<std::uint32_t>(press) + 1U,
                    timestampMs,
                    true},
                timestampMs);
            timestampMs += settings.lockMinimumGapMs;
        }
        std::cout << "lock_sequence: "
                  << application::toString(lockDecision.status) << '\n';
        if (!lockDecision.remoteStartRequested()) {
            std::cout << "scenario_result: FAIL\n";
            return 2;
        }
    }

    auto decision = runtime.dispatch(
        application::Event{application::EventType::RemoteStartRequested},
        vehicle,
        0U);
    printDecision("remote start", decision);

    if (scenario == Scenario::FrameCorruption) {
        const bool expectedOutcome =
            decision.state == application::ControllerState::Fault &&
            decision.fault ==
                application::FaultCode::VehicleCommunication &&
            decision.contains(application::ActionType::SecureOutputs) &&
            gateway.lastBatch().status ==
                infrastructure::ReplayStatus::ConsumerRejected &&
            gateway.statistics().rejectedFrames == 1U;
        std::cout << "injection: corrupted recognized body frame\n";
        return finishRuntimeScenario(
            expectedOutcome, gateway, diagnosticJournal);
    }

    if (scenario == Scenario::UserConfig && !settings.remoteStartEnabled) {
        const bool expectedOutcome =
            decision.state == application::ControllerState::Idle &&
            decision.contains(application::ActionType::NotifyRemoteStartDisabled);
        return finishRuntimeScenario(
            expectedOutcome, gateway, diagnosticJournal);
    }

    vehicle = gateway.state();
    decision = runtime.dispatch(
        application::Event{application::EventType::VehicleStateUpdated},
        vehicle,
        0U);
    printDecision("safe snapshot", decision);

    if (scenario == Scenario::SignalDelay) {
        if (!gateway.setElapsedTime(
                configuration.controller.preparationDelayMs) ||
            !gateway.requestState()) {
            std::cerr << "Replay failed during delayed update injection\n";
            return 1;
        }
        vehicle = gateway.state();
        std::cout << "injection: no new signals before preparation deadline\n";
    }

    decision = runtime.dispatch(
        application::Event{application::EventType::TimerElapsed},
        vehicle,
        configuration.controller.preparationDelayMs);
    printDecision("preparation timer", decision);

    if (scenario == Scenario::SignalDelay) {
        const bool expectedOutcome =
            decision.state == application::ControllerState::Fault &&
            decision.fault == application::FaultCode::SafetyInterlock &&
            decision.safety.contains(
                application::SafetyReason::SignalUnavailable) &&
            decision.contains(application::ActionType::SecureOutputs) &&
            gateway.lastBatch().emittedFrames == 0U;
        return finishRuntimeScenario(
            expectedOutcome, gateway, diagnosticJournal);
    }

    if (!gateway.setElapsedTime(1'800U) || !gateway.requestState()) {
        std::cerr << "Replay failed at 1800 ms\n";
        return 1;
    }
    vehicle = gateway.state();
    decision = runtime.dispatch(
        application::Event{application::EventType::VehicleStateUpdated},
        vehicle,
        1'800U);
    printDecision("engine running", decision);

    if (!gateway.setElapsedTime(5'000U) || !gateway.requestState()) {
        std::cerr << "Replay failed at 5000 ms\n";
        return 1;
    }
    vehicle = gateway.state();
    decision = runtime.dispatch(
        application::Event{application::EventType::VehicleStateUpdated},
        vehicle,
        5'000U);
    printDecision(
        injectHoodOpening
            ? "hood opened"
            : (injectDriverEntry ? "driver door opened" : "safe running update"),
        decision);

    bool expectedOutcome = false;
    if (scenario == Scenario::Nominal) {
        decision = runtime.dispatch(
            application::Event{application::EventType::RemoteStopRequested},
            vehicle,
            5'000U);
        printDecision("remote stop", decision);

        if (!gateway.setElapsedTime(6'000U) || !gateway.requestState()) {
            std::cerr << "Replay failed at 6000 ms\n";
            return 1;
        }
        vehicle = gateway.state();
        decision = runtime.dispatch(
            application::Event{application::EventType::VehicleStateUpdated},
            vehicle,
            6'000U);
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
    } else if (scenario == Scenario::SignalLoss) {
        expectedOutcome =
            decision.state == application::ControllerState::Fault &&
            decision.fault == application::FaultCode::SafetyInterlock &&
            decision.safety.contains(
                application::SafetyReason::SignalUnavailable) &&
            decision.contains(application::ActionType::SecureOutputs) &&
            vehicle.doorsClosed.quality == domain::SignalQuality::Stale;
    } else if (scenario == Scenario::TakeoverTimeout) {
        decision = runtime.dispatch(
            application::Event{application::EventType::TimerElapsed},
            vehicle,
            5'000U + configuration.controller.driverTakeoverTimeoutMs);
        printDecision("takeover timer expired", decision);
        expectedOutcome =
            decision.state == application::ControllerState::Stopping &&
            decision.fault == application::FaultCode::None;
    } else if (scenario == Scenario::TakeoverConfirmed) {
        decision = runtime.dispatch(
            application::Event{application::EventType::DriverTakeoverConfirmed},
            vehicle,
            5'000U);
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

    return finishRuntimeScenario(
        expectedOutcome, gateway, diagnosticJournal);
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
        << "6. Perte des mises a jour carrosserie\n"
        << "7. Retard des signaux avant lancement\n"
        << "8. Corruption d'une trame reconnue\n"
        << "9. Tester un fichier de configuration utilisateur\n"
        << "10. Recuperation d'une configuration persistante corrompue\n"
        << "11. Liaison de configuration securisee simulee\n"
        << "12. Garde anti-rejeu des commandes de verrouillage\n"
        << "13. Adaptateur CAN qualifie sur vecteur de test\n"
        << "14. Superviseur d'actionneurs et pannes injectees\n"
        << "15. Chaine complete runtime et superviseur\n"
        << "16. Quitter\n\n"
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
        case 6U: result = runScenario(Scenario::SignalLoss); break;
        case 7U: result = runScenario(Scenario::SignalDelay); break;
        case 8U: result = runScenario(Scenario::FrameCorruption); break;
        case 9U: {
            std::cout << "Configuration path: ";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::string path{};
            std::getline(std::cin, path);
            inputLineConsumed = true;
            result = runUserConfiguration(path.c_str());
            break;
        }
        case 10U: result = runScenario(Scenario::SettingsRecovery); break;
        case 11U: result = runScenario(Scenario::SettingsLink); break;
        case 12U: result = runScenario(Scenario::LockReplayGuard); break;
        case 13U: result = runScenario(Scenario::QualifiedLockAdapter); break;
        case 14U: result = runScenario(Scenario::ActuatorSupervisor); break;
        case 15U: result = runScenario(Scenario::SupervisedRuntime); break;
        case 16U: return 0;
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
    if (command == "--list-features" && argumentCount == 2) {
        printFeatureList();
        return 0;
    }
    if (command == "--sandbox" && argumentCount == 2) {
        return host::runSandboxProtocol(std::cin, std::cout);
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
