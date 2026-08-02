#include "bmw_remote/application/lock_sequence_detector.hpp"

namespace bmw::remote::application {

bool LockSequenceDetector::observeLockPress(
    const std::uint32_t timestampMs) noexcept {
    if (!configIsValid()) {
        reset();
        return false;
    }

    if (pressCount_ == 0U) {
        firstPressTimestampMs_ = timestampMs;
        lastPressTimestampMs_ = timestampMs;
        pressCount_ = 1U;
    } else {
        const std::uint32_t sinceLastPress = timestampMs - lastPressTimestampMs_;
        const std::uint32_t sinceFirstPress = timestampMs - firstPressTimestampMs_;

        if (sinceLastPress < config_.minimumGapMs) {
            return false;
        }

        if (sinceLastPress > config_.maximumGapMs ||
            sinceFirstPress > config_.maximumSequenceMs) {
            firstPressTimestampMs_ = timestampMs;
            lastPressTimestampMs_ = timestampMs;
            pressCount_ = 1U;
        } else {
            lastPressTimestampMs_ = timestampMs;
            ++pressCount_;
        }
    }

    if (pressCount_ < config_.requiredPresses) {
        return false;
    }

    reset();
    return true;
}

void LockSequenceDetector::reset() noexcept {
    firstPressTimestampMs_ = 0U;
    lastPressTimestampMs_ = 0U;
    pressCount_ = 0U;
}

}  // namespace bmw::remote::application
