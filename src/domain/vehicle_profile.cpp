#include "bmw_remote/domain/vehicle_profile.hpp"

#include <cstring>

namespace bmw::remote::domain {

const VehicleProfile* VehicleProfileRegistry::find(const char* const id) const noexcept {
    if (id == nullptr || profiles_ == nullptr) {
        return nullptr;
    }

    for (std::size_t index = 0U; index < profileCount_; ++index) {
        if (profiles_[index].id != nullptr && std::strcmp(profiles_[index].id, id) == 0) {
            return &profiles_[index];
        }
    }
    return nullptr;
}

const char* toString(const VehiclePlatform platform) noexcept {
    switch (platform) {
        case VehiclePlatform::Unknown: return "unknown";
        case VehiclePlatform::BmwE9x: return "bmw_e9x";
    }
    return "unknown";
}

const char* toString(const BodyVariant body) noexcept {
    switch (body) {
        case BodyVariant::Unknown: return "unknown";
        case BodyVariant::E90: return "e90";
        case BodyVariant::E91: return "e91";
        case BodyVariant::E92: return "e92";
        case BodyVariant::E93: return "e93";
    }
    return "unknown";
}

const char* toString(const QualificationStage stage) noexcept {
    switch (stage) {
        case QualificationStage::Discovery: return "discovery";
        case QualificationStage::ReadOnlyValidated: return "read_only_validated";
        case QualificationStage::BenchValidated: return "bench_validated";
        case QualificationStage::VehicleQualified: return "vehicle_qualified";
    }
    return "unknown";
}

const char* toString(const SignalSupport support) noexcept {
    switch (support) {
        case SignalSupport::Unavailable: return "unavailable";
        case SignalSupport::Candidate: return "candidate";
        case SignalSupport::Verified: return "verified";
    }
    return "unknown";
}

const char* toString(const HoodInterlockSource source) noexcept {
    switch (source) {
        case HoodInterlockSource::Unspecified: return "unspecified";
        case HoodInterlockSource::VehicleSignal: return "vehicle_signal";
        case HoodInterlockSource::ExternalDiscreteInput: return "external_discrete_input";
        case HoodInterlockSource::Synthetic: return "synthetic";
    }
    return "unknown";
}

}  // namespace bmw::remote::domain
