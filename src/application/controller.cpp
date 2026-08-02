#include "bmw_remote/application/controller.hpp"

namespace bmw::remote::application {

bool Decision::contains(const ActionType type) const noexcept {
    for (std::size_t index = 0U; index < actionCount; ++index) {
        if (actions[index].type == type) {
            return true;
        }
    }
    return false;
}

void Decision::add(const ActionType type, const std::uint32_t durationMs) noexcept {
    if (actionCount < actions.size()) {
        actions[actionCount] = Action{type, durationMs};
        ++actionCount;
    }
}

Controller::Controller(const ControllerConfig config) noexcept
    : config_(config),
      safetyPolicy_(config.safety),
      profileReadiness_(
          assessRemoteStartReadiness(config.vehicleProfile, config.profilePolicy)) {}

Decision Controller::handle(
    const Event event,
    const domain::VehicleState& vehicle) noexcept {
    Decision decision{};
    decision.previousState = state_;
    decision.state = state_;
    decision.fault = fault_;
    decision.profileReadiness = profileReadiness_;

    switch (event.type) {
        case EventType::RemoteStartRequested:
            handleStartRequest(decision);
            break;

        case EventType::RemoteStopRequested:
            handleStopRequest(decision);
            break;

        case EventType::VehicleStateUpdated:
            handleVehicleUpdate(decision, vehicle);
            break;

        case EventType::TimerElapsed:
            handleTimer(decision, vehicle);
            break;

        case EventType::InfrastructureFailure:
            enterFault(
                decision,
                event.fault == FaultCode::None ? FaultCode::InternalError : event.fault);
            break;

        case EventType::ResetRequested:
            handleReset(decision, vehicle);
            break;
    }

    decision.state = state_;
    decision.fault = fault_;
    return decision;
}

void Controller::handleStartRequest(Decision& decision) noexcept {
    if (state_ != ControllerState::Idle) {
        decision.add(ActionType::NotifyRequestIgnored);
        return;
    }

    if (!profileReadiness_.ready()) {
        decision.add(ActionType::SecureOutputs);
        decision.add(ActionType::NotifyProfileRejected);
        return;
    }

    transition(decision, ControllerState::Authorizing);
    decision.add(ActionType::RequestVehicleState);
    decision.add(ActionType::ArmTimer, config_.authorizationTimeoutMs);
}

void Controller::handleStopRequest(Decision& decision) noexcept {
    switch (state_) {
        case ControllerState::Authorizing:
        case ControllerState::Preparing:
        case ControllerState::Cranking:
            completeStop(decision);
            break;

        case ControllerState::Running:
            beginStopping(decision);
            break;

        case ControllerState::Fault:
            decision.add(ActionType::SecureOutputs);
            decision.add(ActionType::NotifyRequestIgnored);
            break;

        case ControllerState::Idle:
            decision.add(ActionType::NotifyStopped);
            break;

        case ControllerState::Stopping:
            decision.add(ActionType::NotifyRequestIgnored);
            break;
    }
}

void Controller::handleVehicleUpdate(
    Decision& decision,
    const domain::VehicleState& vehicle) noexcept {
    switch (state_) {
        case ControllerState::Authorizing:
            decision.safety = safetyPolicy_.assessStart(vehicle);
            if (decision.safety.approved()) {
                beginPreparing(decision);
            } else {
                transition(decision, ControllerState::Idle);
                fault_ = FaultCode::None;
                decision.add(ActionType::CancelTimer);
                decision.add(ActionType::SecureOutputs);
                decision.add(ActionType::NotifyStartRejected);
            }
            break;

        case ControllerState::Preparing:
            decision.safety = safetyPolicy_.assessStart(vehicle);
            if (!decision.safety.approved()) {
                enterFault(decision, FaultCode::SafetyInterlock);
            }
            break;

        case ControllerState::Cranking:
            decision.safety = safetyPolicy_.assessCranking(vehicle);
            if (!decision.safety.approved()) {
                enterFault(decision, FaultCode::SafetyInterlock);
            } else if (engineIsRunning(vehicle)) {
                beginRunning(decision);
            }
            break;

        case ControllerState::Running:
            decision.safety = safetyPolicy_.assessRemoteRun(vehicle);
            if (!decision.safety.approved()) {
                enterFault(decision, FaultCode::SafetyInterlock);
            }
            break;

        case ControllerState::Stopping:
            if (vehicle.engineRpm.isFresh() && !engineIsRunning(vehicle)) {
                completeStop(decision);
            }
            break;

        case ControllerState::Idle:
        case ControllerState::Fault:
            break;
    }
}

void Controller::handleTimer(
    Decision& decision,
    const domain::VehicleState& vehicle) noexcept {
    switch (state_) {
        case ControllerState::Authorizing:
            enterFault(decision, FaultCode::AuthorizationTimeout);
            break;

        case ControllerState::Preparing:
            decision.safety = safetyPolicy_.assessStart(vehicle);
            if (decision.safety.approved()) {
                beginCranking(decision);
            } else {
                enterFault(decision, FaultCode::SafetyInterlock);
            }
            break;

        case ControllerState::Cranking:
            enterFault(decision, FaultCode::CrankTimeout);
            break;

        case ControllerState::Running:
            beginStopping(decision);
            break;

        case ControllerState::Stopping:
            enterFault(decision, FaultCode::StopTimeout);
            break;

        case ControllerState::Idle:
        case ControllerState::Fault:
            break;
    }
}

void Controller::handleReset(
    Decision& decision,
    const domain::VehicleState& vehicle) noexcept {
    if (state_ != ControllerState::Fault) {
        decision.add(ActionType::NotifyRequestIgnored);
        return;
    }

    const bool engineStopped = vehicle.engineRpm.isFresh() && !engineIsRunning(vehicle);
    const bool noCriticalFault = vehicle.criticalFaultPresent.isFresh() &&
                                 !vehicle.criticalFaultPresent.value;

    if (!engineStopped || !noCriticalFault) {
        decision.add(ActionType::SecureOutputs);
        decision.add(ActionType::NotifyResetRejected);
        return;
    }

    transition(decision, ControllerState::Idle);
    fault_ = FaultCode::None;
    decision.add(ActionType::CancelTimer);
    decision.add(ActionType::SecureOutputs);
    decision.add(ActionType::NotifyReady);
}

void Controller::beginPreparing(Decision& decision) noexcept {
    transition(decision, ControllerState::Preparing);
    decision.add(ActionType::CancelTimer);
    decision.add(ActionType::EnableIgnition);
    decision.add(ActionType::NotifyStartAccepted);
    decision.add(ActionType::ArmTimer, config_.preparationDelayMs);
}

void Controller::beginCranking(Decision& decision) noexcept {
    transition(decision, ControllerState::Cranking);
    decision.add(ActionType::CancelTimer);
    decision.add(ActionType::EngageStarter);
    decision.add(ActionType::ArmTimer, config_.maximumCrankTimeMs);
}

void Controller::beginRunning(Decision& decision) noexcept {
    transition(decision, ControllerState::Running);
    decision.add(ActionType::CancelTimer);
    decision.add(ActionType::DisengageStarter);
    decision.add(ActionType::NotifyRunning);
    decision.add(ActionType::ArmTimer, config_.maximumRemoteRunTimeMs);
}

void Controller::beginStopping(Decision& decision) noexcept {
    transition(decision, ControllerState::Stopping);
    decision.add(ActionType::CancelTimer);
    decision.add(ActionType::DisengageStarter);
    decision.add(ActionType::SecureOutputs);
    decision.add(ActionType::NotifyStopping);
    decision.add(ActionType::ArmTimer, config_.stopConfirmationTimeoutMs);
}

void Controller::completeStop(Decision& decision) noexcept {
    transition(decision, ControllerState::Idle);
    fault_ = FaultCode::None;
    decision.add(ActionType::CancelTimer);
    decision.add(ActionType::DisengageStarter);
    decision.add(ActionType::SecureOutputs);
    decision.add(ActionType::NotifyStopped);
}

void Controller::enterFault(Decision& decision, const FaultCode fault) noexcept {
    transition(decision, ControllerState::Fault);
    fault_ = fault;
    decision.add(ActionType::CancelTimer);
    decision.add(ActionType::DisengageStarter);
    decision.add(ActionType::SecureOutputs);
    decision.add(ActionType::NotifyFault);
}

void Controller::transition(
    Decision& decision,
    const ControllerState next) noexcept {
    state_ = next;
    decision.state = next;
}

bool Controller::engineIsRunning(
    const domain::VehicleState& vehicle) const noexcept {
    return vehicle.engineRpm.isFresh() &&
           vehicle.engineRpm.value >= safetyPolicy_.runningRpmThreshold();
}

const char* toString(const ControllerState state) noexcept {
    switch (state) {
        case ControllerState::Idle: return "idle";
        case ControllerState::Authorizing: return "authorizing";
        case ControllerState::Preparing: return "preparing";
        case ControllerState::Cranking: return "cranking";
        case ControllerState::Running: return "running";
        case ControllerState::Stopping: return "stopping";
        case ControllerState::Fault: return "fault";
    }
    return "unknown";
}

const char* toString(const FaultCode fault) noexcept {
    switch (fault) {
        case FaultCode::None: return "none";
        case FaultCode::SafetyInterlock: return "safety_interlock";
        case FaultCode::VehicleCommunication: return "vehicle_communication";
        case FaultCode::AuthorizationTimeout: return "authorization_timeout";
        case FaultCode::CrankTimeout: return "crank_timeout";
        case FaultCode::StopTimeout: return "stop_timeout";
        case FaultCode::ActuatorFailure: return "actuator_failure";
        case FaultCode::TimerFailure: return "timer_failure";
        case FaultCode::InternalError: return "internal_error";
    }
    return "unknown";
}

const char* toString(const ActionType action) noexcept {
    switch (action) {
        case ActionType::RequestVehicleState: return "request_vehicle_state";
        case ActionType::EnableIgnition: return "enable_ignition";
        case ActionType::EngageStarter: return "engage_starter";
        case ActionType::DisengageStarter: return "disengage_starter";
        case ActionType::SecureOutputs: return "secure_outputs";
        case ActionType::ArmTimer: return "arm_timer";
        case ActionType::CancelTimer: return "cancel_timer";
        case ActionType::NotifyProfileRejected: return "notify_profile_rejected";
        case ActionType::NotifyStartAccepted: return "notify_start_accepted";
        case ActionType::NotifyStartRejected: return "notify_start_rejected";
        case ActionType::NotifyRunning: return "notify_running";
        case ActionType::NotifyStopping: return "notify_stopping";
        case ActionType::NotifyStopped: return "notify_stopped";
        case ActionType::NotifyFault: return "notify_fault";
        case ActionType::NotifyReady: return "notify_ready";
        case ActionType::NotifyRequestIgnored: return "notify_request_ignored";
        case ActionType::NotifyResetRejected: return "notify_reset_rejected";
    }
    return "unknown";
}

}  // namespace bmw::remote::application
