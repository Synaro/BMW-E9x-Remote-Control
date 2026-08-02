#pragma once

#include <cstdint>

#include "bmw_remote/application/controller.hpp"

namespace bmw::remote::infrastructure {

class VehicleGateway {
public:
    virtual ~VehicleGateway() = default;
    virtual bool requestState() noexcept = 0;
};

class ActuatorPort {
public:
    virtual ~ActuatorPort() = default;
    virtual bool enableIgnition() noexcept = 0;
    virtual bool engageStarter() noexcept = 0;
    virtual bool disengageStarter() noexcept = 0;
    virtual bool secureOutputs() noexcept = 0;
};

class TimerPort {
public:
    virtual ~TimerPort() = default;
    virtual bool arm(std::uint32_t durationMs) noexcept = 0;
    virtual bool cancel() noexcept = 0;
};

class NotificationSink {
public:
    virtual ~NotificationSink() = default;
    virtual void publish(
        application::ActionType notification,
        application::ControllerState state,
        application::FaultCode fault,
        application::SafetyAssessment safety,
        application::ProfileReadinessAssessment profileReadiness) noexcept = 0;
};

}  // namespace bmw::remote::infrastructure
