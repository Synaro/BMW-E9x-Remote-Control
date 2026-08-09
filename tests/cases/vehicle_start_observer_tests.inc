using bmw::remote::application::StartObservationQualification;
using bmw::remote::application::DocumentedSignalObservation;
using bmw::remote::application::VehicleStartObservedState;
using bmw::remote::application::VehicleStartObserver;
using bmw::remote::domain::DiagnosticAvailability;
using bmw::remote::domain::DiagnosticSignal;
using bmw::remote::domain::DiagnosticSource;
using bmw::remote::domain::KeyValidityState;
using bmw::remote::domain::Kl15DisableInhibitor;
using bmw::remote::domain::Kl15EnableInhibitor;
using bmw::remote::domain::Kl50EnableInhibitor;
using bmw::remote::domain::StartSignalSnapshot;
using bmw::remote::domain::TerminalPairState;

StartSignalSnapshot observedCasDdeSnapshot(
    const std::uint8_t klemmenstatus,
    const float rpm,
    const std::uint64_t casStart,
    const std::uint64_t casEnd,
    const std::uint64_t ddeStart,
    const std::uint64_t ddeEnd) {
    StartSignalSnapshot snapshot{};
    snapshot.klemmenstatus = DiagnosticSignal<std::uint8_t>::knownValue(
        klemmenstatus, DiagnosticSource::CasStatusFzgZustand, casStart, casEnd);
    snapshot.engineRpm = DiagnosticSignal<float>::knownValue(
        rpm, DiagnosticSource::DdeStatusMotordrehzahl, ddeStart, ddeEnd);
    return snapshot;
}

const DocumentedSignalObservation* findObservedSignal(
    const bmw::remote::application::VehicleStartObservation& observation,
    const bmw::remote::domain::StartSignalId signal) {
    for (std::size_t index = 0U; index < observation.observedSignalCount; ++index) {
        if (observation.observedSignals[index].signal == signal) {
            return &observation.observedSignals[index];
        }
    }
    return nullptr;
}

void testKlemmenstatusDecoderPreservesDocumentedAndUnvalidatedStates() {
    const auto rest = bmw::remote::domain::decodeKlemmenstatus(0x40U);
    CHECK(rest.terminalR == TerminalPairState::Off);
    CHECK(rest.terminal15 == TerminalPairState::Off);
    CHECK(rest.terminal50 == TerminalPairState::Off);
    CHECK(rest.keyValidity == KeyValidityState::ValidKeyDetected);
    CHECK(rest.rawValueObservedOnTestVehicle);

    const auto key = bmw::remote::domain::decodeKlemmenstatus(0x41U);
    const auto terminalReady = bmw::remote::domain::decodeKlemmenstatus(0x45U);
    const auto cranking = bmw::remote::domain::decodeKlemmenstatus(0x55U);
    CHECK(key.terminalR == TerminalPairState::On);
    CHECK(terminalReady.terminal15 == TerminalPairState::On);
    CHECK(cranking.terminal50 == TerminalPairState::On);

    const auto invalidKl15 = bmw::remote::domain::decodeKlemmenstatus(0x4DU);
    CHECK(invalidKl15.terminal15 == TerminalPairState::Invalid);
    CHECK(!invalidKl15.rawValueObservedOnTestVehicle);
    const auto undocumentedKl15 = bmw::remote::domain::decodeKlemmenstatus(0x49U);
    CHECK(undocumentedKl15.terminal15 == TerminalPairState::UndocumentedEncoding);
    CHECK(!undocumentedKl15.allFieldsDocumented);
}

void testCasInhibitorsUseExactDocumentedValuesAndCannotBeBypassed() {
    CHECK(bmw::remote::domain::decodeKl15EnableInhibitor(1U) ==
          Kl15EnableInhibitor::CurrentKeyInvalid);
    CHECK(bmw::remote::domain::decodeKl15EnableInhibitor(16U) ==
          Kl15EnableInhibitor::KeyNotLatched);
    CHECK(bmw::remote::domain::decodeKl15DisableInhibitor(10U) ==
          Kl15DisableInhibitor::DriveEngagementDetected);
    CHECK(bmw::remote::domain::decodeKl50EnableInhibitor(5U) ==
          Kl50EnableInhibitor::BrakeNotPressed);
    CHECK(bmw::remote::domain::decodeKl50EnableInhibitor(6U) ==
          Kl50EnableInhibitor::BrakeImplausibleOrPressureInsufficient);
    CHECK(bmw::remote::domain::decodeKl50EnableInhibitor(9U) ==
          Kl50EnableInhibitor::DmeDdeAbortEngineRunningOrStartNotAllowed);
    CHECK(bmw::remote::domain::decodeKl50EnableInhibitor(10U) ==
          Kl50EnableInhibitor::ParkOrNeutralNotEngaged);
    CHECK(bmw::remote::domain::decodeKl50EnableInhibitor(11U) ==
          Kl50EnableInhibitor::UnknownDocumentedValue);

    StartSignalSnapshot snapshot{};
    snapshot.kl15EnableInhibitor = DiagnosticSignal<std::uint8_t>::knownValue(
        0U, DiagnosticSource::CasStatusKlemmenverhinderer, 10U, 20U);
    snapshot.kl15DisableInhibitor = DiagnosticSignal<std::uint8_t>::knownValue(
        0U, DiagnosticSource::CasStatusKlemmenverhinderer, 10U, 20U);
    snapshot.kl50EnableInhibitor = DiagnosticSignal<std::uint8_t>::knownValue(
        5U, DiagnosticSource::CasStatusKlemmenverhinderer, 10U, 20U);
    const auto conditions = bmw::remote::domain::observeCasStartConditions(snapshot);
    CHECK(conditions.canObserveStartConditions);
    CHECK(conditions.kl50Enable == Kl50EnableInhibitor::BrakeNotPressed);
}

void testObserverReconstructsOemStartSync01WithoutInventingMissingSignals() {
    VehicleStartObserver observer{};
    const auto ready = observedCasDdeSnapshot(0x45U, 0.0F, 100U, 152U, 153U, 356U);
    const auto crank = observedCasDdeSnapshot(0x55U, 131.0F, 357U, 409U, 410U, 616U);
    const auto rotating = observedCasDdeSnapshot(0x45U, 981.5F, 1'234U, 1'286U, 1'287U, 1'493U);
    const auto running = observedCasDdeSnapshot(0x45U, 916.0F, 1'494U, 1'545U, 1'546U, 1'749U);

    CHECK(observer.observe(ready).primaryVehicleState == VehicleStartObservedState::TerminalReady);
    CHECK(observer.observe(crank, &ready).primaryVehicleState == VehicleStartObservedState::CrankingObserved);
    CHECK(observer.observe(rotating, &crank).primaryVehicleState ==
          VehicleStartObservedState::EngineRotatingObserved);
    const auto result = observer.observe(running, &rotating);
    CHECK(result.primaryVehicleState == VehicleStartObservedState::EngineRunningObserved);
    CHECK(result.primaryStateQualification == StartObservationQualification::ConfirmedPhase3dSequence);
    CHECK(result.supportingSignalCount == 2U);
    CHECK(result.missingSignalCount == 11U);
    CHECK(result.supportingSignals[0U].interval.endedAtMs <
          result.supportingSignals[1U].interval.startedAtMs);
}

void testObserverReconstructsOemFullCycle01() {
    VehicleStartObserver observer{};
    const auto rest = observedCasDdeSnapshot(0x40U, 0.0F, 100U, 150U, 151U, 200U);
    const auto key = observedCasDdeSnapshot(0x41U, 0.0F, 201U, 250U, 251U, 300U);
    const auto crank = observedCasDdeSnapshot(0x55U, 130.0F, 301U, 350U, 351U, 400U);
    const auto rotating = observedCasDdeSnapshot(0x45U, 962.0F, 401U, 450U, 451U, 500U);
    const auto running = observedCasDdeSnapshot(0x45U, 778.5F, 501U, 550U, 551U, 600U);
    const auto shutdown = observedCasDdeSnapshot(0x40U, 779.5F, 601U, 650U, 651U, 700U);
    const auto rundown = observedCasDdeSnapshot(0x40U, 215.0F, 701U, 750U, 751U, 800U);
    const auto stopped = observedCasDdeSnapshot(0x40U, 0.0F, 801U, 850U, 851U, 900U);

    CHECK(observer.observe(rest).primaryVehicleState == VehicleStartObservedState::VehicleRest);
    CHECK(observer.observe(key, &rest).primaryVehicleState == VehicleStartObservedState::KeyPresent);
    CHECK(observer.observe(crank, &key).primaryVehicleState == VehicleStartObservedState::CrankingObserved);
    CHECK(observer.observe(rotating, &crank).primaryVehicleState == VehicleStartObservedState::EngineRotatingObserved);
    CHECK(observer.observe(running, &rotating).primaryVehicleState == VehicleStartObservedState::EngineRunningObserved);
    CHECK(observer.observe(shutdown, &running).primaryVehicleState == VehicleStartObservedState::ShutdownObserved);
    CHECK(observer.observe(stopped, &rundown).primaryVehicleState == VehicleStartObservedState::EngineStoppedObserved);
}

void testObserverFailsUnknownOnCommunicationAndInvalidKlemmenstatus() {
    VehicleStartObserver observer{};
    auto communicationError = observedCasDdeSnapshot(0x45U, 0.0F, 1U, 2U, 3U, 4U);
    communicationError.klemmenstatus =
        DiagnosticSignal<std::uint8_t>::unavailable(DiagnosticAvailability::CommunicationError);
    CHECK(observer.observe(communicationError).primaryVehicleState == VehicleStartObservedState::Unknown);

    const auto invalid = observedCasDdeSnapshot(0x4DU, 0.0F, 1U, 2U, 3U, 4U);
    CHECK(observer.observe(invalid).primaryVehicleState == VehicleStartObservedState::Unknown);
}

void testSstActivityDoesNotDisappearBehindPrimaryKeyState() {
    VehicleStartObserver observer{};
    auto snapshot = observedCasDdeSnapshot(0x41U, 0.0F, 10U, 20U, 21U, 30U);
    snapshot.sstA = DiagnosticSignal<bool>::knownValue(
        true, DiagnosticSource::CasStatusDiagnose, 31U, 40U);
    snapshot.sstB = DiagnosticSignal<bool>::knownValue(
        true, DiagnosticSource::CasStatusDiagnose, 31U, 40U);

    const auto result = observer.observe(snapshot);
    CHECK(result.primaryVehicleState == VehicleStartObservedState::KeyPresent);
    const auto* sstA = findObservedSignal(result, bmw::remote::domain::StartSignalId::SstA);
    const auto* sstB = findObservedSignal(result, bmw::remote::domain::StartSignalId::SstB);
    CHECK(sstA != nullptr);
    CHECK(sstB != nullptr);
    CHECK(sstA != nullptr && sstA->rawValue);
    CHECK(sstB != nullptr && sstB->rawValue);
    CHECK(sstA != nullptr && sstA->qualification ==
          StartObservationQualification::DocumentedNotVehicleValidated);
    CHECK(sstB != nullptr && sstB->qualification ==
          StartObservationQualification::DocumentedNotVehicleValidated);
    CHECK(sstA != nullptr && sstA->evidence.source ==
          DiagnosticSource::CasStatusDiagnose);
}

void testStartDmeActivityDoesNotReplaceCrankingPrimaryState() {
    VehicleStartObserver observer{};
    auto snapshot = observedCasDdeSnapshot(0x55U, 131.0F, 10U, 20U, 21U, 30U);
    snapshot.startDme = DiagnosticSignal<bool>::knownValue(
        true, DiagnosticSource::CasStatusDiagnose, 31U, 40U);

    const auto result = observer.observe(snapshot);
    CHECK(result.primaryVehicleState == VehicleStartObservedState::CrankingObserved);
    const auto* startDme =
        findObservedSignal(result, bmw::remote::domain::StartSignalId::StartDme);
    CHECK(startDme != nullptr);
    CHECK(startDme != nullptr && startDme->rawValue);
    CHECK(startDme != nullptr && startDme->qualification ==
          StartObservationQualification::DocumentedNotVehicleValidated);
}

void testStartReleaseRawValuesNeverBecomeStartAllowedState() {
    VehicleStartObserver observer{};
    for (const bool rawValue : {false, true}) {
        StartSignalSnapshot snapshot{};
        snapshot.startRelease = DiagnosticSignal<bool>::knownValue(
            rawValue, DiagnosticSource::CasStatusDiagnose, 10U, 20U);
        const auto result = observer.observe(snapshot);
        CHECK(result.primaryVehicleState == VehicleStartObservedState::Unknown);
        CHECK(result.primaryStateQualification == StartObservationQualification::Unknown);
        const auto* startRelease =
            findObservedSignal(result, bmw::remote::domain::StartSignalId::StartRelease);
        CHECK(startRelease != nullptr);
        CHECK(startRelease != nullptr && startRelease->rawValue == rawValue);
        CHECK(startRelease != nullptr && startRelease->qualification ==
              StartObservationQualification::DocumentedNotVehicleValidated);
    }
}

void testMfsActivityRemainsUnvalidatedRawObservation() {
    VehicleStartObserver observer{};
    StartSignalSnapshot snapshot{};
    snapshot.mfs = DiagnosticSignal<bool>::knownValue(
        true, DiagnosticSource::CasStatusDiagnose, 10U, 20U);

    const auto result = observer.observe(snapshot);
    CHECK(result.primaryVehicleState == VehicleStartObservedState::Unknown);
    const auto* mfs = findObservedSignal(result, bmw::remote::domain::StartSignalId::Mfs);
    CHECK(mfs != nullptr);
    CHECK(mfs != nullptr && mfs->rawValue);
    CHECK(mfs != nullptr && mfs->qualification ==
          StartObservationQualification::DocumentedNotVehicleValidated);
}
