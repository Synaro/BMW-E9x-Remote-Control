#pragma once

#include <cstdint>

#include "bmw_remote/infrastructure/ports.hpp"

namespace bmw::remote::infrastructure {

struct ActuatorFeedback final {
    bool available{false};
    bool ignitionActive{false};
    bool starterActive{false};
};

struct ActuatorSafetyConfig final {
    static constexpr std::uint32_t MaximumWatchdogTimeoutMs = 5'000U;
    static constexpr std::uint32_t MaximumStarterActiveMs = 10'000U;
    static constexpr std::uint32_t MaximumFeedbackGraceMs = 1'000U;

    std::uint32_t watchdogTimeoutMs{500U};
    std::uint32_t maximumStarterActiveMs{5'000U};
    std::uint32_t feedbackGraceMs{100U};
    bool requireFeedback{true};
};

enum class ActuatorSupervisorFault : std::uint8_t {
    None,
    InvalidConfiguration,
    InitializationFailure,
    ClockRegression,
    WatchdogExpired,
    HardwareInterlockLost,
    StarterTimeout,
    CommandSequence,
    DriverFailure,
    FeedbackUnavailable,
    FeedbackMismatch,
    SafingFailure,
};

struct ActuatorSupervisorStatus final {
    ActuatorSupervisorFault fault{ActuatorSupervisorFault::None};
    bool initialized{false};
    bool ignitionCommanded{false};
    bool starterCommanded{false};
    bool ignitionFeedbackConfirmed{false};
    bool safingFailed{false};

    [[nodiscard]] constexpr bool healthy() const noexcept {
        return initialized && fault == ActuatorSupervisorFault::None &&
               !safingFailed;
    }
};

class ActuatorSafetySupervisor final : public ActuatorPort {
public:
    explicit ActuatorSafetySupervisor(
        ActuatorPort& driver,
        ActuatorSafetyConfig config = {}) noexcept;

    [[nodiscard]] bool heartbeat(std::uint32_t nowMs) noexcept;
    [[nodiscard]] ActuatorSupervisorStatus poll(
        std::uint32_t nowMs,
        bool hardwareStartPermitted,
        ActuatorFeedback feedback = {}) noexcept;
    [[nodiscard]] bool resetFault(
        std::uint32_t nowMs,
        ActuatorFeedback feedback = {}) noexcept;

    bool enableIgnition() noexcept override;
    bool engageStarter() noexcept override;
    bool disengageStarter() noexcept override;
    bool releaseRemoteControl() noexcept override;
    bool secureOutputs() noexcept override;

    [[nodiscard]] constexpr ActuatorSupervisorStatus status() const noexcept {
        return {
            fault_,
            initialized_,
            ignitionCommanded_,
            starterCommanded_,
            ignitionFeedbackConfirmed_,
            safingFailed_};
    }

private:
    [[nodiscard]] bool configIsValid() const noexcept;
    [[nodiscard]] bool observeClock(std::uint32_t nowMs) noexcept;
    [[nodiscard]] bool canEnergize() noexcept;
    [[nodiscard]] bool safeDownstream() noexcept;
    void trip(ActuatorSupervisorFault fault) noexcept;

    ActuatorPort& driver_;
    ActuatorSafetyConfig config_{};
    ActuatorSupervisorFault fault_{ActuatorSupervisorFault::None};
    std::uint32_t lastObservedMs_{0U};
    std::uint32_t lastHeartbeatMs_{0U};
    std::uint32_t starterEngagedAtMs_{0U};
    std::uint32_t lastOutputChangeMs_{0U};
    bool initialized_{false};
    bool clockInitialized_{false};
    bool heartbeatInitialized_{false};
    bool hardwareStartPermitted_{false};
    bool ignitionCommanded_{false};
    bool starterCommanded_{false};
    bool ignitionFeedbackConfirmed_{false};
    bool safingFailed_{false};
};

[[nodiscard]] const char* toString(ActuatorSupervisorFault fault) noexcept;

}  // namespace bmw::remote::infrastructure
