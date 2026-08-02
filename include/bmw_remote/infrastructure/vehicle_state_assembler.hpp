#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "bmw_remote/domain/vehicle_signal.hpp"
#include "bmw_remote/domain/vehicle_state.hpp"
#include "bmw_remote/infrastructure/can_frame.hpp"

namespace bmw::remote::infrastructure {

struct DecodedSignal final {
    domain::VehicleSignal signal{domain::VehicleSignal::BatteryMillivolts};
    std::uint32_t value{0U};
};

struct DecodedSignalBatch final {
    static constexpr std::size_t MaximumSignals = 10U;

    std::array<DecodedSignal, MaximumSignals> signals{};
    std::size_t count{0U};

    [[nodiscard]] bool add(domain::VehicleSignal signal, std::uint32_t value) noexcept;
};

enum class DecodeResult : std::uint8_t {
    Ignored,
    Decoded,
    Invalid,
};

class CanFrameDecoder {
public:
    virtual ~CanFrameDecoder() = default;
    [[nodiscard]] virtual DecodeResult decode(
        const CanFrame& frame,
        DecodedSignalBatch& output) const noexcept = 0;
};

struct SignalFreshnessConfig final {
    std::uint32_t batteryMaximumAgeMs{2'000U};
    std::uint32_t engineRpmMaximumAgeMs{500U};
    std::uint32_t bodyMaximumAgeMs{2'000U};
    std::uint32_t transmissionMaximumAgeMs{1'000U};
    std::uint32_t faultMaximumAgeMs{2'000U};
};

struct AssemblyStatistics final {
    std::uint32_t consumedFrames{0U};
    std::uint32_t decodedFrames{0U};
    std::uint32_t ignoredFrames{0U};
    std::uint32_t rejectedFrames{0U};
    std::uint32_t decodedSignals{0U};
};

class VehicleStateAssembler final : public CanFrameConsumer {
public:
    explicit VehicleStateAssembler(
        const CanFrameDecoder& decoder,
        SignalFreshnessConfig freshness = {}) noexcept;

    bool consume(const CanFrame& frame) noexcept override;

    [[nodiscard]] domain::VehicleState snapshot(std::uint32_t nowMs) const noexcept;
    [[nodiscard]] constexpr AssemblyStatistics statistics() const noexcept {
        return statistics_;
    }

    void reset() noexcept;

private:
    template <typename T>
    struct StoredSignal final {
        T value{};
        std::uint32_t observedAtMs{0U};
        bool observed{false};
    };

    [[nodiscard]] bool validateBatch(const DecodedSignalBatch& batch) const noexcept;
    [[nodiscard]] bool validateSignal(const DecodedSignal& signal) const noexcept;
    void applyBatch(const DecodedSignalBatch& batch, std::uint32_t timestampMs) noexcept;
    void applySignal(const DecodedSignal& signal, std::uint32_t timestampMs) noexcept;

    template <typename T>
    [[nodiscard]] static domain::Observed<T> observe(
        const StoredSignal<T>& signal,
        std::uint32_t nowMs,
        std::uint32_t maximumAgeMs) noexcept {
        if (!signal.observed) {
            return {};
        }

        const std::uint32_t ageMs = nowMs - signal.observedAtMs;
        const domain::SignalQuality quality = ageMs <= maximumAgeMs
                                                  ? domain::SignalQuality::Fresh
                                                  : domain::SignalQuality::Stale;
        return domain::Observed<T>{signal.value, quality};
    }

    const CanFrameDecoder& decoder_;
    SignalFreshnessConfig freshness_{};
    AssemblyStatistics statistics_{};
    StoredSignal<std::uint16_t> batteryMillivolts_{};
    StoredSignal<std::uint16_t> engineRpm_{};
    StoredSignal<bool> hoodClosed_{};
    StoredSignal<bool> doorsClosed_{};
    StoredSignal<bool> trunkClosed_{};
    StoredSignal<bool> brakePressed_{};
    StoredSignal<bool> parkingBrakeApplied_{};
    StoredSignal<domain::Transmission> transmission_{};
    StoredSignal<domain::Gear> gear_{};
    StoredSignal<bool> criticalFaultPresent_{};
};

}  // namespace bmw::remote::infrastructure
