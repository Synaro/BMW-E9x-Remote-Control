#include "bmw_remote/infrastructure/replay_vehicle_gateway.hpp"

namespace bmw::remote::infrastructure {

ReplayVehicleGateway::ReplayVehicleGateway(
    const CanFrame* const frames,
    const std::size_t frameCount,
    const CanFrameDecoder& decoder,
    const SignalFreshnessConfig freshness) noexcept
    : replay_(frames, frameCount), assembler_(decoder, freshness) {
    lastBatch_.status = replay_.isValid() ? ReplayStatus::Ready : ReplayStatus::InvalidTrace;
}

bool ReplayVehicleGateway::requestState() noexcept {
    if (!timeIsValid_) {
        lastBatch_.status = ReplayStatus::NonMonotonicTime;
        lastBatch_.emittedFrames = 0U;
        return false;
    }

    lastBatch_ = replay_.advanceTo(elapsedMs_, assembler_);
    return lastBatch_.status == ReplayStatus::Ready ||
           lastBatch_.status == ReplayStatus::Complete;
}

bool ReplayVehicleGateway::setElapsedTime(const std::uint32_t elapsedMs) noexcept {
    if (elapsedMs < elapsedMs_) {
        timeIsValid_ = false;
        lastBatch_.status = ReplayStatus::NonMonotonicTime;
        lastBatch_.emittedFrames = 0U;
        return false;
    }

    elapsedMs_ = elapsedMs;
    return true;
}

void ReplayVehicleGateway::reset() noexcept {
    replay_.reset();
    assembler_.reset();
    elapsedMs_ = 0U;
    timeIsValid_ = true;
    lastBatch_ = {};
    lastBatch_.status = replay_.isValid() ? ReplayStatus::Ready : ReplayStatus::InvalidTrace;
}

domain::VehicleState ReplayVehicleGateway::state() const noexcept {
    return assembler_.snapshot(elapsedMs_);
}

}  // namespace bmw::remote::infrastructure
