#include "bmw_remote/application/lock_command_gate.hpp"

#include <cstdint>

namespace bmw::remote::application {
namespace {

constexpr std::uint32_t HalfCounterRange = 0x80000000U;

[[nodiscard]] bool strictlyNewer(
    const std::uint32_t candidate,
    const std::uint32_t previous) noexcept {
    const std::uint32_t distance = candidate - previous;
    return distance != 0U && distance < HalfCounterRange;
}

[[nodiscard]] bool isFuture(
    const std::uint32_t timestamp,
    const std::uint32_t nowMs) noexcept {
    return timestamp != nowMs && strictlyNewer(timestamp, nowMs);
}

}  // namespace

LockCommandDecision LockCommandGate::process(
    const LockCommandEvidence evidence,
    const std::uint32_t nowMs) noexcept {
    if (!configIsValid()) {
        return reject(LockCommandStatus::RejectedInvalidConfiguration);
    }

    if (clockInitialized_ && nowMs != lastNowMs_ &&
        !strictlyNewer(nowMs, lastNowMs_)) {
        return reject(LockCommandStatus::RejectedClockRegression);
    }
    lastNowMs_ = nowMs;
    clockInitialized_ = true;

    if (evidence.trust != LockCommandTrust::Verified ||
        evidence.source == LockCommandSource::Unspecified) {
        return reject(LockCommandStatus::RejectedUntrustedSource);
    }
    if (evidence.source == LockCommandSource::SyntheticTest &&
        !config_.allowSyntheticSource) {
        return reject(LockCommandStatus::RejectedSyntheticSource);
    }
    if (!evidence.vehicleSecured) {
        return reject(LockCommandStatus::RejectedVehicleNotSecured);
    }
    if (isFuture(evidence.observedAtMs, nowMs)) {
        return reject(LockCommandStatus::RejectedFutureEvidence);
    }
    if (nowMs - evidence.observedAtMs > config_.maximumEvidenceAgeMs) {
        return reject(LockCommandStatus::RejectedStaleEvidence);
    }

    if (evidenceInitialized_) {
        if (evidence.sourceSequence == lastSourceSequence_) {
            return reject(LockCommandStatus::RejectedDuplicateSequence);
        }
        if (!strictlyNewer(
                evidence.sourceSequence, lastSourceSequence_)) {
            return reject(LockCommandStatus::RejectedOutOfOrderSequence);
        }
        if (evidence.observedAtMs == lastEvidenceTimestampMs_ ||
            !strictlyNewer(
                evidence.observedAtMs, lastEvidenceTimestampMs_)) {
            return reject(LockCommandStatus::RejectedNonMonotonicEvidence);
        }
    }

    lastSourceSequence_ = evidence.sourceSequence;
    lastEvidenceTimestampMs_ = evidence.observedAtMs;
    evidenceInitialized_ = true;

    switch (detector_.observe(evidence.observedAtMs)) {
        case LockPressResult::Accepted:
            return {LockCommandStatus::PressAccepted};
        case LockPressResult::SequenceCompleted:
            return {LockCommandStatus::RemoteStartRequested};
        case LockPressResult::SequenceRestarted:
            return {LockCommandStatus::SequenceRestarted};
        case LockPressResult::IgnoredDebounce:
            return {LockCommandStatus::RejectedTiming};
        case LockPressResult::InvalidConfiguration:
            return reject(LockCommandStatus::RejectedInvalidConfiguration);
    }
    return reject(LockCommandStatus::RejectedInvalidConfiguration);
}

void LockCommandGate::reset() noexcept {
    detector_.reset();
    lastNowMs_ = 0U;
    lastSourceSequence_ = 0U;
    lastEvidenceTimestampMs_ = 0U;
    clockInitialized_ = false;
    evidenceInitialized_ = false;
}

bool LockCommandGate::configIsValid() const noexcept {
    const LockSequenceConfig& sequence = config_.sequence;
    const std::uint32_t requiredIntervals =
        sequence.requiredPresses == 0U
            ? 0U
            : static_cast<std::uint32_t>(sequence.requiredPresses - 1U);
    const std::uint64_t minimumSequenceTime =
        static_cast<std::uint64_t>(requiredIntervals) * sequence.minimumGapMs;
    return sequence.requiredPresses != 0U &&
           sequence.minimumGapMs <= sequence.maximumGapMs &&
           sequence.maximumGapMs <= sequence.maximumSequenceMs &&
           minimumSequenceTime <= sequence.maximumSequenceMs &&
           config_.maximumEvidenceAgeMs != 0U &&
           config_.maximumEvidenceAgeMs <=
               LockCommandGateConfig::MaximumEvidenceAgeLimitMs;
}

LockCommandDecision LockCommandGate::reject(
    const LockCommandStatus status) noexcept {
    detector_.reset();
    return {status};
}

const char* toString(const LockCommandSource source) noexcept {
    switch (source) {
        case LockCommandSource::Unspecified: return "unspecified";
        case LockCommandSource::VehicleAdapter: return "vehicle_adapter";
        case LockCommandSource::SyntheticTest: return "synthetic_test";
    }
    return "unknown";
}

const char* toString(const LockCommandTrust trust) noexcept {
    switch (trust) {
        case LockCommandTrust::Unverified: return "unverified";
        case LockCommandTrust::Candidate: return "candidate";
        case LockCommandTrust::Verified: return "verified";
    }
    return "unknown";
}

const char* toString(const LockCommandStatus status) noexcept {
    switch (status) {
        case LockCommandStatus::PressAccepted: return "press_accepted";
        case LockCommandStatus::SequenceRestarted: return "sequence_restarted";
        case LockCommandStatus::RemoteStartRequested:
            return "remote_start_requested";
        case LockCommandStatus::RejectedInvalidConfiguration:
            return "rejected_invalid_configuration";
        case LockCommandStatus::RejectedUntrustedSource:
            return "rejected_untrusted_source";
        case LockCommandStatus::RejectedSyntheticSource:
            return "rejected_synthetic_source";
        case LockCommandStatus::RejectedVehicleNotSecured:
            return "rejected_vehicle_not_secured";
        case LockCommandStatus::RejectedStaleEvidence:
            return "rejected_stale_evidence";
        case LockCommandStatus::RejectedFutureEvidence:
            return "rejected_future_evidence";
        case LockCommandStatus::RejectedDuplicateSequence:
            return "rejected_duplicate_sequence";
        case LockCommandStatus::RejectedOutOfOrderSequence:
            return "rejected_out_of_order_sequence";
        case LockCommandStatus::RejectedNonMonotonicEvidence:
            return "rejected_non_monotonic_evidence";
        case LockCommandStatus::RejectedClockRegression:
            return "rejected_clock_regression";
        case LockCommandStatus::RejectedTiming: return "rejected_timing";
    }
    return "unknown";
}

}  // namespace bmw::remote::application
