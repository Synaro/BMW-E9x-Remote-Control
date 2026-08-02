#include "bmw_remote/infrastructure/actuator_safety_supervisor.hpp"

#include <cstdint>

namespace bmw::remote::infrastructure {
namespace {

constexpr std::uint32_t HalfCounterRange = 0x80000000U;

[[nodiscard]] bool strictlyNewer(
    const std::uint32_t candidate,
    const std::uint32_t previous) noexcept {
    const std::uint32_t distance = candidate - previous;
    return distance != 0U && distance < HalfCounterRange;
}

}  // namespace

ActuatorSafetySupervisor::ActuatorSafetySupervisor(
    ActuatorPort& driver,
    const ActuatorSafetyConfig config) noexcept
    : driver_(driver), config_(config) {
    if (!configIsValid()) {
        fault_ = ActuatorSupervisorFault::InvalidConfiguration;
        static_cast<void>(safeDownstream());
        return;
    }
    if (!safeDownstream()) {
        fault_ = ActuatorSupervisorFault::InitializationFailure;
        return;
    }
    initialized_ = true;
}

bool ActuatorSafetySupervisor::heartbeat(const std::uint32_t nowMs) noexcept {
    if (!observeClock(nowMs)) {
        return false;
    }
    if (!initialized_ || fault_ != ActuatorSupervisorFault::None) {
        return false;
    }
    if (heartbeatInitialized_ &&
        (ignitionCommanded_ || starterCommanded_) &&
        nowMs - lastHeartbeatMs_ > config_.watchdogTimeoutMs) {
        trip(ActuatorSupervisorFault::WatchdogExpired);
        return false;
    }
    lastHeartbeatMs_ = nowMs;
    heartbeatInitialized_ = true;
    return true;
}

ActuatorSupervisorStatus ActuatorSafetySupervisor::poll(
    const std::uint32_t nowMs,
    const bool hardwareStartPermitted,
    const ActuatorFeedback feedback) noexcept {
    if (!observeClock(nowMs)) {
        return status();
    }
    if (!initialized_ || fault_ != ActuatorSupervisorFault::None) {
        return status();
    }

    hardwareStartPermitted_ = hardwareStartPermitted;
    const bool outputsActive = ignitionCommanded_ || starterCommanded_;
    if (outputsActive && !hardwareStartPermitted_) {
        trip(ActuatorSupervisorFault::HardwareInterlockLost);
        return status();
    }
    if (outputsActive &&
        (!heartbeatInitialized_ ||
         nowMs - lastHeartbeatMs_ > config_.watchdogTimeoutMs)) {
        trip(ActuatorSupervisorFault::WatchdogExpired);
        return status();
    }
    if (starterCommanded_ &&
        nowMs - starterEngagedAtMs_ > config_.maximumStarterActiveMs) {
        trip(ActuatorSupervisorFault::StarterTimeout);
        return status();
    }

    const bool feedbackGraceElapsed =
        nowMs - lastOutputChangeMs_ > config_.feedbackGraceMs;
    if (feedbackGraceElapsed && feedback.available &&
        (feedback.ignitionActive != ignitionCommanded_ ||
         feedback.starterActive != starterCommanded_)) {
        trip(ActuatorSupervisorFault::FeedbackMismatch);
        return status();
    }
    if (feedbackGraceElapsed && config_.requireFeedback && outputsActive &&
        !feedback.available) {
        trip(ActuatorSupervisorFault::FeedbackUnavailable);
        return status();
    }
    if (feedback.available &&
        feedback.ignitionActive == ignitionCommanded_ &&
        feedback.starterActive == starterCommanded_) {
        ignitionFeedbackConfirmed_ = ignitionCommanded_;
    }
    return status();
}

bool ActuatorSafetySupervisor::resetFault(
    const std::uint32_t nowMs,
    const ActuatorFeedback feedback) noexcept {
    if (!observeClock(nowMs)) {
        return false;
    }
    if (!configIsValid()) {
        fault_ = ActuatorSupervisorFault::InvalidConfiguration;
        static_cast<void>(safeDownstream());
        return false;
    }
    if (!safeDownstream()) {
        fault_ = ActuatorSupervisorFault::SafingFailure;
        return false;
    }
    if ((config_.requireFeedback && !feedback.available) ||
        (feedback.available &&
         (feedback.ignitionActive || feedback.starterActive))) {
        fault_ = feedback.available
                     ? ActuatorSupervisorFault::FeedbackMismatch
                     : ActuatorSupervisorFault::FeedbackUnavailable;
        return false;
    }

    fault_ = ActuatorSupervisorFault::None;
    safingFailed_ = false;
    heartbeatInitialized_ = false;
    hardwareStartPermitted_ = false;
    initialized_ = true;
    return true;
}

bool ActuatorSafetySupervisor::enableIgnition() noexcept {
    if (!canEnergize()) {
        return false;
    }
    if (!driver_.enableIgnition()) {
        trip(ActuatorSupervisorFault::DriverFailure);
        return false;
    }
    ignitionCommanded_ = true;
    ignitionFeedbackConfirmed_ = !config_.requireFeedback;
    lastOutputChangeMs_ = lastObservedMs_;
    return true;
}

bool ActuatorSafetySupervisor::engageStarter() noexcept {
    if (!canEnergize()) {
        return false;
    }
    if (starterCommanded_ || !ignitionCommanded_ ||
        (config_.requireFeedback && !ignitionFeedbackConfirmed_)) {
        trip(ActuatorSupervisorFault::CommandSequence);
        return false;
    }
    if (!driver_.engageStarter()) {
        trip(ActuatorSupervisorFault::DriverFailure);
        return false;
    }
    starterCommanded_ = true;
    starterEngagedAtMs_ = lastObservedMs_;
    lastOutputChangeMs_ = lastObservedMs_;
    return true;
}

bool ActuatorSafetySupervisor::disengageStarter() noexcept {
    if (!driver_.disengageStarter()) {
        trip(ActuatorSupervisorFault::DriverFailure);
        return false;
    }
    starterCommanded_ = false;
    lastOutputChangeMs_ = lastObservedMs_;
    return true;
}

bool ActuatorSafetySupervisor::releaseRemoteControl() noexcept {
    if (!canEnergize()) {
        return false;
    }
    if (starterCommanded_) {
        trip(ActuatorSupervisorFault::CommandSequence);
        return false;
    }
    if (!driver_.releaseRemoteControl()) {
        trip(ActuatorSupervisorFault::DriverFailure);
        return false;
    }
    ignitionCommanded_ = false;
    starterCommanded_ = false;
    ignitionFeedbackConfirmed_ = false;
    lastOutputChangeMs_ = lastObservedMs_;
    return true;
}

bool ActuatorSafetySupervisor::secureOutputs() noexcept {
    const bool secured = safeDownstream();
    if (!secured) {
        fault_ = ActuatorSupervisorFault::SafingFailure;
    }
    return secured;
}

bool ActuatorSafetySupervisor::configIsValid() const noexcept {
    return config_.watchdogTimeoutMs != 0U &&
           config_.watchdogTimeoutMs <=
               ActuatorSafetyConfig::MaximumWatchdogTimeoutMs &&
           config_.maximumStarterActiveMs != 0U &&
           config_.maximumStarterActiveMs <=
               ActuatorSafetyConfig::MaximumStarterActiveMs &&
           config_.feedbackGraceMs <=
               ActuatorSafetyConfig::MaximumFeedbackGraceMs &&
           config_.feedbackGraceMs <= config_.watchdogTimeoutMs;
}

bool ActuatorSafetySupervisor::observeClock(
    const std::uint32_t nowMs) noexcept {
    if (clockInitialized_ && nowMs != lastObservedMs_ &&
        !strictlyNewer(nowMs, lastObservedMs_)) {
        trip(ActuatorSupervisorFault::ClockRegression);
        return false;
    }
    lastObservedMs_ = nowMs;
    clockInitialized_ = true;
    return true;
}

bool ActuatorSafetySupervisor::canEnergize() noexcept {
    if (!initialized_) {
        trip(ActuatorSupervisorFault::InitializationFailure);
        return false;
    }
    if (fault_ != ActuatorSupervisorFault::None) {
        return false;
    }
    if (!clockInitialized_ || !heartbeatInitialized_) {
        trip(ActuatorSupervisorFault::WatchdogExpired);
        return false;
    }
    if (!hardwareStartPermitted_) {
        trip(ActuatorSupervisorFault::HardwareInterlockLost);
        return false;
    }
    if (lastObservedMs_ - lastHeartbeatMs_ > config_.watchdogTimeoutMs) {
        trip(ActuatorSupervisorFault::WatchdogExpired);
        return false;
    }
    return true;
}

bool ActuatorSafetySupervisor::safeDownstream() noexcept {
    const bool starterReleased = driver_.disengageStarter();
    const bool outputsSecured = driver_.secureOutputs();
    ignitionCommanded_ = false;
    starterCommanded_ = false;
    ignitionFeedbackConfirmed_ = false;
    lastOutputChangeMs_ = lastObservedMs_;
    if (!starterReleased || !outputsSecured) {
        safingFailed_ = true;
        return false;
    }
    return true;
}

void ActuatorSafetySupervisor::trip(
    const ActuatorSupervisorFault fault) noexcept {
    if (fault_ == ActuatorSupervisorFault::None) {
        fault_ = fault;
    }
    static_cast<void>(safeDownstream());
}

const char* toString(const ActuatorSupervisorFault fault) noexcept {
    switch (fault) {
        case ActuatorSupervisorFault::None: return "none";
        case ActuatorSupervisorFault::InvalidConfiguration:
            return "invalid_configuration";
        case ActuatorSupervisorFault::InitializationFailure:
            return "initialization_failure";
        case ActuatorSupervisorFault::ClockRegression:
            return "clock_regression";
        case ActuatorSupervisorFault::WatchdogExpired:
            return "watchdog_expired";
        case ActuatorSupervisorFault::HardwareInterlockLost:
            return "hardware_interlock_lost";
        case ActuatorSupervisorFault::StarterTimeout:
            return "starter_timeout";
        case ActuatorSupervisorFault::CommandSequence:
            return "command_sequence";
        case ActuatorSupervisorFault::DriverFailure:
            return "driver_failure";
        case ActuatorSupervisorFault::FeedbackUnavailable:
            return "feedback_unavailable";
        case ActuatorSupervisorFault::FeedbackMismatch:
            return "feedback_mismatch";
        case ActuatorSupervisorFault::SafingFailure:
            return "safing_failure";
    }
    return "unknown";
}

}  // namespace bmw::remote::infrastructure
