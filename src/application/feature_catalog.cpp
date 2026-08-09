#include "bmw_remote/application/feature_catalog.hpp"

namespace bmw::remote::application {
namespace {

constexpr std::uint32_t capability(const FeatureCapability value) noexcept {
    return featureCapabilityMask(value);
}

constexpr std::array<FeatureDescriptor, FeatureCount> Catalog = {{
    {FeatureId::ColdEngineGuard, "cold_engine_guard", "Protection moteur froid", FeatureCategory::TelemetryCockpit, FeatureControlClass::Informational, FeatureReleaseTier::V1ReadOnly, capability(FeatureCapability::VehicleStateRead), 0U, true},
    {FeatureId::DpfRegenerationIndicator, "dpf_regeneration_indicator", "Indicateur regeneration FAP", FeatureCategory::TelemetryCockpit, FeatureControlClass::Informational, FeatureReleaseTier::V1ReadOnly, capability(FeatureCapability::VehicleStateRead), 0U, true},
    {FeatureId::TransmissionOverheatAlert, "transmission_overheat_alert", "Alerte temperature BVA", FeatureCategory::TelemetryCockpit, FeatureControlClass::Informational, FeatureReleaseTier::V1ReadOnly, capability(FeatureCapability::VehicleStateRead), 0U, true},
}};

[[nodiscard]] constexpr std::uint64_t featureBit(const FeatureId id) noexcept {
    return std::uint64_t{1U} << static_cast<std::size_t>(id);
}

}  // namespace

const std::array<FeatureDescriptor, FeatureCount>& featureCatalog() noexcept {
    return Catalog;
}

const FeatureDescriptor* findFeature(const FeatureId id) noexcept {
    const std::size_t index = static_cast<std::size_t>(id);
    return index < Catalog.size() ? &Catalog[index] : nullptr;
}

const FeatureDescriptor* findFeature(const char* const code) noexcept {
    return code == nullptr ? nullptr : findFeature(std::string_view{code});
}

const FeatureDescriptor* findFeature(const std::string_view code) noexcept {
    for (const FeatureDescriptor& descriptor : Catalog) {
        if (code == descriptor.code) {
            return &descriptor;
        }
    }
    return nullptr;
}

FeatureResolution resolveFeature(
    const FeatureRequests& requests,
    const FeatureId id,
    const FeatureRuntimeContext context) noexcept {
    const FeatureDescriptor* const descriptor = findFeature(id);
    if (descriptor == nullptr || !requests.enabled(id)) {
        return {FeatureResolutionStatus::DisabledByUser, 0U};
    }
    if ((context.implementedFeatures & featureBit(id)) == 0U) {
        return {FeatureResolutionStatus::NotImplemented, 0U};
    }
    if (descriptor->releaseTier == FeatureReleaseTier::BenchOnly &&
        context.target == FeatureExecutionTarget::Vehicle) {
        return {FeatureResolutionStatus::CriticalControlBlocked, 0U};
    }

    std::uint32_t missing =
        descriptor->requiredCapabilities & ~context.availableCapabilities;
    if (descriptor->anyCapability != 0U &&
        (descriptor->anyCapability & context.availableCapabilities) == 0U) {
        missing |= descriptor->anyCapability;
    }
    if (missing != 0U) {
        return {FeatureResolutionStatus::MissingCapabilities, missing};
    }
    if (descriptor->requiresQualifiedVehicleSignals &&
        !context.vehicleSignalsQualified) {
        return {FeatureResolutionStatus::SignalsUnqualified, 0U};
    }
    if (descriptor->controlClass == FeatureControlClass::ComfortVehicleWrite &&
        !context.comfortWritesQualified) {
        return {FeatureResolutionStatus::ComfortWritesUnqualified, 0U};
    }
    if (descriptor->controlClass ==
            FeatureControlClass::SafetyCriticalVehicleControl &&
        !context.criticalControlsQualified) {
        return {FeatureResolutionStatus::CriticalControlBlocked, 0U};
    }
    return {
        context.target == FeatureExecutionTarget::Simulation
            ? FeatureResolutionStatus::Simulated
            : FeatureResolutionStatus::Available,
        0U};
}

const char* toString(const FeatureCategory category) noexcept {
    switch (category) {
        case FeatureCategory::SecurityAccess: return "security_access";
        case FeatureCategory::TelemetryCockpit: return "telemetry_cockpit";
        case FeatureCategory::Lighting: return "lighting";
        case FeatureCategory::ComfortAutomation: return "comfort_automation";
        case FeatureCategory::SoftwareRule: return "software_rule";
    }
    return "unknown";
}

const char* toString(const FeatureControlClass controlClass) noexcept {
    switch (controlClass) {
        case FeatureControlClass::Informational: return "informational";
        case FeatureControlClass::ExternalOutput: return "external_output";
        case FeatureControlClass::ComfortVehicleWrite: return "comfort_vehicle_write";
        case FeatureControlClass::SafetyCriticalVehicleControl:
            return "safety_critical_vehicle_control";
    }
    return "unknown";
}

const char* toString(const FeatureReleaseTier tier) noexcept {
    switch (tier) {
        case FeatureReleaseTier::V1ReadOnly: return "v1_read_only";
        case FeatureReleaseTier::FutureComfort: return "future_comfort";
        case FeatureReleaseTier::BenchOnly: return "bench_only";
    }
    return "unknown";
}

const char* toString(const FeatureResolutionStatus status) noexcept {
    switch (status) {
        case FeatureResolutionStatus::DisabledByUser: return "disabled_by_user";
        case FeatureResolutionStatus::NotImplemented: return "not_implemented";
        case FeatureResolutionStatus::MissingCapabilities: return "missing_capabilities";
        case FeatureResolutionStatus::SignalsUnqualified: return "signals_unqualified";
        case FeatureResolutionStatus::ComfortWritesUnqualified:
            return "comfort_writes_unqualified";
        case FeatureResolutionStatus::CriticalControlBlocked:
            return "critical_control_blocked";
        case FeatureResolutionStatus::Simulated: return "simulated";
        case FeatureResolutionStatus::Available: return "available";
    }
    return "unknown";
}

}  // namespace bmw::remote::application
