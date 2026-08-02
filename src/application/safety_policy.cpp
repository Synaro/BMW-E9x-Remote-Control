#include "bmw_remote/application/safety_policy.hpp"

namespace bmw::remote::application {
namespace {

template <typename T>
void requireFresh(
    SafetyAssessment& assessment,
    const domain::Observed<T>& signal) noexcept {
    if (!signal.isFresh()) {
        assessment.add(SafetyReason::SignalUnavailable);
    }
}

}  // namespace

SafetyAssessment SafetyPolicy::assessCommon(
    const domain::VehicleState& vehicle,
    const bool requireDoorsClosed,
    const bool requireTrunkClosed,
    const bool prohibitBrake) const noexcept {
    SafetyAssessment assessment{};

    requireFresh(assessment, vehicle.batteryMillivolts);
    requireFresh(assessment, vehicle.parkingBrakeApplied);
    requireFresh(assessment, vehicle.transmission);
    requireFresh(assessment, vehicle.gear);
    requireFresh(assessment, vehicle.criticalFaultPresent);

    if (prohibitBrake) {
        requireFresh(assessment, vehicle.brakePressed);
    }

    if (config_.requireHoodClosed) {
        requireFresh(assessment, vehicle.hoodClosed);
    }

    if (requireDoorsClosed) {
        requireFresh(assessment, vehicle.doorsClosed);
    }

    if (requireTrunkClosed) {
        requireFresh(assessment, vehicle.trunkClosed);
    }

    if (vehicle.batteryMillivolts.isFresh() &&
        vehicle.batteryMillivolts.value < config_.minimumBatteryMillivolts) {
        assessment.add(SafetyReason::BatteryTooLow);
    }

    if (config_.requireHoodClosed &&
        vehicle.hoodClosed.isFresh() &&
        !vehicle.hoodClosed.value) {
        assessment.add(SafetyReason::HoodOpen);
    }

    if (requireDoorsClosed &&
        vehicle.doorsClosed.isFresh() &&
        !vehicle.doorsClosed.value) {
        assessment.add(SafetyReason::DoorOpen);
    }

    if (requireTrunkClosed &&
        vehicle.trunkClosed.isFresh() &&
        !vehicle.trunkClosed.value) {
        assessment.add(SafetyReason::TrunkOpen);
    }

    if (prohibitBrake &&
        vehicle.brakePressed.isFresh() &&
        vehicle.brakePressed.value) {
        assessment.add(SafetyReason::BrakePressed);
    }

    if (vehicle.parkingBrakeApplied.isFresh() &&
        !vehicle.parkingBrakeApplied.value) {
        assessment.add(SafetyReason::ParkingBrakeReleased);
    }

    if (vehicle.criticalFaultPresent.isFresh() &&
        vehicle.criticalFaultPresent.value) {
        assessment.add(SafetyReason::CriticalVehicleFault);
    }

    if (vehicle.transmission.isFresh() && vehicle.gear.isFresh()) {
        bool transmissionIsSafe = false;

        switch (vehicle.transmission.value) {
            case domain::Transmission::Automatic:
                transmissionIsSafe = vehicle.gear.value == domain::Gear::Park;
                break;

            case domain::Transmission::Manual:
                transmissionIsSafe = config_.allowManualTransmission &&
                                     vehicle.gear.value == domain::Gear::Neutral;
                break;

            case domain::Transmission::Unknown:
                break;
        }

        if (!transmissionIsSafe) {
            assessment.add(SafetyReason::TransmissionUnsafe);
        }
    }

    return assessment;
}

SafetyAssessment SafetyPolicy::assessStart(
    const domain::VehicleState& vehicle) const noexcept {
    SafetyAssessment assessment = assessCommon(
        vehicle,
        config_.requireVehicleSecured,
        config_.requireVehicleSecured,
        true);
    requireFresh(assessment, vehicle.engineRpm);

    if (vehicle.engineRpm.isFresh() &&
        vehicle.engineRpm.value >= config_.runningRpmThreshold) {
        assessment.add(SafetyReason::EngineAlreadyRunning);
    }

    return assessment;
}

SafetyAssessment SafetyPolicy::assessCranking(
    const domain::VehicleState& vehicle) const noexcept {
    SafetyAssessment assessment = assessCommon(
        vehicle,
        config_.requireVehicleSecured,
        config_.requireVehicleSecured,
        true);
    requireFresh(assessment, vehicle.engineRpm);
    return assessment;
}

SafetyAssessment SafetyPolicy::assessRemoteRun(
    const domain::VehicleState& vehicle) const noexcept {
    SafetyAssessment assessment = assessCommon(
        vehicle,
        config_.requireVehicleSecured,
        config_.requireVehicleSecured,
        true);
    requireFresh(assessment, vehicle.engineRpm);

    if (vehicle.engineRpm.isFresh() &&
        vehicle.engineRpm.value < config_.runningRpmThreshold) {
        assessment.add(SafetyReason::EngineNotRunning);
    }

    return assessment;
}

SafetyAssessment SafetyPolicy::assessDriverTakeover(
    const domain::VehicleState& vehicle) const noexcept {
    SafetyAssessment assessment = assessCommon(
        vehicle,
        false,
        config_.requireVehicleSecured,
        false);
    requireFresh(assessment, vehicle.engineRpm);

    if (vehicle.engineRpm.isFresh() &&
        vehicle.engineRpm.value < config_.runningRpmThreshold) {
        assessment.add(SafetyReason::EngineNotRunning);
    }

    return assessment;
}

}  // namespace bmw::remote::application
