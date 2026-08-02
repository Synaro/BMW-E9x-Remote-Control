#pragma once

#include <cstddef>
#include <cstdint>

#include "bmw_remote/infrastructure/can_frame.hpp"

namespace bmw::remote::infrastructure {

enum class ReplayStatus : std::uint8_t {
    Ready,
    Complete,
    InvalidTrace,
    ConsumerRejected,
    NonMonotonicTime,
};

struct ReplayBatch final {
    ReplayStatus status{ReplayStatus::Ready};
    std::size_t emittedFrames{0U};
    std::size_t nextFrameIndex{0U};
};

class CanTraceReplay final {
public:
    CanTraceReplay(const CanFrame* frames, std::size_t frameCount) noexcept;

    [[nodiscard]] ReplayBatch advanceTo(
        std::uint32_t elapsedMs,
        CanFrameConsumer& consumer) noexcept;

    void reset() noexcept;

    [[nodiscard]] constexpr bool isValid() const noexcept { return valid_; }
    [[nodiscard]] constexpr std::size_t frameCount() const noexcept { return frameCount_; }
    [[nodiscard]] constexpr std::size_t cursor() const noexcept { return cursor_; }

private:
    [[nodiscard]] bool validate() const noexcept;

    const CanFrame* frames_{nullptr};
    std::size_t frameCount_{0U};
    std::size_t cursor_{0U};
    bool valid_{false};
};

[[nodiscard]] const char* toString(ReplayStatus status) noexcept;

}  // namespace bmw::remote::infrastructure
