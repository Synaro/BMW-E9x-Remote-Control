#pragma once

#include <cstddef>
#include <cstdint>

#include "bmw_remote/application/controller.hpp"
#include "bmw_remote/application/user_settings.hpp"

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
    virtual bool releaseRemoteControl() noexcept = 0;
    virtual bool secureOutputs() noexcept = 0;
};

class TimerPort {
public:
    virtual ~TimerPort() = default;
    virtual bool arm(std::uint32_t durationMs) noexcept = 0;
    virtual bool cancel() noexcept = 0;
};

class UserSettingsStore {
public:
    virtual ~UserSettingsStore() = default;
    virtual bool load(application::UserSettings& settings) noexcept = 0;
    virtual bool save(const application::UserSettings& settings) noexcept = 0;
};

class SettingsTransportPort {
public:
    virtual ~SettingsTransportPort() = default;
    virtual bool send(
        const std::uint8_t* data,
        std::size_t size) noexcept = 0;
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
