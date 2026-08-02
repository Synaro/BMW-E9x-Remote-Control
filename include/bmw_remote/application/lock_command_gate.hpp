#pragma once

#include <cstdint>

#include "bmw_remote/application/lock_sequence_detector.hpp"

namespace bmw::remote::application {

enum class LockCommandSource : std::uint8_t {
    Unspecified,
    VehicleAdapter,
    SyntheticTest,
};

enum class LockCommandTrust : std::uint8_t {
    Unverified,
    Candidate,
    Verified,
};

struct LockCommandEvidence final {
    LockCommandSource source{LockCommandSource::Unspecified};
    LockCommandTrust trust{LockCommandTrust::Unverified};
    std::uint32_t sourceSequence{0U};
    std::uint32_t observedAtMs{0U};
    bool vehicleSecured{false};
};

enum class LockCommandStatus : std::uint8_t {
    PressAccepted,
    SequenceRestarted,
    RemoteStartRequested,
    RejectedInvalidConfiguration,
    RejectedUntrustedSource,
    RejectedSyntheticSource,
    RejectedVehicleNotSecured,
    RejectedStaleEvidence,
    RejectedFutureEvidence,
    RejectedDuplicateSequence,
    RejectedOutOfOrderSequence,
    RejectedNonMonotonicEvidence,
    RejectedClockRegression,
    RejectedTiming,
};

struct LockCommandDecision final {
    LockCommandStatus status{LockCommandStatus::RejectedUntrustedSource};

    [[nodiscard]] constexpr bool accepted() const noexcept {
        return status == LockCommandStatus::PressAccepted ||
               status == LockCommandStatus::SequenceRestarted ||
               status == LockCommandStatus::RemoteStartRequested;
    }

    [[nodiscard]] constexpr bool remoteStartRequested() const noexcept {
        return status == LockCommandStatus::RemoteStartRequested;
    }
};

struct LockCommandGateConfig final {
    static constexpr std::uint32_t MaximumEvidenceAgeLimitMs = 5'000U;

    LockSequenceConfig sequence{};
    std::uint32_t maximumEvidenceAgeMs{500U};
    bool allowSyntheticSource{false};
};

class LockCommandGate final {
public:
    explicit constexpr LockCommandGate(
        const LockCommandGateConfig config = {}) noexcept
        : config_(config), detector_(config.sequence) {}

    [[nodiscard]] LockCommandDecision process(
        LockCommandEvidence evidence,
        std::uint32_t nowMs) noexcept;

    void reset() noexcept;

    [[nodiscard]] constexpr std::uint8_t pressCount() const noexcept {
        return detector_.pressCount();
    }

private:
    [[nodiscard]] bool configIsValid() const noexcept;
    [[nodiscard]] LockCommandDecision reject(
        LockCommandStatus status) noexcept;

    LockCommandGateConfig config_{};
    LockSequenceDetector detector_{};
    std::uint32_t lastNowMs_{0U};
    std::uint32_t lastSourceSequence_{0U};
    std::uint32_t lastEvidenceTimestampMs_{0U};
    bool clockInitialized_{false};
    bool evidenceInitialized_{false};
};

[[nodiscard]] const char* toString(LockCommandSource source) noexcept;
[[nodiscard]] const char* toString(LockCommandTrust trust) noexcept;
[[nodiscard]] const char* toString(LockCommandStatus status) noexcept;

}  // namespace bmw::remote::application
