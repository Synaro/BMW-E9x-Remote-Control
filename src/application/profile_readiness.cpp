#include "bmw_remote/application/profile_readiness.hpp"

#include <array>

namespace bmw::remote::application {
namespace {

constexpr std::array<domain::VehicleSignal, 8U> CoreRequiredSignals = {
    domain::VehicleSignal::BatteryMillivolts,
    domain::VehicleSignal::EngineRpm,
    domain::VehicleSignal::HoodClosed,
    domain::VehicleSignal::BrakePressed,
    domain::VehicleSignal::ParkingBrakeApplied,
    domain::VehicleSignal::Transmission,
    domain::VehicleSignal::Gear,
    domain::VehicleSignal::CriticalFaultPresent,
};

void assessSignal(
    ProfileReadinessAssessment& assessment,
    const domain::VehicleProfile& profile,
    const domain::VehicleSignal signal) noexcept {
    switch (profile.support(signal)) {
        case domain::SignalSupport::Unavailable:
            assessment.add(ProfileReadinessReason::MissingRequiredSignal);
            break;

        case domain::SignalSupport::Candidate:
            assessment.add(ProfileReadinessReason::UnverifiedRequiredSignal);
            break;

        case domain::SignalSupport::Verified:
            break;
    }
}

}  // namespace

ProfileReadinessAssessment assessRemoteStartReadiness(
    const domain::VehicleProfile& profile,
    const ProfileReadinessPolicy policy) noexcept {
    ProfileReadinessAssessment assessment{};

    for (const domain::VehicleSignal signal : CoreRequiredSignals) {
        assessSignal(assessment, profile, signal);
    }

    if (policy.requireVehicleSecured) {
        assessSignal(assessment, profile, domain::VehicleSignal::DoorsClosed);
        assessSignal(assessment, profile, domain::VehicleSignal::TrunkClosed);
    }

    if (profile.qualification < domain::QualificationStage::ReadOnlyValidated) {
        assessment.add(ProfileReadinessReason::QualificationTooLow);
    }

    if (profile.transmission == domain::Transmission::Unknown) {
        assessment.add(ProfileReadinessReason::TransmissionUnknown);
    } else if (profile.transmission == domain::Transmission::Manual &&
               !policy.allowManualTransmission) {
        assessment.add(ProfileReadinessReason::ManualTransmissionNotAllowed);
    }

    return assessment;
}

ProfileReadinessAssessment assessRemoteStartReadiness(
    const domain::VehicleProfile* const profile,
    const ProfileReadinessPolicy policy) noexcept {
    if (profile != nullptr) {
        return assessRemoteStartReadiness(*profile, policy);
    }

    ProfileReadinessAssessment assessment{};
    assessment.add(ProfileReadinessReason::ProfileNotSelected);
    return assessment;
}

}  // namespace bmw::remote::application
