#include <array>
#include <cstdint>
#include <iostream>

#include "bmw_remote/application/controller.hpp"
#include "bmw_remote/infrastructure/replay_vehicle_gateway.hpp"
#include "bmw_remote/infrastructure/runtime.hpp"
#include "bmw_remote/simulation/synthetic_can.hpp"

namespace {

using namespace bmw::remote;

class ConsoleActuators final : public infrastructure::ActuatorPort {
public:
    bool enableIgnition() noexcept override {
        std::cout << "  actuator: enable ignition\n";
        return true;
    }

    bool engageStarter() noexcept override {
        std::cout << "  actuator: engage starter\n";
        return true;
    }

    bool disengageStarter() noexcept override {
        std::cout << "  actuator: disengage starter\n";
        return true;
    }

    bool secureOutputs() noexcept override {
        std::cout << "  actuator: secure all outputs\n";
        return true;
    }
};

class ConsoleTimer final : public infrastructure::TimerPort {
public:
    bool arm(const std::uint32_t durationMs) noexcept override {
        std::cout << "  timer: arm " << durationMs << " ms\n";
        return true;
    }

    bool cancel() noexcept override {
        std::cout << "  timer: cancel\n";
        return true;
    }
};

class ConsoleNotifications final : public infrastructure::NotificationSink {
public:
    void publish(
        const application::ActionType notification,
        const application::ControllerState state,
        const application::FaultCode fault,
        const application::SafetyAssessment safety) noexcept override {
        std::cout << "  notification: " << application::toString(notification)
                  << " state=" << application::toString(state)
                  << " fault=" << application::toString(fault)
                  << " safety_mask=" << safety.reasons << '\n';
    }
};

void printDecision(
    const char* const label,
    const application::Decision& decision) {
    std::cout << label << ": "
              << application::toString(decision.previousState) << " -> "
              << application::toString(decision.state) << '\n';
}

}  // namespace

int main() {
    using namespace bmw::remote;

    simulation::SyntheticPowertrainState stopped{};
    simulation::SyntheticPowertrainState running{};
    running.engineRpm = 850U;

    simulation::SyntheticBodyState safeBody{};
    simulation::SyntheticBodyState hoodOpen = safeBody;
    hoodOpen.hoodClosed = false;

    const std::array<infrastructure::CanFrame, 6U> trace = {
        simulation::makeSyntheticPowertrainFrame(0U, stopped),
        simulation::makeSyntheticBodyFrame(0U, safeBody),
        simulation::makeSyntheticPowertrainFrame(1'800U, running),
        simulation::makeSyntheticBodyFrame(1'800U, safeBody),
        simulation::makeSyntheticPowertrainFrame(5'000U, running),
        simulation::makeSyntheticBodyFrame(5'000U, hoodOpen),
    };

    simulation::SyntheticCanDecoder decoder{};
    infrastructure::ReplayVehicleGateway gateway{
        trace.data(), trace.size(), decoder};
    ConsoleActuators actuators{};
    ConsoleTimer timer{};
    ConsoleNotifications notifications{};
    infrastructure::Runtime runtime{
        application::Controller{}, gateway, actuators, timer, notifications};

    domain::VehicleState vehicle{};

    if (!gateway.setElapsedTime(0U)) {
        return 1;
    }
    auto decision = runtime.dispatch(
        application::Event{application::EventType::RemoteStartRequested}, vehicle);
    printDecision("remote start", decision);

    vehicle = gateway.state();
    decision = runtime.dispatch(
        application::Event{application::EventType::VehicleStateUpdated}, vehicle);
    printDecision("safe snapshot", decision);

    decision = runtime.dispatch(
        application::Event{application::EventType::TimerElapsed}, vehicle);
    printDecision("preparation timer", decision);

    if (!gateway.setElapsedTime(1'800U) || !gateway.requestState()) {
        std::cerr << "Replay failed at 1800 ms\n";
        return 1;
    }
    vehicle = gateway.state();
    decision = runtime.dispatch(
        application::Event{application::EventType::VehicleStateUpdated}, vehicle);
    printDecision("engine running", decision);

    if (!gateway.setElapsedTime(5'000U) || !gateway.requestState()) {
        std::cerr << "Replay failed at 5000 ms\n";
        return 1;
    }
    vehicle = gateway.state();
    decision = runtime.dispatch(
        application::Event{application::EventType::VehicleStateUpdated}, vehicle);
    printDecision("hood opened", decision);

    const infrastructure::AssemblyStatistics stats = gateway.statistics();
    std::cout << "replay: " << stats.consumedFrames << " frames, "
              << stats.decodedSignals << " decoded signals\n";

    const bool expectedFault =
        decision.state == application::ControllerState::Fault &&
        decision.fault == application::FaultCode::SafetyInterlock &&
        decision.safety.contains(application::SafetyReason::HoodOpen);
    return expectedFault ? 0 : 2;
}
