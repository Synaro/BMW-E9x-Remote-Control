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
    StartRequestObserved,
    StartAllowedObserved,
    CrankingObserved,
    EngineRotatingObserved,
    EngineRunningObserved,
    ShutdownObserved,
    EngineStoppedObserved,
};

enum class StartObservationQualification : std::uint8_t {
    Unknown,
    DocumentedSignalNotVehicleValidated,
    ConfirmedPhase3dSequence,
};

struct StartObservationEvidence {
    domain::StartSignalId signal{domain::StartSignalId::Klemmenstatus};
    domain::DiagnosticSource source{domain::DiagnosticSource::None};
    domain::AcquisitionInterval interval{};
};

struct VehicleStartObservation {
    VehicleStartObservedState state{VehicleStartObservedState::Unknown};
    StartObservationQualification qualification{StartObservationQualification::Unknown};
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
