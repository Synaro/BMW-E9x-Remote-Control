#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "bmw_remote/application/safety_policy.hpp"
#include "bmw_remote/domain/vehicle_state.hpp"

namespace bmw::remote::application {

enum class ControllerState : std::uint8_t {
    Idle,
    Authorizing,
    Preparing,
    Cranking,
    Running,
    Stopping,
    Fault,
};

enum class FaultCode : std::uint8_t {
    None,
    SafetyInterlock,
    VehicleCommunication,
    AuthorizationTimeout,
    CrankTimeout,
    StopTimeout,
    ActuatorFailure,
    TimerFailure,
    InternalError,
};

enum class EventType : std::uint8_t {
    RemoteStartRequested,
    RemoteStopRequested,
    VehicleStateUpdated,
    TimerElapsed,
    InfrastructureFailure,
    ResetRequested,
};

struct Event final {
    EventType type{EventType::VehicleStateUpdated};
    FaultCode fault{FaultCode::None};

    [[nodiscard]] static constexpr Event infrastructureFailure(
        const FaultCode code) noexcept {
        return Event{EventType::InfrastructureFailure, code};
    }
};

enum class ActionType : std::uint8_t {
    RequestVehicleState,
    EnableIgnition,
    EngageStarter,
    DisengageStarter,
    SecureOutputs,
    ArmTimer,
    CancelTimer,
    NotifyStartAccepted,
    NotifyStartRejected,
    NotifyRunning,
    NotifyStopping,
    NotifyStopped,
    NotifyFault,
    NotifyReady,
    NotifyRequestIgnored,
    NotifyResetRejected,
};

struct Action final {
    ActionType type{ActionType::SecureOutputs};
    std::uint32_t durationMs{0U};
};

struct Decision final {
    static constexpr std::size_t MaximumActions = 8U;

    ControllerState previousState{ControllerState::Idle};
    ControllerState state{ControllerState::Idle};
    FaultCode fault{FaultCode::None};
    SafetyAssessment safety{};
    std::array<Action, MaximumActions> actions{};
    std::size_t actionCount{0U};

    [[nodiscard]] bool contains(const ActionType type) const noexcept;

private:
    friend class Controller;
    void add(ActionType type, std::uint32_t durationMs = 0U) noexcept;
};

struct ControllerConfig final {
    SafetyPolicyConfig safety{};
    std::uint32_t authorizationTimeoutMs{2'000U};
    std::uint32_t preparationDelayMs{1'500U};
    std::uint32_t maximumCrankTimeMs{5'000U};
    std::uint32_t maximumRemoteRunTimeMs{15U * 60U * 1'000U};
    std::uint32_t stopConfirmationTimeoutMs{3'000U};
};

class Controller final {
public:
    explicit Controller(const ControllerConfig config = {}) noexcept;

    [[nodiscard]] Decision handle(
        Event event,
        const domain::VehicleState& vehicle) noexcept;

    [[nodiscard]] constexpr ControllerState state() const noexcept {
        return state_;
    }

    [[nodiscard]] constexpr FaultCode fault() const noexcept {
        return fault_;
    }

private:
    void handleStartRequest(Decision& decision) noexcept;
    void handleStopRequest(Decision& decision) noexcept;
    void handleVehicleUpdate(
        Decision& decision,
        const domain::VehicleState& vehicle) noexcept;
    void handleTimer(
        Decision& decision,
        const domain::VehicleState& vehicle) noexcept;
    void handleReset(
        Decision& decision,
        const domain::VehicleState& vehicle) noexcept;

    void beginPreparing(Decision& decision) noexcept;
    void beginCranking(Decision& decision) noexcept;
    void beginRunning(Decision& decision) noexcept;
    void beginStopping(Decision& decision) noexcept;
    void completeStop(Decision& decision) noexcept;
    void enterFault(Decision& decision, FaultCode fault) noexcept;
    void transition(Decision& decision, ControllerState next) noexcept;

    [[nodiscard]] bool engineIsRunning(
        const domain::VehicleState& vehicle) const noexcept;

    ControllerConfig config_{};
    SafetyPolicy safetyPolicy_{};
    ControllerState state_{ControllerState::Idle};
    FaultCode fault_{FaultCode::None};
};

[[nodiscard]] const char* toString(ControllerState state) noexcept;
[[nodiscard]] const char* toString(FaultCode fault) noexcept;
[[nodiscard]] const char* toString(ActionType action) noexcept;

}  // namespace bmw::remote::application
