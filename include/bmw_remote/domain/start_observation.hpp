#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace bmw::remote::domain {

enum class DiagnosticAvailability : std::uint8_t {
    Known,
    Unknown,
    CommunicationError,
    NotCaptured,
};

enum class DiagnosticSource : std::uint8_t {
    None,
    CasStatusDiagnose,
    CasStatusFzgZustand,
    CasStatusKlemmenverhinderer,
    DdeStatusMotordrehzahl,
};

struct AcquisitionInterval {
    std::uint64_t startedAtMs{0U};
    std::uint64_t endedAtMs{0U};
    bool captured{false};

    bool valid() const;
};

template <typename T>
struct DiagnosticSignal {
    DiagnosticAvailability availability{DiagnosticAvailability::NotCaptured};
    T value{};
    AcquisitionInterval interval{};
    DiagnosticSource source{DiagnosticSource::None};

    bool known() const {
        return availability == DiagnosticAvailability::Known && interval.valid() &&
               source != DiagnosticSource::None;
    }

    static DiagnosticSignal knownValue(
        const T value,
        const DiagnosticSource source,
        const std::uint64_t startedAtMs,
        const std::uint64_t endedAtMs) {
        return DiagnosticSignal{
            DiagnosticAvailability::Known,
            value,
            AcquisitionInterval{startedAtMs, endedAtMs, true},
            source};
    }

    static DiagnosticSignal unavailable(const DiagnosticAvailability availability) {
        return DiagnosticSignal{availability, T{}, AcquisitionInterval{}, DiagnosticSource::None};
    }
};

enum class TerminalPairState : std::uint8_t {
    Off,
    On,
    UndocumentedEncoding,
    Invalid,
};

enum class KeyValidityState : std::uint8_t {
    NoKeyDetected,
    ValidKeyDetected,
    InvalidOrMissingKeyDetected,
    UndocumentedEncoding,
};

struct KlemmenstatusDecoded {
    std::uint8_t raw{0U};
    TerminalPairState terminalR{TerminalPairState::Off};
    TerminalPairState terminal15{TerminalPairState::Off};
    TerminalPairState terminal50{TerminalPairState::Off};
    KeyValidityState keyValidity{KeyValidityState::NoKeyDetected};
    bool rawValueObservedOnTestVehicle{false};
    bool allFieldsDocumented{false};
};

KlemmenstatusDecoded decodeKlemmenstatus(std::uint8_t raw);

enum class Kl15EnableInhibitor : std::uint8_t {
    NoBlocker,
    CurrentKeyInvalid,
    ElvControlOrQueryError,
    ElvErrorCounterMaximum,
    KeyNotLatched,
    UnknownDocumentedValue,
};

enum class Kl15DisableInhibitor : std::uint8_t {
    NoBlocker,
    VehicleSpeedDetected,
    VehicleSpeedImplausible,
    DriveEngagementDetected,
    UnknownDocumentedValue,
};

enum class Kl50EnableInhibitor : std::uint8_t {
    NoBlocker,
    NoValidKey,
    BrakeNotPressed,
    BrakeImplausibleOrPressureInsufficient,
    ClutchNotPressed,
    ClutchImplausible,
    DmeDdeAbortEngineRunningOrStartNotAllowed,
    ParkOrNeutralNotEngaged,
    TransponderNotLocked,
    Kl50AssemblyMode,
    ElvControlOrQueryError,
    ElvErrorCounterMaximum,
    KeyNotLatched,
    UnknownDocumentedValue,
};

Kl15EnableInhibitor decodeKl15EnableInhibitor(std::uint8_t raw);
Kl15DisableInhibitor decodeKl15DisableInhibitor(std::uint8_t raw);
Kl50EnableInhibitor decodeKl50EnableInhibitor(std::uint8_t raw);

enum class StartSignalId : std::uint8_t {
    SstA,
    SstB,
    KeyRast,
    Brake,
    ParkNeutralOrClutch,
    Mfs,
    StartDme,
    StartRelease,
    Klemmenstatus,
    Kl15EnableInhibitor,
    Kl15DisableInhibitor,
    Kl50EnableInhibitor,
    EngineRpm,
    Count,
};

struct StartSignalSnapshot {
    DiagnosticSignal<bool> sstA{};
    DiagnosticSignal<bool> sstB{};
    DiagnosticSignal<bool> keyRast{};
    DiagnosticSignal<bool> brake{};
    DiagnosticSignal<bool> parkNeutralOrClutch{};
    DiagnosticSignal<bool> mfs{};
    DiagnosticSignal<bool> startDme{};
    DiagnosticSignal<bool> startRelease{};
    DiagnosticSignal<std::uint8_t> klemmenstatus{};
    DiagnosticSignal<std::uint8_t> kl15EnableInhibitor{};
    DiagnosticSignal<std::uint8_t> kl15DisableInhibitor{};
    DiagnosticSignal<std::uint8_t> kl50EnableInhibitor{};
    DiagnosticSignal<float> engineRpm{};
};

struct CasStartConditions {
    bool canObserveStartConditions{false};
    Kl15EnableInhibitor kl15Enable{Kl15EnableInhibitor::UnknownDocumentedValue};
    Kl15DisableInhibitor kl15Disable{Kl15DisableInhibitor::UnknownDocumentedValue};
    Kl50EnableInhibitor kl50Enable{Kl50EnableInhibitor::UnknownDocumentedValue};
};

CasStartConditions observeCasStartConditions(const StartSignalSnapshot& snapshot);

}  // namespace bmw::remote::domain
