#include "bmw_remote/infrastructure/diagnostic_journal.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace bmw::remote::infrastructure {
namespace {

[[nodiscard]] bool isCommand(
    const application::EventType type) noexcept {
    using application::EventType;
    switch (type) {
        case EventType::RemoteStartRequested:
        case EventType::RemoteStopRequested:
        case EventType::DriverTakeoverConfirmed:
        case EventType::ResetRequested:
            return true;

        case EventType::VehicleStateUpdated:
        case EventType::TimerElapsed:
        case EventType::InfrastructureFailure:
            return false;
    }
    return false;
}

[[nodiscard]] DiagnosticReason rejectionReason(
    const application::Decision& decision) noexcept {
    using application::ActionType;
    if (decision.contains(ActionType::NotifyRemoteStartDisabled)) {
        return DiagnosticReason::RemoteStartDisabled;
    }
    if (decision.contains(ActionType::NotifyProfileRejected)) {
        return DiagnosticReason::ProfileNotReady;
    }
    if (decision.contains(ActionType::NotifyStartRejected)) {
        return DiagnosticReason::SafetyPolicy;
    }
    if (decision.contains(ActionType::NotifyResetRejected)) {
        return DiagnosticReason::UnsafeReset;
    }
    if (decision.contains(ActionType::NotifyRequestIgnored)) {
        return DiagnosticReason::InvalidState;
    }
    return DiagnosticReason::None;
}

[[nodiscard]] DiagnosticRecord makeRecord(
    const DiagnosticRecordType type,
    const application::EventType trigger,
    const application::Decision& decision,
    const DiagnosticReason reason,
    const std::uint32_t timestampMs) noexcept {
    DiagnosticRecord record{};
    record.timestampMs = timestampMs;
    record.safetyReasons = decision.safety.reasons;
    record.type = type;
    record.trigger = trigger;
    record.previousState = decision.previousState;
    record.state = decision.state;
    record.fault = decision.fault;
    record.reason = reason;
    record.profileReasons = decision.profileReadiness.reasons;
    return record;
}

}  // namespace

void DiagnosticJournal::observe(
    const application::Event event,
    const application::Decision& decision,
    const std::uint32_t timestampMs) noexcept {
    if (isCommand(event.type)) {
        DiagnosticRecord command = makeRecord(
            DiagnosticRecordType::CommandReceived,
            event.type,
            decision,
            DiagnosticReason::None,
            timestampMs);
        command.state = decision.previousState;
        command.fault = decision.previousFault;
        append(command);
    }

    if (decision.previousState != decision.state) {
        append(makeRecord(
            DiagnosticRecordType::StateTransition,
            event.type,
            decision,
            DiagnosticReason::None,
            timestampMs));
    }

    const DiagnosticReason rejected = rejectionReason(decision);
    if (rejected != DiagnosticReason::None) {
        append(makeRecord(
            DiagnosticRecordType::RequestRejected,
            event.type,
            decision,
            rejected,
            timestampMs));
    }

    if (decision.state == application::ControllerState::Fault &&
        decision.previousState != application::ControllerState::Fault) {
        const DiagnosticReason faultReason =
            decision.fault == application::FaultCode::SafetyInterlock
                ? DiagnosticReason::SafetyPolicy
                : DiagnosticReason::None;
        append(makeRecord(
            DiagnosticRecordType::FaultEntered,
            event.type,
            decision,
            faultReason,
            timestampMs));
    }
}

void DiagnosticJournal::recordInfrastructureFailure(
    const application::EventType trigger,
    const application::ControllerState state,
    const application::FaultCode fault,
    const std::uint32_t timestampMs) noexcept {
    recordFailure(
        DiagnosticRecordType::InfrastructureFailure,
        trigger,
        state,
        fault,
        timestampMs);
}

void DiagnosticJournal::recordSafingFailure(
    const application::ControllerState state,
    const application::FaultCode fault,
    const std::uint32_t timestampMs) noexcept {
    recordFailure(
        DiagnosticRecordType::SafingFailure,
        application::EventType::InfrastructureFailure,
        state,
        fault,
        timestampMs);
}

bool DiagnosticJournal::read(
    const std::size_t chronologicalIndex,
    DiagnosticRecord& record) const noexcept {
    if (chronologicalIndex >= size_) {
        return false;
    }
    const std::size_t storageIndex =
        (oldestIndex_ + chronologicalIndex) % records_.size();
    record = records_[storageIndex];
    return true;
}

void DiagnosticJournal::clear() noexcept {
    records_.fill(DiagnosticRecord{});
    oldestIndex_ = 0U;
    size_ = 0U;
    nextSequence_ = 1U;
    overwrittenCount_ = 0U;
}

void DiagnosticJournal::append(DiagnosticRecord record) noexcept {
    record.sequence = nextSequence_;
    ++nextSequence_;

    std::size_t storageIndex = 0U;
    if (size_ < records_.size()) {
        storageIndex = (oldestIndex_ + size_) % records_.size();
        ++size_;
    } else {
        storageIndex = oldestIndex_;
        oldestIndex_ = (oldestIndex_ + 1U) % records_.size();
        if (overwrittenCount_ < std::numeric_limits<std::uint32_t>::max()) {
            ++overwrittenCount_;
        }
    }
    records_[storageIndex] = record;
}

void DiagnosticJournal::recordFailure(
    const DiagnosticRecordType type,
    const application::EventType trigger,
    const application::ControllerState state,
    const application::FaultCode fault,
    const std::uint32_t timestampMs) noexcept {
    DiagnosticRecord record{};
    record.timestampMs = timestampMs;
    record.type = type;
    record.trigger = trigger;
    record.previousState = state;
    record.state = state;
    record.fault = fault;
    append(record);
}

const char* toString(const DiagnosticRecordType type) noexcept {
    switch (type) {
        case DiagnosticRecordType::CommandReceived: return "command_received";
        case DiagnosticRecordType::StateTransition: return "state_transition";
        case DiagnosticRecordType::RequestRejected: return "request_rejected";
        case DiagnosticRecordType::FaultEntered: return "fault_entered";
        case DiagnosticRecordType::InfrastructureFailure:
            return "infrastructure_failure";
        case DiagnosticRecordType::SafingFailure: return "safing_failure";
    }
    return "unknown";
}

const char* toString(const DiagnosticReason reason) noexcept {
    switch (reason) {
        case DiagnosticReason::None: return "none";
        case DiagnosticReason::RemoteStartDisabled:
            return "remote_start_disabled";
        case DiagnosticReason::ProfileNotReady: return "profile_not_ready";
        case DiagnosticReason::SafetyPolicy: return "safety_policy";
        case DiagnosticReason::InvalidState: return "invalid_state";
        case DiagnosticReason::UnsafeReset: return "unsafe_reset";
    }
    return "unknown";
}

}  // namespace bmw::remote::infrastructure
