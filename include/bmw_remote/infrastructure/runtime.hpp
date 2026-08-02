#pragma once

#include "bmw_remote/application/controller.hpp"
#include "bmw_remote/domain/vehicle_state.hpp"
#include "bmw_remote/infrastructure/diagnostic_journal.hpp"
#include "bmw_remote/infrastructure/ports.hpp"

namespace bmw::remote::infrastructure {

class Runtime final {
public:
    Runtime(
        application::Controller controller,
        VehicleGateway& vehicleGateway,
        ActuatorPort& actuators,
        TimerPort& timer,
        NotificationSink& notifications,
        DiagnosticJournal* diagnosticJournal = nullptr) noexcept;

    [[nodiscard]] application::Decision dispatch(
        application::Event event,
        const domain::VehicleState& vehicle,
        std::uint32_t timestampMs = 0U) noexcept;

    [[nodiscard]] constexpr application::ControllerState state() const noexcept {
        return controller_.state();
    }

private:
    [[nodiscard]] application::FaultCode execute(
        const application::Decision& decision) noexcept;
    [[nodiscard]] application::FaultCode executeAction(
        const application::Action& action,
        const application::Decision& decision) noexcept;
    void executeSafing(
        const application::Decision& decision,
        std::uint32_t timestampMs) noexcept;

    application::Controller controller_;
    VehicleGateway& vehicleGateway_;
    ActuatorPort& actuators_;
    TimerPort& timer_;
    NotificationSink& notifications_;
    DiagnosticJournal* diagnosticJournal_{nullptr};
};

}  // namespace bmw::remote::infrastructure
