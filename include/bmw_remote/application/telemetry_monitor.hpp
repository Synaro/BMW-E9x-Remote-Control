#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "bmw_remote/application/feature_catalog.hpp"
#include "bmw_remote/domain/vehicle_state.hpp"

namespace bmw::remote::application {

enum class TelemetryConditionState : std::uint8_t {
    Disabled,
    Unavailable,
    Normal,
    Active,
};

enum class TelemetryAlertType : std::uint8_t {
    ColdEngineHighRpm,
    ColdEngineRecovered,
    DpfRegenerationStarted,
    DpfRegenerationStopped,
    TransmissionOverheat,
    TransmissionTemperatureRecovered,
};

struct TelemetryAlert final {
    TelemetryAlertType type{TelemetryAlertType::ColdEngineHighRpm};
};

struct TelemetryMonitorConfig final {
    FeatureRequests requestedFeatures{};
    FeatureRuntimeContext runtime{};
    std::uint16_t coldEngineMaximumRpm{2'200U};
    std::int16_t engineWarmTemperatureC{75};
    std::int16_t transmissionOverheatTemperatureC{110};
    std::uint8_t temperatureHysteresisC{5U};
};

struct TelemetryMonitorLimits final {
    static constexpr std::uint16_t MinimumColdEngineMaximumRpm{1'000U};
    static constexpr std::uint16_t MaximumColdEngineMaximumRpm{4'500U};
    static constexpr std::int16_t MinimumEngineWarmTemperatureC{40};
    static constexpr std::int16_t MaximumEngineWarmTemperatureC{110};
    static constexpr std::int16_t MinimumTransmissionOverheatTemperatureC{80};
    static constexpr std::int16_t MaximumTransmissionOverheatTemperatureC{150};
    static constexpr std::uint8_t MinimumTemperatureHysteresisC{1U};
    static constexpr std::uint8_t MaximumTemperatureHysteresisC{20U};
};

[[nodiscard]] constexpr bool validTelemetryMonitorConfig(
    const TelemetryMonitorConfig& config) noexcept {
    return config.requestedFeatures.valid() &&
           config.coldEngineMaximumRpm >=
               TelemetryMonitorLimits::MinimumColdEngineMaximumRpm &&
           config.coldEngineMaximumRpm <=
               TelemetryMonitorLimits::MaximumColdEngineMaximumRpm &&
           config.engineWarmTemperatureC >=
               TelemetryMonitorLimits::MinimumEngineWarmTemperatureC &&
           config.engineWarmTemperatureC <=
               TelemetryMonitorLimits::MaximumEngineWarmTemperatureC &&
           config.transmissionOverheatTemperatureC >=
               TelemetryMonitorLimits::MinimumTransmissionOverheatTemperatureC &&
           config.transmissionOverheatTemperatureC <=
               TelemetryMonitorLimits::MaximumTransmissionOverheatTemperatureC &&
           config.temperatureHysteresisC >=
               TelemetryMonitorLimits::MinimumTemperatureHysteresisC &&
           config.temperatureHysteresisC <=
               TelemetryMonitorLimits::MaximumTemperatureHysteresisC;
}

struct TelemetryReport final {
    static constexpr std::size_t MaximumAlerts = 6U;

    TelemetryConditionState coldEngineGuard{TelemetryConditionState::Disabled};
    TelemetryConditionState dpfRegeneration{TelemetryConditionState::Disabled};
    TelemetryConditionState transmissionOverheat{
        TelemetryConditionState::Disabled};
    FeatureResolution coldEngineFeature{};
    FeatureResolution dpfFeature{};
    FeatureResolution transmissionFeature{};
    std::array<TelemetryAlert, MaximumAlerts> alerts{};
    std::size_t alertCount{0U};

    [[nodiscard]] bool addAlert(TelemetryAlertType type) noexcept;
    [[nodiscard]] bool contains(TelemetryAlertType type) const noexcept;
};

class TelemetryMonitor final {
public:
    explicit TelemetryMonitor(TelemetryMonitorConfig config = {}) noexcept;

    [[nodiscard]] TelemetryReport evaluate(
        const domain::VehicleState& vehicle) noexcept;
    void reset() noexcept;

private:
    [[nodiscard]] TelemetryConditionState evaluateColdEngine(
        const domain::VehicleState& vehicle,
        const FeatureResolution& resolution) const noexcept;
    [[nodiscard]] TelemetryConditionState evaluateDpf(
        const domain::VehicleState& vehicle,
        const FeatureResolution& resolution) const noexcept;
    [[nodiscard]] TelemetryConditionState evaluateTransmission(
        const domain::VehicleState& vehicle,
        const FeatureResolution& resolution) const noexcept;
    static void appendTransitionAlerts(
        TelemetryConditionState previous,
        TelemetryConditionState current,
        TelemetryAlertType activated,
        TelemetryAlertType recovered,
        TelemetryReport& report) noexcept;

    TelemetryMonitorConfig config_{};
    bool configValid_{true};
    TelemetryConditionState previousColdEngine_{TelemetryConditionState::Disabled};
    TelemetryConditionState previousDpf_{TelemetryConditionState::Disabled};
    TelemetryConditionState previousTransmission_{TelemetryConditionState::Disabled};
};

[[nodiscard]] const char* toString(TelemetryConditionState state) noexcept;
[[nodiscard]] const char* toString(TelemetryAlertType type) noexcept;

}  // namespace bmw::remote::application
