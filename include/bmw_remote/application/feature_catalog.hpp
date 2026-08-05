#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bmw::remote::application {

enum class FeatureId : std::uint8_t {
    PassiveBleAccess,
    SequentialKillSwitch,
    AlarmPushNotification,
    ValetMode,
    AntiCarjacking,
    SlamAndGoLock,
    HandsFreeTrunk,
    PhoneLeftBehindAlert,

    NeedleSweep,
    ExternalShiftLight,
    AndroidRacingDashboard,
    SteeringWheelMMode,
    ColdEngineGuard,
    OilTemperatureGauge,
    OneTouchDtc,
    ForcedDpfRegeneration,
    DpfRegenerationIndicator,
    ActiveTransmissionCooling,
    TransmissionOverheatAlert,
    FlightRecorder,
    VirtualObdBle,
    SmartTurboTimer,

    CustomWelcomeLighting,
    RapidHeadlightFlash,
    FourCornerStrobe,
    DynamicCorneringLights,
    DirectionalFollowMeHome,
    DynamicAmbientLighting,

    CustomStartupSequence,
    AutomaticDefrost,
    DynamicReverseTilt,
    RainWindowClosure,
    HighSpeedWindowClosure,
    ReverseAudioDucking,
    DriveThroughAssistant,
    SteeringWheelRemap,
    AutomaticHotspot,
    SmartphoneVoiceAssistant,
    MultiDriverProfiles,

    SmokerWindowOverride,
    RainSmokerPriority,
    PassengerReverseTiltOverride,
    ManualOverridePriority,

    Count,
};

inline constexpr std::size_t FeatureCount =
    static_cast<std::size_t>(FeatureId::Count);
static_assert(FeatureCount <= 64U, "Feature requests must fit in the fixed mask");

enum class FeatureCategory : std::uint8_t {
    SecurityAccess,
    TelemetryCockpit,
    Lighting,
    ComfortAutomation,
    SoftwareRule,
};

enum class FeatureControlClass : std::uint8_t {
    Informational,
    ExternalOutput,
    ComfortVehicleWrite,
    SafetyCriticalVehicleControl,
};

enum class FeatureReleaseTier : std::uint8_t {
    V1ReadOnly,
    FutureComfort,
    BenchOnly,
};

enum class FeatureCapability : std::uint32_t {
    None = 0U,
    VehicleStateRead = 1U << 0U,
    BodyBusWrite = 1U << 1U,
    PowertrainBusWrite = 1U << 2U,
    SteeringWheelInput = 1U << 3U,
    BleRadio = 1U << 4U,
    WifiNetwork = 1U << 5U,
    CellularNetwork = 1U << 6U,
    SdStorage = 1U << 7U,
    ExternalRgbOutput = 1U << 8U,
    AndroidCompanion = 1U << 9U,
    IosCompanion = 1U << 10U,
    LinBus = 1U << 11U,
    AlarmStatus = 1U << 12U,
    RainSensor = 1U << 13U,
    PassengerOccupancy = 1U << 14U,
    AuthenticatedPhone = 1U << 15U,
};

[[nodiscard]] constexpr std::uint32_t featureCapabilityMask(
    const FeatureCapability capability) noexcept {
    return static_cast<std::uint32_t>(capability);
}

struct FeatureDescriptor final {
    FeatureId id{FeatureId::PassiveBleAccess};
    const char* code{"passive_ble_access"};
    const char* displayName{"Passive BLE access"};
    FeatureCategory category{FeatureCategory::SecurityAccess};
    FeatureControlClass controlClass{FeatureControlClass::Informational};
    FeatureReleaseTier releaseTier{FeatureReleaseTier::FutureComfort};
    std::uint32_t requiredCapabilities{0U};
    std::uint32_t anyCapability{0U};
    bool requiresQualifiedVehicleSignals{false};
};

class FeatureRequests final {
public:
    constexpr FeatureRequests() noexcept = default;
    explicit constexpr FeatureRequests(const std::uint64_t mask) noexcept
        : mask_(mask) {}

    [[nodiscard]] constexpr bool enabled(const FeatureId id) const noexcept {
        const std::size_t index = static_cast<std::size_t>(id);
        return index < FeatureCount &&
               (mask_ & (std::uint64_t{1U} << index)) != 0U;
    }

    constexpr bool setEnabled(
        const FeatureId id,
        const bool enabledValue) noexcept {
        const std::size_t index = static_cast<std::size_t>(id);
        if (index >= FeatureCount) {
            return false;
        }
        const std::uint64_t bit = std::uint64_t{1U} << index;
        mask_ = enabledValue ? (mask_ | bit) : (mask_ & ~bit);
        return true;
    }

    [[nodiscard]] constexpr std::uint64_t mask() const noexcept {
        return mask_;
    }

    [[nodiscard]] constexpr bool valid() const noexcept {
        constexpr std::uint64_t ValidMask = []() constexpr {
            if constexpr (FeatureCount == 64U) {
                return ~std::uint64_t{0U};
            }
            return (std::uint64_t{1U} << FeatureCount) - 1U;
        }();
        return (mask_ & ~ValidMask) == 0U;
    }

private:
    std::uint64_t mask_{0U};
};

enum class FeatureExecutionTarget : std::uint8_t {
    Simulation,
    Vehicle,
};

enum class FeatureResolutionStatus : std::uint8_t {
    DisabledByUser,
    NotImplemented,
    MissingCapabilities,
    SignalsUnqualified,
    ComfortWritesUnqualified,
    CriticalControlBlocked,
    Simulated,
    Available,
};

struct FeatureRuntimeContext final {
    FeatureExecutionTarget target{FeatureExecutionTarget::Vehicle};
    std::uint64_t implementedFeatures{0U};
    std::uint32_t availableCapabilities{0U};
    bool vehicleSignalsQualified{false};
    bool comfortWritesQualified{false};
    bool criticalControlsQualified{false};
};

struct FeatureResolution final {
    FeatureResolutionStatus status{FeatureResolutionStatus::DisabledByUser};
    std::uint32_t missingCapabilities{0U};

    [[nodiscard]] constexpr bool effective() const noexcept {
        return status == FeatureResolutionStatus::Simulated ||
               status == FeatureResolutionStatus::Available;
    }
};

[[nodiscard]] const std::array<FeatureDescriptor, FeatureCount>&
featureCatalog() noexcept;

[[nodiscard]] const FeatureDescriptor* findFeature(FeatureId id) noexcept;
[[nodiscard]] const FeatureDescriptor* findFeature(const char* code) noexcept;
[[nodiscard]] const FeatureDescriptor* findFeature(std::string_view code) noexcept;

[[nodiscard]] FeatureResolution resolveFeature(
    const FeatureRequests& requests,
    FeatureId id,
    FeatureRuntimeContext context) noexcept;

[[nodiscard]] const char* toString(FeatureCategory category) noexcept;
[[nodiscard]] const char* toString(FeatureControlClass controlClass) noexcept;
[[nodiscard]] const char* toString(FeatureReleaseTier tier) noexcept;
[[nodiscard]] const char* toString(FeatureResolutionStatus status) noexcept;

}  // namespace bmw::remote::application
