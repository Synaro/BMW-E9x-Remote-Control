#include "bmw_remote/application/vehicle_start_observer.hpp"

namespace bmw::remote::application {
namespace {

using domain::DiagnosticAvailability;
using domain::StartSignalId;
using domain::StartSignalSnapshot;
using domain::TerminalPairState;

template <typename T>
void addEvidence(
    VehicleStartObservation& result,
    const StartSignalId id,
    const domain::DiagnosticSignal<T>& signal) {
    if (signal.known() && result.supportingSignalCount < result.supportingSignals.size()) {
        result.supportingSignals[result.supportingSignalCount++] =
            StartObservationEvidence{id, signal.source, signal.interval};
    }
}

void addDocumentedSignal(
    VehicleStartObservation& result,
    const StartSignalId id,
    const domain::DiagnosticSignal<bool>& signal) {
    if (signal.known() && result.observedSignalCount < result.observedSignals.size()) {
        result.observedSignals[result.observedSignalCount++] = DocumentedSignalObservation{
            id,
            signal.value,
            StartObservationQualification::DocumentedNotVehicleValidated,
            StartObservationEvidence{id, signal.source, signal.interval}};
    }
}

void collectDocumentedSignals(
    VehicleStartObservation& result,
    const StartSignalSnapshot& snapshot) {
    addDocumentedSignal(result, StartSignalId::SstA, snapshot.sstA);
    addDocumentedSignal(result, StartSignalId::SstB, snapshot.sstB);
    addDocumentedSignal(result, StartSignalId::KeyRast, snapshot.keyRast);
    addDocumentedSignal(result, StartSignalId::Brake, snapshot.brake);
    addDocumentedSignal(
        result, StartSignalId::ParkNeutralOrClutch, snapshot.parkNeutralOrClutch);
    addDocumentedSignal(result, StartSignalId::Mfs, snapshot.mfs);
    addDocumentedSignal(result, StartSignalId::StartDme, snapshot.startDme);
    addDocumentedSignal(result, StartSignalId::StartRelease, snapshot.startRelease);
}

template <typename T>
void addMissing(
    VehicleStartObservation& result,
    const StartSignalId id,
    const domain::DiagnosticSignal<T>& signal) {
    if (!signal.known()) {
        result.missingSignals[result.missingSignalCount++] = id;
    }
}

void collectMissing(VehicleStartObservation& result, const StartSignalSnapshot& snapshot) {
    addMissing(result, StartSignalId::SstA, snapshot.sstA);
    addMissing(result, StartSignalId::SstB, snapshot.sstB);
    addMissing(result, StartSignalId::KeyRast, snapshot.keyRast);
    addMissing(result, StartSignalId::Brake, snapshot.brake);
    addMissing(result, StartSignalId::ParkNeutralOrClutch, snapshot.parkNeutralOrClutch);
    addMissing(result, StartSignalId::Mfs, snapshot.mfs);
    addMissing(result, StartSignalId::StartDme, snapshot.startDme);
    addMissing(result, StartSignalId::StartRelease, snapshot.startRelease);
    addMissing(result, StartSignalId::Klemmenstatus, snapshot.klemmenstatus);
    addMissing(result, StartSignalId::Kl15EnableInhibitor, snapshot.kl15EnableInhibitor);
    addMissing(result, StartSignalId::Kl15DisableInhibitor, snapshot.kl15DisableInhibitor);
    addMissing(result, StartSignalId::Kl50EnableInhibitor, snapshot.kl50EnableInhibitor);
    addMissing(result, StartSignalId::EngineRpm, snapshot.engineRpm);
}

bool hasCommunicationError(const StartSignalSnapshot& snapshot) {
    return snapshot.klemmenstatus.availability == DiagnosticAvailability::CommunicationError ||
           snapshot.engineRpm.availability == DiagnosticAvailability::CommunicationError;
}

bool isKnownPair(const StartSignalSnapshot& snapshot) {
    return snapshot.klemmenstatus.known() && snapshot.engineRpm.known();
}

}  // namespace

VehicleStartObservation VehicleStartObserver::observe(
    const StartSignalSnapshot& current,
    const StartSignalSnapshot* previous) const {
    VehicleStartObservation result{};
    collectMissing(result, current);
    collectDocumentedSignals(result, current);

    if (hasCommunicationError(current)) {
        return result;
    }

    if (isKnownPair(current)) {
        const auto terminals = domain::decodeKlemmenstatus(current.klemmenstatus.value);
        if (!terminals.allFieldsDocumented || !terminals.rawValueObservedOnTestVehicle) {
            return result;
        }

        addEvidence(result, StartSignalId::Klemmenstatus, current.klemmenstatus);
        addEvidence(result, StartSignalId::EngineRpm, current.engineRpm);
        result.primaryStateQualification =
            StartObservationQualification::ConfirmedPhase3dSequence;

        const bool rpmZero = current.engineRpm.value == 0.0F;
        const bool previousPairKnown = previous != nullptr && isKnownPair(*previous);
        domain::KlemmenstatusDecoded previousTerminals{};
        if (previousPairKnown) {
            previousTerminals = domain::decodeKlemmenstatus(previous->klemmenstatus.value);
        }

        if (terminals.raw == 0x40U) {
            if (!rpmZero && previousPairKnown && previousTerminals.raw == 0x45U) {
                result.primaryVehicleState = VehicleStartObservedState::ShutdownObserved;
            } else if (rpmZero && previousPairKnown && previousTerminals.raw == 0x40U &&
                       previous->engineRpm.value > 0.0F) {
                result.primaryVehicleState = VehicleStartObservedState::EngineStoppedObserved;
            } else if (rpmZero) {
                result.primaryVehicleState = VehicleStartObservedState::VehicleRest;
            }
            return result;
        }
        if (terminals.raw == 0x41U && rpmZero) {
            result.primaryVehicleState = VehicleStartObservedState::KeyPresent;
            return result;
        }
        if (terminals.raw == 0x45U) {
            if (rpmZero) {
                result.primaryVehicleState = VehicleStartObservedState::TerminalReady;
            } else if (previousPairKnown && previousTerminals.raw == 0x55U) {
                result.primaryVehicleState = VehicleStartObservedState::EngineRotatingObserved;
            } else if (previousPairKnown && previousTerminals.raw == 0x45U &&
                       previous->engineRpm.value > 0.0F) {
                result.primaryVehicleState = VehicleStartObservedState::EngineRunningObserved;
            }
            return result;
        }
        if (terminals.raw == 0x55U && current.engineRpm.value > 0.0F) {
            result.primaryVehicleState = VehicleStartObservedState::CrankingObserved;
            return result;
        }
    }

    if (current.keyRast.known() && current.keyRast.value) {
        result.primaryVehicleState = VehicleStartObservedState::KeyPresent;
        result.primaryStateQualification =
            StartObservationQualification::DocumentedNotVehicleValidated;
        addEvidence(result, StartSignalId::KeyRast, current.keyRast);
    }
    return result;
}

}  // namespace bmw::remote::application
