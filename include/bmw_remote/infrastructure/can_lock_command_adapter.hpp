#pragma once

#include <cstdint>

#include "bmw_remote/application/lock_command_gate.hpp"
#include "bmw_remote/infrastructure/can_frame.hpp"

namespace bmw::remote::infrastructure {

struct CanBitMatcher final {
    std::uint8_t byteIndex{0U};
    std::uint8_t mask{0U};
    std::uint8_t expectedValue{0U};

    [[nodiscard]] constexpr bool valid(
        const std::uint8_t dataLength) const noexcept {
        return byteIndex < dataLength && mask != 0U &&
               (expectedValue & static_cast<std::uint8_t>(~mask)) == 0U;
    }

    [[nodiscard]] constexpr bool matches(
        const CanFrame& frame) const noexcept {
        return byteIndex < frame.dataLength &&
               byteIndex < CanFrame::MaximumDataLength && mask != 0U &&
               (frame.data[byteIndex] & mask) == expectedValue;
    }
};

struct CanCounterField final {
    std::uint8_t byteIndex{0U};
    std::uint8_t mask{0U};

    [[nodiscard]] constexpr std::uint8_t bitCount() const noexcept {
        std::uint8_t count = 0U;
        for (std::uint8_t bits = mask; bits != 0U; bits >>= 1U) {
            count = static_cast<std::uint8_t>(count + (bits & 1U));
        }
        return count;
    }

    [[nodiscard]] constexpr bool valid(
        const std::uint8_t dataLength) const noexcept {
        return byteIndex < dataLength && bitCount() >= 2U;
    }

    [[nodiscard]] std::uint8_t extract(const CanFrame& frame) const noexcept;
};

struct CanLockCommandAdapterConfig final {
    bool enabled{false};
    application::LockCommandTrust trust{
        application::LockCommandTrust::Unverified};
    std::uint32_t identifier{0U};
    bool extended{false};
    std::uint8_t dataLength{0U};
    CanBitMatcher lockCommand{};
    CanBitMatcher vehicleSecured{};
    CanCounterField rollingCounter{};
};

enum class CanLockDecodeStatus : std::uint8_t {
    Disabled,
    Ignored,
    Primed,
    NoCommand,
    EvidenceProduced,
    RejectedInvalidConfiguration,
    RejectedInvalidFrame,
    RejectedDuplicateCounter,
    RejectedOutOfOrderCounter,
    RejectedTimestampRegression,
};

struct CanLockDecodeResult final {
    CanLockDecodeStatus status{CanLockDecodeStatus::Disabled};
    application::LockCommandEvidence evidence{};

    [[nodiscard]] constexpr bool hasEvidence() const noexcept {
        return status == CanLockDecodeStatus::EvidenceProduced;
    }

    [[nodiscard]] constexpr bool requiresGestureReset() const noexcept {
        return status == CanLockDecodeStatus::RejectedInvalidConfiguration ||
               status == CanLockDecodeStatus::RejectedInvalidFrame ||
               status == CanLockDecodeStatus::RejectedDuplicateCounter ||
               status == CanLockDecodeStatus::RejectedOutOfOrderCounter ||
               status == CanLockDecodeStatus::RejectedTimestampRegression;
    }
};

class CanLockCommandAdapter final {
public:
    explicit constexpr CanLockCommandAdapter(
        const CanLockCommandAdapterConfig config = {}) noexcept
        : config_(config) {}

    [[nodiscard]] CanLockDecodeResult process(
        const CanFrame& frame) noexcept;
    void reset() noexcept;

private:
    [[nodiscard]] bool configIsValid() const noexcept;
    [[nodiscard]] CanLockDecodeResult reject(
        CanLockDecodeStatus status) noexcept;

    CanLockCommandAdapterConfig config_{};
    std::uint32_t lastTimestampMs_{0U};
    std::uint32_t expandedSequence_{0U};
    std::uint8_t lastRawCounter_{0U};
    bool lastCommandActive_{false};
    bool initialized_{false};
};

struct CanLockPipelineResult final {
    CanLockDecodeStatus decodeStatus{CanLockDecodeStatus::Disabled};
    application::LockCommandDecision command{};
    bool commandEvaluated{false};
};

class CanLockCommandPipeline final {
public:
    explicit constexpr CanLockCommandPipeline(
        const CanLockCommandAdapterConfig adapterConfig = {},
        const application::LockCommandGateConfig gateConfig = {}) noexcept
        : adapter_(adapterConfig), gate_(gateConfig) {}

    [[nodiscard]] CanLockPipelineResult process(
        const CanFrame& frame,
        std::uint32_t nowMs) noexcept;
    void reset() noexcept;

    [[nodiscard]] constexpr std::uint8_t pressCount() const noexcept {
        return gate_.pressCount();
    }

private:
    CanLockCommandAdapter adapter_{};
    application::LockCommandGate gate_{};
};

[[nodiscard]] const char* toString(CanLockDecodeStatus status) noexcept;

}  // namespace bmw::remote::infrastructure
