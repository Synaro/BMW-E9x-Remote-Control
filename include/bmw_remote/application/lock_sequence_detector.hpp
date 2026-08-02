#pragma once

#include <cstdint>

namespace bmw::remote::application {

struct LockSequenceConfig final {
    std::uint8_t requiredPresses{3U};
    std::uint32_t minimumGapMs{80U};
    std::uint32_t maximumGapMs{1'500U};
    std::uint32_t maximumSequenceMs{3'000U};
};

enum class LockPressResult : std::uint8_t {
    Accepted,
    SequenceCompleted,
    SequenceRestarted,
    IgnoredDebounce,
    InvalidConfiguration,
};

class LockSequenceDetector final {
public:
    explicit constexpr LockSequenceDetector(
        const LockSequenceConfig config = {}) noexcept
        : config_(config) {}

    [[nodiscard]] LockPressResult observe(
        std::uint32_t timestampMs) noexcept;

    [[nodiscard]] bool observeLockPress(std::uint32_t timestampMs) noexcept {
        return observe(timestampMs) == LockPressResult::SequenceCompleted;
    }
    void reset() noexcept;

    [[nodiscard]] constexpr std::uint8_t pressCount() const noexcept {
        return pressCount_;
    }

private:
    [[nodiscard]] constexpr bool configIsValid() const noexcept {
        const std::uint32_t requiredIntervals =
            config_.requiredPresses == 0U
                ? 0U
                : static_cast<std::uint32_t>(config_.requiredPresses - 1U);
        const std::uint64_t minimumSequenceTime =
            static_cast<std::uint64_t>(requiredIntervals) *
            config_.minimumGapMs;
        return config_.requiredPresses != 0U &&
               config_.minimumGapMs <= config_.maximumGapMs &&
               config_.maximumGapMs <= config_.maximumSequenceMs &&
               minimumSequenceTime <= config_.maximumSequenceMs;
    }

    LockSequenceConfig config_{};
    std::uint32_t firstPressTimestampMs_{0U};
    std::uint32_t lastPressTimestampMs_{0U};
    std::uint8_t pressCount_{0U};
};

[[nodiscard]] const char* toString(LockPressResult result) noexcept;

}  // namespace bmw::remote::application
