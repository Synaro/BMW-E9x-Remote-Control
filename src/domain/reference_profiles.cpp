#include "bmw_remote/domain/reference_profiles.hpp"

namespace bmw::remote::domain::profiles {
namespace {

constexpr std::array<SignalSupport, vehicleSignalCount()> CandidateSignals = {
    SignalSupport::Candidate,
    SignalSupport::Candidate,
    SignalSupport::Candidate,
    SignalSupport::Candidate,
    SignalSupport::Candidate,
    SignalSupport::Candidate,
    SignalSupport::Candidate,
    SignalSupport::Candidate,
    SignalSupport::Candidate,
    SignalSupport::Candidate,
};

constexpr VehicleProfile Profiles[] = {
    {
        "bmw-e90-2009-n47d20c-automatic",
        "BMW E90 2009 N47D20C automatic",
        VehiclePlatform::BmwE9x,
        BodyVariant::E90,
        2009U,
        2009U,
        "N47D20C",
        FuelType::Diesel,
        Transmission::Automatic,
        QualificationStage::Discovery,
        CandidateSignals,
    },
};

constexpr VehicleProfileRegistry Registry{Profiles, sizeof(Profiles) / sizeof(Profiles[0])};

}  // namespace

const VehicleProfile& e90_2009_n47d20c_automatic() noexcept {
    return Profiles[0];
}

const VehicleProfileRegistry& registry() noexcept {
    return Registry;
}

}  // namespace bmw::remote::domain::profiles
