#include "bmw_remote/application/telemetry_monitor.hpp"

namespace bmw::remote::application {
namespace {

[[nodiscard]] bool effective(const FeatureResolution resolution) noexcept {
    return resolution.effective();
}

}  // namespace

bool TelemetryReport::addAlert(const TelemetryAlertType type) noexcept {
    if (alertCount >= alerts.size()) {
        return false;
    }
    alerts[alertCount++] = TelemetryAlert{type};
    return true;
}

bool TelemetryReport::contains(const TelemetryAlertType type) const noexcept {
    for (std::size_t index = 0U; index < alertCount; ++index) {
        if (alerts[index].type == type) {
            return true;
        }
    }
    return false;
}

TelemetryMonitor::TelemetryMonitor(const TelemetryMonitorConfig config) noexcept
    : config_(config), configValid_(validTelemetryMonitorConfig(config)) {}

TelemetryReport TelemetryMonitor::evaluate(
    const domain::VehicleState& vehicle) noexcept {
    TelemetryReport report{};
    report.coldEngineFeature = resolveFeature(
        config_.requestedFeatures, FeatureId::ColdEngineGuard, config_.runtime);
    report.dpfFeature = resolveFeature(
        config_.requestedFeatures,
        FeatureId::DpfRegenerationIndicator,
        config_.runtime);
    report.transmissionFeature = resolveFeature(
        config_.requestedFeatures,
        FeatureId::TransmissionOverheatAlert,
        config_.runtime);

    report.coldEngineGuard =
        evaluateColdEngine(vehicle, report.coldEngineFeature);
    report.dpfRegeneration = evaluateDpf(vehicle, report.dpfFeature);
    report.transmissionOverheat =
        evaluateTransmission(vehicle, report.transmissionFeature);

    appendTransitionAlerts(
        previousColdEngine_,
        report.coldEngineGuard,
        TelemetryAlertType::ColdEngineHighRpm,
        TelemetryAlertType::ColdEngineRecovered,
        report);
    appendTransitionAlerts(
        previousDpf_,
        report.dpfRegeneration,
        TelemetryAlertType::DpfRegenerationStarted,
        TelemetryAlertType::DpfRegenerationStopped,
        report);
    appendTransitionAlerts(
        previousTransmission_,
        report.transmissionOverheat,
        TelemetryAlertType::TransmissionOverheat,
        TelemetryAlertType::TransmissionTemperatureRecovered,
        report);

    previousColdEngine_ = report.coldEngineGuard;
    previousDpf_ = report.dpfRegeneration;
    previousTransmission_ = report.transmissionOverheat;
    return report;
}

void TelemetryMonitor::reset() noexcept {
    previousColdEngine_ = TelemetryConditionState::Disabled;
    previousDpf_ = TelemetryConditionState::Disabled;
    previousTransmission_ = TelemetryConditionState::Disabled;
}

TelemetryConditionState TelemetryMonitor::evaluateColdEngine(
    const domain::VehicleState& vehicle,
    const FeatureResolution& resolution) const noexcept {
    if (!configValid_ || !effective(resolution)) {
        return resolution.status == FeatureResolutionStatus::DisabledByUser
                   ? TelemetryConditionState::Disabled
                   : TelemetryConditionState::Unavailable;
    }
    if (!vehicle.engineRpm.isFresh()) {
        return TelemetryConditionState::Unavailable;
    }

    bool temperatureAvailable = false;
    bool engineCold = false;
    if (vehicle.engineOilTemperatureC.isFresh()) {
        temperatureAvailable = true;
        engineCold =
            vehicle.engineOilTemperatureC.value < config_.engineWarmTemperatureC;
    }
    if (vehicle.coolantTemperatureC.isFresh()) {
        temperatureAvailable = true;
        engineCold = engineCold ||
                     vehicle.coolantTemperatureC.value <
                         config_.engineWarmTemperatureC;
    }
    if (!temperatureAvailable) {
        return TelemetryConditionState::Unavailable;
    }
    return engineCold && vehicle.engineRpm.value > config_.coldEngineMaximumRpm
               ? TelemetryConditionState::Active
               : TelemetryConditionState::Normal;
}

TelemetryConditionState TelemetryMonitor::evaluateDpf(
    const domain::VehicleState& vehicle,
    const FeatureResolution& resolution) const noexcept {
    if (!configValid_ || !effective(resolution)) {
        return resolution.status == FeatureResolutionStatus::DisabledByUser
                   ? TelemetryConditionState::Disabled
                   : TelemetryConditionState::Unavailable;
    }
    if (!vehicle.dpfRegenerationActive.isFresh()) {
        return TelemetryConditionState::Unavailable;
    }
    return vehicle.dpfRegenerationActive.value
               ? TelemetryConditionState::Active
               : TelemetryConditionState::Normal;
}

TelemetryConditionState TelemetryMonitor::evaluateTransmission(
    const domain::VehicleState& vehicle,
    const FeatureResolution& resolution) const noexcept {
    if (!configValid_ || !effective(resolution)) {
        return resolution.status == FeatureResolutionStatus::DisabledByUser
                   ? TelemetryConditionState::Disabled
                   : TelemetryConditionState::Unavailable;
    }
    if (!vehicle.transmissionOilTemperatureC.isFresh()) {
        return TelemetryConditionState::Unavailable;
    }

    const std::int16_t threshold =
        previousTransmission_ == TelemetryConditionState::Active
            ? static_cast<std::int16_t>(
                  config_.transmissionOverheatTemperatureC -
                  config_.temperatureHysteresisC)
            : config_.transmissionOverheatTemperatureC;
    return vehicle.transmissionOilTemperatureC.value >= threshold
               ? TelemetryConditionState::Active
               : TelemetryConditionState::Normal;
}

void TelemetryMonitor::appendTransitionAlerts(
    const TelemetryConditionState previous,
    const TelemetryConditionState current,
    const TelemetryAlertType activated,
    const TelemetryAlertType recovered,
    TelemetryReport& report) noexcept {
    if (current == TelemetryConditionState::Active &&
        previous != TelemetryConditionState::Active) {
        static_cast<void>(report.addAlert(activated));
    } else if (current == TelemetryConditionState::Normal &&
               previous == TelemetryConditionState::Active) {
        static_cast<void>(report.addAlert(recovered));
    }
}

const char* toString(const TelemetryConditionState state) noexcept {
    switch (state) {
        case TelemetryConditionState::Disabled: return "disabled";
        case TelemetryConditionState::Unavailable: return "unavailable";
        case TelemetryConditionState::Normal: return "normal";
        case TelemetryConditionState::Active: return "active";
    }
    return "unknown";
}

const char* toString(const TelemetryAlertType type) noexcept {
    switch (type) {
        case TelemetryAlertType::ColdEngineHighRpm:
            return "cold_engine_high_rpm";
        case TelemetryAlertType::ColdEngineRecovered:
            return "cold_engine_recovered";
        case TelemetryAlertType::DpfRegenerationStarted:
            return "dpf_regeneration_started";
        case TelemetryAlertType::DpfRegenerationStopped:
            return "dpf_regeneration_stopped";
        case TelemetryAlertType::TransmissionOverheat:
            return "transmission_overheat";
        case TelemetryAlertType::TransmissionTemperatureRecovered:
            return "transmission_temperature_recovered";
    }
    return "unknown";
}

}  // namespace bmw::remote::application
