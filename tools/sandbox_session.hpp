#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>

#include "bmw_remote/application/controller.hpp"
#include "bmw_remote/application/feature_catalog.hpp"
#include "bmw_remote/application/telemetry_monitor.hpp"
#include "bmw_remote/domain/vehicle_state.hpp"
#include "bmw_remote/infrastructure/actuator_safety_supervisor.hpp"

namespace bmw::remote::host {

struct SandboxSnapshot final {
    std::uint32_t timeMs{0U};
    application::ControllerState state{application::ControllerState::Idle};
    application::FaultCode fault{application::FaultCode::None};
    infrastructure::ActuatorSupervisorFault supervisorFault{
        infrastructure::ActuatorSupervisorFault::None};
    bool timerArmed{false};
    std::uint32_t timerDueMs{0U};
    std::uint32_t timerDurationMs{0U};
    bool ignitionActive{false};
    bool starterActive{false};
    bool hardwareStartPermitted{true};
    bool hoodMonitoringRequired{true};
    domain::VehicleState vehicle{};
    application::FeatureRequests requestedFeatures{};
    application::TelemetryReport telemetry{};
    application::EventType lastEvent{application::EventType::VehicleStateUpdated};
    application::Decision lastDecision{};
    std::size_t diagnosticRecords{0U};
};

struct SandboxResult final {
    bool ok{true};
    std::string error{};
    SandboxSnapshot snapshot{};
};

class SandboxSession final {
public:
    SandboxSession();
    ~SandboxSession();

    SandboxSession(SandboxSession&&) noexcept;
    SandboxSession& operator=(SandboxSession&&) noexcept;
    SandboxSession(const SandboxSession&) = delete;
    SandboxSession& operator=(const SandboxSession&) = delete;

    [[nodiscard]] SandboxResult execute(const std::string& command);
    [[nodiscard]] SandboxSnapshot snapshot() const noexcept;

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};

[[nodiscard]] std::string encodeSandboxResult(const SandboxResult& result);
int runSandboxProtocol(std::istream& input, std::ostream& output);

}  // namespace bmw::remote::host
