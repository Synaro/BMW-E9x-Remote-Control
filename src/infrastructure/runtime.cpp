#include "bmw_remote/infrastructure/runtime.hpp"

namespace bmw::remote::infrastructure {

Runtime::Runtime(
    application::Controller controller,
    VehicleGateway& vehicleGateway,
    ActuatorPort& actuators,
    TimerPort& timer,
    NotificationSink& notifications,
    DiagnosticJournal* const diagnosticJournal) noexcept
    : controller_(controller),
      vehicleGateway_(vehicleGateway),
      actuators_(actuators),
      timer_(timer),
      notifications_(notifications),
      diagnosticJournal_(diagnosticJournal) {}

application::Decision Runtime::dispatch(
    const application::Event event,
    const domain::VehicleState& vehicle,
    const std::uint32_t timestampMs) noexcept {
    application::Decision decision = controller_.handle(event, vehicle);
    if (diagnosticJournal_ != nullptr) {
        diagnosticJournal_->observe(event, decision, timestampMs);
    }
    const application::FaultCode executionFault = execute(decision);

    if (executionFault == application::FaultCode::None) {
        return decision;
    }

    if (diagnosticJournal_ != nullptr) {
        diagnosticJournal_->recordInfrastructureFailure(
            event.type,
            decision.state,
            executionFault,
            timestampMs);
    }
    const application::Event failureEvent =
        application::Event::infrastructureFailure(executionFault);
    application::Decision faultDecision =
        controller_.handle(failureEvent, vehicle);
    if (diagnosticJournal_ != nullptr) {
        diagnosticJournal_->observe(failureEvent, faultDecision, timestampMs);
    }
    executeSafing(faultDecision, timestampMs);
    return faultDecision;
}

application::FaultCode Runtime::execute(
    const application::Decision& decision) noexcept {
    for (std::size_t index = 0U; index < decision.actionCount; ++index) {
        const application::FaultCode fault = executeAction(decision.actions[index], decision);
        if (fault != application::FaultCode::None) {
            return fault;
        }
    }

    return application::FaultCode::None;
}

application::FaultCode Runtime::executeAction(
    const application::Action& action,
    const application::Decision& decision) noexcept {
    using application::ActionType;
    using application::FaultCode;

    switch (action.type) {
        case ActionType::RequestVehicleState:
            return vehicleGateway_.requestState()
                       ? FaultCode::None
                       : FaultCode::VehicleCommunication;

        case ActionType::EnableIgnition:
            return actuators_.enableIgnition()
                       ? FaultCode::None
                       : FaultCode::ActuatorFailure;

        case ActionType::EngageStarter:
            return actuators_.engageStarter()
                       ? FaultCode::None
                       : FaultCode::ActuatorFailure;

        case ActionType::DisengageStarter:
            return actuators_.disengageStarter()
                       ? FaultCode::None
                       : FaultCode::ActuatorFailure;

        case ActionType::ReleaseRemoteControl:
            return actuators_.releaseRemoteControl()
                       ? FaultCode::None
                       : FaultCode::ActuatorFailure;

        case ActionType::SecureOutputs:
            return actuators_.secureOutputs()
                       ? FaultCode::None
                       : FaultCode::ActuatorFailure;

        case ActionType::ArmTimer:
            return timer_.arm(action.durationMs)
                       ? FaultCode::None
                       : FaultCode::TimerFailure;

        case ActionType::CancelTimer:
            return timer_.cancel()
                       ? FaultCode::None
                       : FaultCode::TimerFailure;

        case ActionType::NotifyProfileRejected:
        case ActionType::NotifyRemoteStartDisabled:
        case ActionType::NotifyStartAccepted:
        case ActionType::NotifyStartRejected:
        case ActionType::NotifyRunning:
        case ActionType::NotifyTakeoverPending:
        case ActionType::NotifyTakeoverComplete:
        case ActionType::NotifyStopping:
        case ActionType::NotifyStopped:
        case ActionType::NotifyFault:
        case ActionType::NotifyReady:
        case ActionType::NotifyRequestIgnored:
        case ActionType::NotifyResetRejected:
            notifications_.publish(
                action.type,
                decision.state,
                decision.fault,
                decision.safety,
                decision.profileReadiness);
            return FaultCode::None;
    }

    return FaultCode::InternalError;
}

void Runtime::executeSafing(
    const application::Decision& decision,
    const std::uint32_t timestampMs) noexcept {
    using application::ActionType;
    using application::FaultCode;

    for (std::size_t index = 0U; index < decision.actionCount; ++index) {
        const application::Action& action = decision.actions[index];

        switch (action.type) {
            case ActionType::CancelTimer:
                if (!timer_.cancel() && diagnosticJournal_ != nullptr) {
                    diagnosticJournal_->recordSafingFailure(
                        decision.state,
                        FaultCode::TimerFailure,
                        timestampMs);
                }
                break;

            case ActionType::DisengageStarter:
                if (!actuators_.disengageStarter() &&
                    diagnosticJournal_ != nullptr) {
                    diagnosticJournal_->recordSafingFailure(
                        decision.state,
                        FaultCode::ActuatorFailure,
                        timestampMs);
                }
                break;

            case ActionType::SecureOutputs:
                if (!actuators_.secureOutputs() &&
                    diagnosticJournal_ != nullptr) {
                    diagnosticJournal_->recordSafingFailure(
                        decision.state,
                        FaultCode::ActuatorFailure,
                        timestampMs);
                }
                break;

            case ActionType::NotifyFault:
                notifications_.publish(
                    action.type,
                    decision.state,
                    decision.fault,
                    decision.safety,
                    decision.profileReadiness);
                break;

            default:
                break;
        }
    }
}

}  // namespace bmw::remote::infrastructure
