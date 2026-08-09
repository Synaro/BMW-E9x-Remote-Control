using bmw::remote::application::StartObservationQualification;
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

    CHECK(observer.observe(ready).state == VehicleStartObservedState::TerminalReady);
    CHECK(observer.observe(crank, &ready).state == VehicleStartObservedState::CrankingObserved);
    CHECK(observer.observe(rotating, &crank).state ==
          VehicleStartObservedState::EngineRotatingObserved);
    const auto result = observer.observe(running, &rotating);
    CHECK(result.state == VehicleStartObservedState::EngineRunningObserved);
    CHECK(result.qualification == StartObservationQualification::ConfirmedPhase3dSequence);
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

    CHECK(observer.observe(rest).state == VehicleStartObservedState::VehicleRest);
    CHECK(observer.observe(key, &rest).state == VehicleStartObservedState::KeyPresent);
    CHECK(observer.observe(crank, &key).state == VehicleStartObservedState::CrankingObserved);
    CHECK(observer.observe(rotating, &crank).state == VehicleStartObservedState::EngineRotatingObserved);
    CHECK(observer.observe(running, &rotating).state == VehicleStartObservedState::EngineRunningObserved);
    CHECK(observer.observe(shutdown, &running).state == VehicleStartObservedState::ShutdownObserved);
    CHECK(observer.observe(stopped, &rundown).state == VehicleStartObservedState::EngineStoppedObserved);
}

void testObserverFailsUnknownOnCommunicationAndInvalidKlemmenstatus() {
    VehicleStartObserver observer{};
    auto communicationError = observedCasDdeSnapshot(0x45U, 0.0F, 1U, 2U, 3U, 4U);
    communicationError.klemmenstatus =
        DiagnosticSignal<std::uint8_t>::unavailable(DiagnosticAvailability::CommunicationError);
    CHECK(observer.observe(communicationError).state == VehicleStartObservedState::Unknown);

    const auto invalid = observedCasDdeSnapshot(0x4DU, 0.0F, 1U, 2U, 3U, 4U);
    CHECK(observer.observe(invalid).state == VehicleStartObservedState::Unknown);
}

void testDocumentedCasSignalsRemainObservationsOnly() {
    VehicleStartObserver observer{};
    StartSignalSnapshot snapshot{};
    snapshot.startRelease = DiagnosticSignal<bool>::knownValue(
        true, DiagnosticSource::CasStatusDiagnose, 10U, 20U);
    auto result = observer.observe(snapshot);
    CHECK(result.state == VehicleStartObservedState::StartAllowedObserved);
    CHECK(result.qualification ==
          StartObservationQualification::DocumentedSignalNotVehicleValidated);

    snapshot = StartSignalSnapshot{};
    snapshot.sstA = DiagnosticSignal<bool>::knownValue(
        true, DiagnosticSource::CasStatusDiagnose, 30U, 40U);
    snapshot.sstB = DiagnosticSignal<bool>::knownValue(
        true, DiagnosticSource::CasStatusDiagnose, 30U, 40U);
    result = observer.observe(snapshot);
    CHECK(result.state == VehicleStartObservedState::StartRequestObserved);
    CHECK(result.qualification ==
          StartObservationQualification::DocumentedSignalNotVehicleValidated);
}
