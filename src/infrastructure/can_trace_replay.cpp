#include "bmw_remote/infrastructure/can_trace_replay.hpp"

namespace bmw::remote::infrastructure {

CanTraceReplay::CanTraceReplay(
    const CanFrame* const frames,
    const std::size_t frameCount) noexcept
    : frames_(frames), frameCount_(frameCount), valid_(validate()) {}

ReplayBatch CanTraceReplay::advanceTo(
    const std::uint32_t elapsedMs,
    CanFrameConsumer& consumer) noexcept {
    ReplayBatch batch{};
    batch.nextFrameIndex = cursor_;

    if (!valid_) {
        batch.status = ReplayStatus::InvalidTrace;
        return batch;
    }

    while (cursor_ < frameCount_ && frames_[cursor_].timestampMs <= elapsedMs) {
        if (!consumer.consume(frames_[cursor_])) {
            batch.status = ReplayStatus::ConsumerRejected;
            batch.nextFrameIndex = cursor_;
            return batch;
        }

        ++cursor_;
        ++batch.emittedFrames;
    }

    batch.nextFrameIndex = cursor_;
    batch.status = cursor_ == frameCount_ ? ReplayStatus::Complete : ReplayStatus::Ready;
    return batch;
}

void CanTraceReplay::reset() noexcept {
    cursor_ = 0U;
}

bool CanTraceReplay::validate() const noexcept {
    if (frames_ == nullptr) {
        return frameCount_ == 0U;
    }

    for (std::size_t index = 0U; index < frameCount_; ++index) {
        if (!frames_[index].isValid()) {
            return false;
        }

        if (index > 0U && frames_[index].timestampMs < frames_[index - 1U].timestampMs) {
            return false;
        }
    }

    return true;
}

const char* toString(const ReplayStatus status) noexcept {
    switch (status) {
        case ReplayStatus::Ready: return "ready";
        case ReplayStatus::Complete: return "complete";
        case ReplayStatus::InvalidTrace: return "invalid_trace";
        case ReplayStatus::ConsumerRejected: return "consumer_rejected";
        case ReplayStatus::NonMonotonicTime: return "non_monotonic_time";
    }
    return "unknown";
}

}  // namespace bmw::remote::infrastructure
