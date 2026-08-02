#pragma once

#include <cstdint>

namespace bmw::remote::application {

struct LockSequenceConfig final {
    std::uint8_t requiredPresses{3U};
    std::uint32_t minimumGapMs{80U};
    std::uint32_t maximumGapMs{1'500U};
    std::uint32_t maximumSequenceMs{3'000U};
};

class LockSequenceDetector final {
public:
    explicit constexpr LockSequenceDetector(
        const LockSequenceConfig config = {}) noexcept
        : config_(config) {}

    [[nodiscard]] bool observeLockPress(std::uint32_t timestampMs) noexcept;
    void reset() noexcept;

    [[nodiscard]] constexpr std::uint8_t pressCount() const noexcept {
        return pressCount_;
    }

private:
    [[nodiscard]] constexpr bool configIsValid() const noexcept {
        return config_.requiredPresses != 0U &&
               config_.minimumGapMs <= config_.maximumGapMs &&
               config_.maximumGapMs <= config_.maximumSequenceMs;
    }

    LockSequenceConfig config_{};
    std::uint32_t firstPressTimestampMs_{0U};
    std::uint32_t lastPressTimestampMs_{0U};
    std::uint8_t pressCount_{0U};
};

}  // namespace bmw::remote::application
