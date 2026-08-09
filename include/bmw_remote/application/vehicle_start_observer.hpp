#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "bmw_remote/domain/start_observation.hpp"

namespace bmw::remote::application {

enum class VehicleStartObservedState : std::uint8_t {
    Unknown,
    VehicleRest,
    KeyPresent,
    TerminalReady,
    CrankingObserved,
    EngineRotatingObserved,
    EngineRunningObserved,
    ShutdownObserved,
    EngineStoppedObserved,
};

enum class StartObservationQualification : std::uint8_t {
    Unknown,
    DocumentedNotVehicleValidated,
    ConfirmedPhase3dSequence,
};

struct StartObservationEvidence {
    domain::StartSignalId signal{domain::StartSignalId::Klemmenstatus};
    domain::DiagnosticSource source{domain::DiagnosticSource::None};
    domain::AcquisitionInterval interval{};
};

struct DocumentedSignalObservation {
    domain::StartSignalId signal{domain::StartSignalId::SstA};
    bool rawValue{false};
    StartObservationQualification qualification{
        StartObservationQualification::DocumentedNotVehicleValidated};
    StartObservationEvidence evidence{};
};

struct VehicleStartObservation {
    VehicleStartObservedState primaryVehicleState{VehicleStartObservedState::Unknown};
    StartObservationQualification primaryStateQualification{
        StartObservationQualification::Unknown};
    std::array<DocumentedSignalObservation, 8U> observedSignals{};
    std::size_t observedSignalCount{0U};
    std::array<StartObservationEvidence, 4U> supportingSignals{};
    std::size_t supportingSignalCount{0U};
    std::array<domain::StartSignalId, static_cast<std::size_t>(domain::StartSignalId::Count)>
        missingSignals{};
    std::size_t missingSignalCount{0U};
};

class VehicleStartObserver {
  public:
    VehicleStartObservation observe(
        const domain::StartSignalSnapshot& current,
        const domain::StartSignalSnapshot* previous = nullptr) const;
};

}  // namespace bmw::remote::application
