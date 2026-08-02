#include "bmw_remote/application/lock_sequence_detector.hpp"

namespace bmw::remote::application {

LockPressResult LockSequenceDetector::observe(
    const std::uint32_t timestampMs) noexcept {
    if (!configIsValid()) {
        reset();
        return LockPressResult::InvalidConfiguration;
    }

    LockPressResult result = LockPressResult::Accepted;
    if (pressCount_ == 0U) {
        firstPressTimestampMs_ = timestampMs;
        lastPressTimestampMs_ = timestampMs;
        pressCount_ = 1U;
    } else {
        const std::uint32_t sinceLastPress = timestampMs - lastPressTimestampMs_;
        const std::uint32_t sinceFirstPress = timestampMs - firstPressTimestampMs_;

        if (sinceLastPress < config_.minimumGapMs) {
            return LockPressResult::IgnoredDebounce;
        }

        if (sinceLastPress > config_.maximumGapMs ||
            sinceFirstPress > config_.maximumSequenceMs) {
            firstPressTimestampMs_ = timestampMs;
            lastPressTimestampMs_ = timestampMs;
            pressCount_ = 1U;
            result = LockPressResult::SequenceRestarted;
        } else {
            lastPressTimestampMs_ = timestampMs;
            ++pressCount_;
        }
    }

    if (pressCount_ < config_.requiredPresses) {
        return result;
    }

    reset();
    return LockPressResult::SequenceCompleted;
}

void LockSequenceDetector::reset() noexcept {
    firstPressTimestampMs_ = 0U;
    lastPressTimestampMs_ = 0U;
    pressCount_ = 0U;
}

const char* toString(const LockPressResult result) noexcept {
    switch (result) {
        case LockPressResult::Accepted: return "accepted";
        case LockPressResult::SequenceCompleted: return "sequence_completed";
        case LockPressResult::SequenceRestarted: return "sequence_restarted";
        case LockPressResult::IgnoredDebounce: return "ignored_debounce";
        case LockPressResult::InvalidConfiguration:
            return "invalid_configuration";
    }
    return "unknown";
}

}  // namespace bmw::remote::application
