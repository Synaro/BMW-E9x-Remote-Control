#include "bmw_remote/infrastructure/runtime.hpp"

namespace bmw::remote::infrastructure {

Runtime::Runtime(
    application::Controller controller,
    VehicleGateway& vehicleGateway,
    ActuatorPort& actuators,
    TimerPort& timer,
    NotificationSink& notifications) noexcept
    : controller_(controller),
      vehicleGateway_(vehicleGateway),
      actuators_(actuators),
      timer_(timer),
      notifications_(notifications) {}

application::Decision Runtime::dispatch(
    const application::Event event,
    const domain::VehicleState& vehicle) noexcept {
    application::Decision decision = controller_.handle(event, vehicle);
    const application::FaultCode executionFault = execute(decision);

    if (executionFault == application::FaultCode::None) {
        return decision;
    }

    application::Decision faultDecision = controller_.handle(
        application::Event::infrastructureFailure(executionFault),
        vehicle);
    executeSafing(faultDecision);
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

void Runtime::executeSafing(const application::Decision& decision) noexcept {
    using application::ActionType;

    for (std::size_t index = 0U; index < decision.actionCount; ++index) {
        const application::Action& action = decision.actions[index];

        switch (action.type) {
            case ActionType::CancelTimer:
                static_cast<void>(timer_.cancel());
                break;

            case ActionType::DisengageStarter:
                static_cast<void>(actuators_.disengageStarter());
                break;

            case ActionType::SecureOutputs:
                static_cast<void>(actuators_.secureOutputs());
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
