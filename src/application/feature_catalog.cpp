#include "bmw_remote/application/feature_catalog.hpp"

namespace bmw::remote::application {
namespace {

constexpr std::uint32_t capability(const FeatureCapability value) noexcept {
    return featureCapabilityMask(value);
}

constexpr std::array<FeatureDescriptor, FeatureCount> Catalog = {{
    {FeatureId::PassiveBleAccess, "passive_ble_access", "Acces mains-libres BLE", FeatureCategory::SecurityAccess, FeatureControlClass::SafetyCriticalVehicleControl, FeatureReleaseTier::BenchOnly, capability(FeatureCapability::BleRadio) | capability(FeatureCapability::AuthenticatedPhone) | capability(FeatureCapability::BodyBusWrite), capability(FeatureCapability::AndroidCompanion) | capability(FeatureCapability::IosCompanion), false},
    {FeatureId::SequentialKillSwitch, "sequential_kill_switch", "Kill-switch sequentiel", FeatureCategory::SecurityAccess, FeatureControlClass::SafetyCriticalVehicleControl, FeatureReleaseTier::BenchOnly, capability(FeatureCapability::SteeringWheelInput) | capability(FeatureCapability::PowertrainBusWrite), 0U, true},
    {FeatureId::AlarmPushNotification, "alarm_push_notification", "Notification alarme", FeatureCategory::SecurityAccess, FeatureControlClass::Informational, FeatureReleaseTier::V1ReadOnly, capability(FeatureCapability::AlarmStatus), capability(FeatureCapability::WifiNetwork) | capability(FeatureCapability::CellularNetwork), true},
    {FeatureId::ValetMode, "valet_mode", "Mode valet", FeatureCategory::SecurityAccess, FeatureControlClass::SafetyCriticalVehicleControl, FeatureReleaseTier::BenchOnly, capability(FeatureCapability::PowertrainBusWrite) | capability(FeatureCapability::AuthenticatedPhone), 0U, true},
    {FeatureId::AntiCarjacking, "anti_carjacking", "Anti-carjacking", FeatureCategory::SecurityAccess, FeatureControlClass::SafetyCriticalVehicleControl, FeatureReleaseTier::BenchOnly, capability(FeatureCapability::PowertrainBusWrite) | capability(FeatureCapability::AuthenticatedPhone), 0U, true},
    {FeatureId::SlamAndGoLock, "slam_and_go_lock", "Verrouillage Slam & Go", FeatureCategory::SecurityAccess, FeatureControlClass::SafetyCriticalVehicleControl, FeatureReleaseTier::BenchOnly, capability(FeatureCapability::BleRadio) | capability(FeatureCapability::AuthenticatedPhone) | capability(FeatureCapability::BodyBusWrite), 0U, true},
    {FeatureId::HandsFreeTrunk, "hands_free_trunk", "Coffre mains-libres", FeatureCategory::SecurityAccess, FeatureControlClass::SafetyCriticalVehicleControl, FeatureReleaseTier::BenchOnly, capability(FeatureCapability::BleRadio) | capability(FeatureCapability::AuthenticatedPhone) | capability(FeatureCapability::BodyBusWrite), 0U, true},
    {FeatureId::PhoneLeftBehindAlert, "phone_left_behind_alert", "Alerte telephone oublie", FeatureCategory::SecurityAccess, FeatureControlClass::ExternalOutput, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::BleRadio) | capability(FeatureCapability::AuthenticatedPhone) | capability(FeatureCapability::VehicleStateRead), 0U, true},

    {FeatureId::NeedleSweep, "needle_sweep", "Balayage des aiguilles", FeatureCategory::TelemetryCockpit, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::BodyBusWrite), 0U, false},
    {FeatureId::ExternalShiftLight, "external_shift_light", "Shift-light externe", FeatureCategory::TelemetryCockpit, FeatureControlClass::ExternalOutput, FeatureReleaseTier::V1ReadOnly, capability(FeatureCapability::VehicleStateRead) | capability(FeatureCapability::ExternalRgbOutput), 0U, true},
    {FeatureId::AndroidRacingDashboard, "android_racing_dashboard", "Dashboard racing Android", FeatureCategory::TelemetryCockpit, FeatureControlClass::Informational, FeatureReleaseTier::V1ReadOnly, capability(FeatureCapability::VehicleStateRead) | capability(FeatureCapability::AndroidCompanion), 0U, true},
    {FeatureId::SteeringWheelMMode, "steering_wheel_m_mode", "M-Mode au volant", FeatureCategory::TelemetryCockpit, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::SteeringWheelInput) | capability(FeatureCapability::BodyBusWrite), 0U, true},
    {FeatureId::ColdEngineGuard, "cold_engine_guard", "Protection moteur froid", FeatureCategory::TelemetryCockpit, FeatureControlClass::Informational, FeatureReleaseTier::V1ReadOnly, capability(FeatureCapability::VehicleStateRead), 0U, true},
    {FeatureId::OilTemperatureGauge, "oil_temperature_gauge", "Jauge temperature huile", FeatureCategory::TelemetryCockpit, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::VehicleStateRead) | capability(FeatureCapability::BodyBusWrite), 0U, true},
    {FeatureId::OneTouchDtc, "one_touch_dtc", "DTC en une touche", FeatureCategory::TelemetryCockpit, FeatureControlClass::SafetyCriticalVehicleControl, FeatureReleaseTier::BenchOnly, capability(FeatureCapability::SteeringWheelInput) | capability(FeatureCapability::PowertrainBusWrite), 0U, true},
    {FeatureId::ForcedDpfRegeneration, "forced_dpf_regeneration", "Regeneration FAP forcee", FeatureCategory::TelemetryCockpit, FeatureControlClass::SafetyCriticalVehicleControl, FeatureReleaseTier::BenchOnly, capability(FeatureCapability::SteeringWheelInput) | capability(FeatureCapability::PowertrainBusWrite), 0U, true},
    {FeatureId::DpfRegenerationIndicator, "dpf_regeneration_indicator", "Indicateur regeneration FAP", FeatureCategory::TelemetryCockpit, FeatureControlClass::Informational, FeatureReleaseTier::V1ReadOnly, capability(FeatureCapability::VehicleStateRead), 0U, true},
    {FeatureId::ActiveTransmissionCooling, "active_transmission_cooling", "Refroidissement actif BVA", FeatureCategory::TelemetryCockpit, FeatureControlClass::SafetyCriticalVehicleControl, FeatureReleaseTier::BenchOnly, capability(FeatureCapability::VehicleStateRead) | capability(FeatureCapability::PowertrainBusWrite), 0U, true},
    {FeatureId::TransmissionOverheatAlert, "transmission_overheat_alert", "Alerte temperature BVA", FeatureCategory::TelemetryCockpit, FeatureControlClass::Informational, FeatureReleaseTier::V1ReadOnly, capability(FeatureCapability::VehicleStateRead), 0U, true},
    {FeatureId::FlightRecorder, "flight_recorder", "Enregistreur de vol", FeatureCategory::TelemetryCockpit, FeatureControlClass::Informational, FeatureReleaseTier::V1ReadOnly, capability(FeatureCapability::VehicleStateRead) | capability(FeatureCapability::SdStorage), 0U, true},
    {FeatureId::VirtualObdBle, "virtual_obd_ble", "OBD2 BLE virtuel", FeatureCategory::TelemetryCockpit, FeatureControlClass::Informational, FeatureReleaseTier::V1ReadOnly, capability(FeatureCapability::VehicleStateRead) | capability(FeatureCapability::BleRadio), 0U, true},
    {FeatureId::SmartTurboTimer, "smart_turbo_timer", "Turbo timer intelligent", FeatureCategory::TelemetryCockpit, FeatureControlClass::SafetyCriticalVehicleControl, FeatureReleaseTier::BenchOnly, capability(FeatureCapability::VehicleStateRead) | capability(FeatureCapability::PowertrainBusWrite), 0U, true},

    {FeatureId::CustomWelcomeLighting, "custom_welcome_lighting", "Eclairage d'accueil personnalise", FeatureCategory::Lighting, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::BodyBusWrite), 0U, false},
    {FeatureId::RapidHeadlightFlash, "rapid_headlight_flash", "Appel de phares rapide", FeatureCategory::Lighting, FeatureControlClass::SafetyCriticalVehicleControl, FeatureReleaseTier::BenchOnly, capability(FeatureCapability::SteeringWheelInput) | capability(FeatureCapability::BodyBusWrite), 0U, true},
    {FeatureId::FourCornerStrobe, "four_corner_strobe", "Strobe quatre coins", FeatureCategory::Lighting, FeatureControlClass::SafetyCriticalVehicleControl, FeatureReleaseTier::BenchOnly, capability(FeatureCapability::SteeringWheelInput) | capability(FeatureCapability::BodyBusWrite), 0U, true},
    {FeatureId::DynamicCorneringLights, "dynamic_cornering_lights", "Eclairage dynamique de virage", FeatureCategory::Lighting, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::VehicleStateRead) | capability(FeatureCapability::BodyBusWrite), 0U, true},
    {FeatureId::DirectionalFollowMeHome, "directional_follow_me_home", "Follow-me-home directionnel", FeatureCategory::Lighting, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::VehicleStateRead) | capability(FeatureCapability::BodyBusWrite), 0U, true},
    {FeatureId::DynamicAmbientLighting, "dynamic_ambient_lighting", "Ambiance LED dynamique", FeatureCategory::Lighting, FeatureControlClass::ExternalOutput, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::VehicleStateRead) | capability(FeatureCapability::ExternalRgbOutput), 0U, true},

    {FeatureId::CustomStartupSequence, "custom_startup_sequence", "Sequence de demarrage personnalisee", FeatureCategory::ComfortAutomation, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::VehicleStateRead) | capability(FeatureCapability::BodyBusWrite), 0U, true},
    {FeatureId::AutomaticDefrost, "automatic_defrost", "Degivrage automatique", FeatureCategory::ComfortAutomation, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::VehicleStateRead) | capability(FeatureCapability::BodyBusWrite), 0U, true},
    {FeatureId::DynamicReverseTilt, "dynamic_reverse_tilt", "Inclinaison dynamique du retro", FeatureCategory::ComfortAutomation, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::VehicleStateRead) | capability(FeatureCapability::LinBus), 0U, true},
    {FeatureId::RainWindowClosure, "rain_window_closure", "Fermeture automatique pluie", FeatureCategory::ComfortAutomation, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::RainSensor) | capability(FeatureCapability::BodyBusWrite), 0U, true},
    {FeatureId::HighSpeedWindowClosure, "high_speed_window_closure", "Fermeture aero des vitres", FeatureCategory::ComfortAutomation, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::VehicleStateRead) | capability(FeatureCapability::BodyBusWrite), 0U, true},
    {FeatureId::ReverseAudioDucking, "reverse_audio_ducking", "Baisse audio en marche arriere", FeatureCategory::ComfortAutomation, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::VehicleStateRead) | capability(FeatureCapability::BodyBusWrite), 0U, true},
    {FeatureId::DriveThroughAssistant, "drive_through_assistant", "Assistant peage et drive", FeatureCategory::ComfortAutomation, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::SteeringWheelInput) | capability(FeatureCapability::BodyBusWrite), 0U, true},
    {FeatureId::SteeringWheelRemap, "steering_wheel_remap", "Remapping des boutons volant", FeatureCategory::ComfortAutomation, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::SteeringWheelInput), 0U, true},
    {FeatureId::AutomaticHotspot, "automatic_hotspot", "Hotspot automatique", FeatureCategory::ComfortAutomation, FeatureControlClass::ExternalOutput, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::BleRadio), capability(FeatureCapability::AndroidCompanion) | capability(FeatureCapability::IosCompanion), false},
    {FeatureId::SmartphoneVoiceAssistant, "smartphone_voice_assistant", "Assistant vocal smartphone", FeatureCategory::ComfortAutomation, FeatureControlClass::ExternalOutput, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::BleRadio) | capability(FeatureCapability::SteeringWheelInput), capability(FeatureCapability::AndroidCompanion) | capability(FeatureCapability::IosCompanion), false},
    {FeatureId::MultiDriverProfiles, "multi_driver_profiles", "Profils multi-conducteurs", FeatureCategory::ComfortAutomation, FeatureControlClass::ExternalOutput, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::BleRadio) | capability(FeatureCapability::AuthenticatedPhone), capability(FeatureCapability::AndroidCompanion) | capability(FeatureCapability::IosCompanion), false},

    {FeatureId::SmokerWindowOverride, "smoker_window_override", "Override vitres mode fumeur", FeatureCategory::SoftwareRule, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::BodyBusWrite), 0U, true},
    {FeatureId::RainSmokerPriority, "rain_smoker_priority", "Priorite pluie et mode fumeur", FeatureCategory::SoftwareRule, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::RainSensor) | capability(FeatureCapability::BodyBusWrite), 0U, true},
    {FeatureId::PassengerReverseTiltOverride, "passenger_reverse_tilt_override", "Override retro avec passager", FeatureCategory::SoftwareRule, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::PassengerOccupancy) | capability(FeatureCapability::LinBus), 0U, true},
    {FeatureId::ManualOverridePriority, "manual_override_priority", "Priorite aux commandes manuelles", FeatureCategory::SoftwareRule, FeatureControlClass::ComfortVehicleWrite, FeatureReleaseTier::FutureComfort, capability(FeatureCapability::VehicleStateRead), 0U, true},
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
