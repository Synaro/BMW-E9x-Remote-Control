#include "bmw_remote/infrastructure/can_lock_command_adapter.hpp"

#include <cstdint>

namespace bmw::remote::infrastructure {
namespace {

constexpr std::uint32_t HalfCounterRange = 0x80000000U;

[[nodiscard]] bool strictlyNewer(
    const std::uint32_t candidate,
    const std::uint32_t previous) noexcept {
    const std::uint32_t distance = candidate - previous;
    return distance != 0U && distance < HalfCounterRange;
}

}  // namespace

std::uint8_t CanCounterField::extract(const CanFrame& frame) const noexcept {
    if (byteIndex >= frame.dataLength ||
        byteIndex >= CanFrame::MaximumDataLength) {
        return 0U;
    }
    std::uint8_t packed = 0U;
    std::uint8_t outputBit = 0U;
    const std::uint8_t value = frame.data[byteIndex];
    for (std::uint8_t inputBit = 0U; inputBit < 8U; ++inputBit) {
        const std::uint8_t inputMask =
            static_cast<std::uint8_t>(1U << inputBit);
        if ((mask & inputMask) == 0U) {
            continue;
        }
        if ((value & inputMask) != 0U) {
            packed = static_cast<std::uint8_t>(
                packed | static_cast<std::uint8_t>(1U << outputBit));
        }
        ++outputBit;
    }
    return packed;
}

CanLockDecodeResult CanLockCommandAdapter::process(
    const CanFrame& frame) noexcept {
    if (!config_.enabled) {
        return {CanLockDecodeStatus::Disabled, {}};
    }
    if (!configIsValid()) {
        return reject(CanLockDecodeStatus::RejectedInvalidConfiguration);
    }
    if (!frame.isValid()) {
        return reject(CanLockDecodeStatus::RejectedInvalidFrame);
    }
    if (frame.identifier != config_.identifier ||
        frame.extended != config_.extended) {
        return {CanLockDecodeStatus::Ignored, {}};
    }
    if (frame.dataLength != config_.dataLength) {
        return reject(CanLockDecodeStatus::RejectedInvalidFrame);
    }
    if (initialized_ && frame.timestampMs != lastTimestampMs_ &&
        !strictlyNewer(frame.timestampMs, lastTimestampMs_)) {
        return reject(CanLockDecodeStatus::RejectedTimestampRegression);
    }

    const std::uint8_t rawCounter = config_.rollingCounter.extract(frame);
    const bool commandActive = config_.lockCommand.matches(frame);
    if (!initialized_) {
        lastTimestampMs_ = frame.timestampMs;
        lastRawCounter_ = rawCounter;
        lastCommandActive_ = commandActive;
        initialized_ = true;
        return {CanLockDecodeStatus::Primed, {}};
    }

    const std::uint16_t modulus = static_cast<std::uint16_t>(
        1U << config_.rollingCounter.bitCount());
    const std::uint16_t distance = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(rawCounter) + modulus -
         static_cast<std::uint16_t>(lastRawCounter_)) % modulus);
    if (distance == 0U) {
        return reject(CanLockDecodeStatus::RejectedDuplicateCounter);
    }
    if (distance >= modulus / 2U) {
        return reject(CanLockDecodeStatus::RejectedOutOfOrderCounter);
    }

    lastTimestampMs_ = frame.timestampMs;
    lastRawCounter_ = rawCounter;
    expandedSequence_ += distance;
    const bool risingEdge = commandActive && !lastCommandActive_;
    lastCommandActive_ = commandActive;
    if (!risingEdge) {
        return {CanLockDecodeStatus::NoCommand, {}};
    }

    return {
        CanLockDecodeStatus::EvidenceProduced,
        application::LockCommandEvidence{
            application::LockCommandSource::VehicleAdapter,
            config_.trust,
            expandedSequence_,
            frame.timestampMs,
            config_.vehicleSecured.matches(frame)}};
}

void CanLockCommandAdapter::reset() noexcept {
    lastTimestampMs_ = 0U;
    expandedSequence_ = 0U;
    lastRawCounter_ = 0U;
    lastCommandActive_ = false;
    initialized_ = false;
}

bool CanLockCommandAdapter::configIsValid() const noexcept {
    const std::uint32_t maximumIdentifier =
        config_.extended ? CanFrame::MaximumExtendedIdentifier
                         : CanFrame::MaximumStandardIdentifier;
    const bool commandCounterIndependent =
        config_.lockCommand.byteIndex != config_.rollingCounter.byteIndex ||
        (config_.lockCommand.mask & config_.rollingCounter.mask) == 0U;
    const bool securedCounterIndependent =
        config_.vehicleSecured.byteIndex !=
            config_.rollingCounter.byteIndex ||
        (config_.vehicleSecured.mask & config_.rollingCounter.mask) == 0U;
    const bool commandSecuredIndependent =
        config_.lockCommand.byteIndex != config_.vehicleSecured.byteIndex ||
        (config_.lockCommand.mask & config_.vehicleSecured.mask) == 0U;
    return config_.identifier <= maximumIdentifier &&
           config_.dataLength != 0U &&
           config_.dataLength <= CanFrame::MaximumDataLength &&
           config_.lockCommand.valid(config_.dataLength) &&
           config_.vehicleSecured.valid(config_.dataLength) &&
           config_.rollingCounter.valid(config_.dataLength) &&
           commandCounterIndependent && securedCounterIndependent &&
           commandSecuredIndependent;
}

CanLockDecodeResult CanLockCommandAdapter::reject(
    const CanLockDecodeStatus status) noexcept {
    reset();
    return {status, {}};
}

CanLockPipelineResult CanLockCommandPipeline::process(
    const CanFrame& frame,
    const std::uint32_t nowMs) noexcept {
    const CanLockDecodeResult decoded = adapter_.process(frame);
    if (decoded.requiresGestureReset()) {
        gate_.reset();
    }
    if (!decoded.hasEvidence()) {
        return {decoded.status, {}, false};
    }
    return {decoded.status, gate_.process(decoded.evidence, nowMs), true};
}

void CanLockCommandPipeline::reset() noexcept {
    adapter_.reset();
    gate_.reset();
}

const char* toString(const CanLockDecodeStatus status) noexcept {
    switch (status) {
        case CanLockDecodeStatus::Disabled: return "disabled";
        case CanLockDecodeStatus::Ignored: return "ignored";
        case CanLockDecodeStatus::Primed: return "primed";
        case CanLockDecodeStatus::NoCommand: return "no_command";
        case CanLockDecodeStatus::EvidenceProduced: return "evidence_produced";
        case CanLockDecodeStatus::RejectedInvalidConfiguration:
            return "rejected_invalid_configuration";
        case CanLockDecodeStatus::RejectedInvalidFrame:
            return "rejected_invalid_frame";
        case CanLockDecodeStatus::RejectedDuplicateCounter:
            return "rejected_duplicate_counter";
        case CanLockDecodeStatus::RejectedOutOfOrderCounter:
            return "rejected_out_of_order_counter";
        case CanLockDecodeStatus::RejectedTimestampRegression:
            return "rejected_timestamp_regression";
    }
    return "unknown";
}

}  // namespace bmw::remote::infrastructure
