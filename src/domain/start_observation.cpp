#include "bmw_remote/domain/start_observation.hpp"

namespace bmw::remote::domain {
namespace {

TerminalPairState decodeTerminalPair(const std::uint8_t value) {
    switch (value) {
        case 0U:
            return TerminalPairState::Off;
        case 1U:
            return TerminalPairState::On;
        case 3U:
            return TerminalPairState::Invalid;
        default:
            return TerminalPairState::UndocumentedEncoding;
    }
}

KeyValidityState decodeKeyValidity(const std::uint8_t value) {
    switch (value) {
        case 0U:
            return KeyValidityState::NoKeyDetected;
        case 1U:
            return KeyValidityState::ValidKeyDetected;
        case 2U:
            return KeyValidityState::InvalidOrMissingKeyDetected;
        default:
            return KeyValidityState::UndocumentedEncoding;
    }
}

}  // namespace

bool AcquisitionInterval::valid() const {
    return captured && startedAtMs <= endedAtMs;
}

KlemmenstatusDecoded decodeKlemmenstatus(const std::uint8_t raw) {
    KlemmenstatusDecoded decoded{};
    decoded.raw = raw;
    decoded.terminalR = decodeTerminalPair(static_cast<std::uint8_t>(raw & 0x03U));
    decoded.terminal15 = decodeTerminalPair(static_cast<std::uint8_t>((raw >> 2U) & 0x03U));
    decoded.terminal50 = decodeTerminalPair(static_cast<std::uint8_t>((raw >> 4U) & 0x03U));
    decoded.keyValidity = decodeKeyValidity(static_cast<std::uint8_t>((raw >> 6U) & 0x03U));
    decoded.rawValueObservedOnTestVehicle =
        raw == 0x40U || raw == 0x41U || raw == 0x45U || raw == 0x55U;
    decoded.allFieldsDocumented =
        decoded.terminalR != TerminalPairState::UndocumentedEncoding &&
        decoded.terminal15 != TerminalPairState::UndocumentedEncoding &&
        decoded.terminal50 != TerminalPairState::UndocumentedEncoding &&
        decoded.keyValidity != KeyValidityState::UndocumentedEncoding;
    return decoded;
}

Kl15EnableInhibitor decodeKl15EnableInhibitor(const std::uint8_t raw) {
    switch (raw) {
        case 0U: return Kl15EnableInhibitor::NoBlocker;
        case 1U: return Kl15EnableInhibitor::CurrentKeyInvalid;
        case 14U: return Kl15EnableInhibitor::ElvControlOrQueryError;
        case 15U: return Kl15EnableInhibitor::ElvErrorCounterMaximum;
        case 16U: return Kl15EnableInhibitor::KeyNotLatched;
        default: return Kl15EnableInhibitor::UnknownDocumentedValue;
    }
}

Kl15DisableInhibitor decodeKl15DisableInhibitor(const std::uint8_t raw) {
    switch (raw) {
        case 0U: return Kl15DisableInhibitor::NoBlocker;
        case 3U: return Kl15DisableInhibitor::VehicleSpeedDetected;
        case 4U: return Kl15DisableInhibitor::VehicleSpeedImplausible;
        case 10U: return Kl15DisableInhibitor::DriveEngagementDetected;
        default: return Kl15DisableInhibitor::UnknownDocumentedValue;
    }
}

Kl50EnableInhibitor decodeKl50EnableInhibitor(const std::uint8_t raw) {
    switch (raw) {
        case 0U: return Kl50EnableInhibitor::NoBlocker;
        case 1U: return Kl50EnableInhibitor::NoValidKey;
        case 5U: return Kl50EnableInhibitor::BrakeNotPressed;
        case 6U: return Kl50EnableInhibitor::BrakeImplausibleOrPressureInsufficient;
        case 7U: return Kl50EnableInhibitor::ClutchNotPressed;
        case 8U: return Kl50EnableInhibitor::ClutchImplausible;
        case 9U: return Kl50EnableInhibitor::DmeDdeAbortEngineRunningOrStartNotAllowed;
        case 10U: return Kl50EnableInhibitor::ParkOrNeutralNotEngaged;
        case 12U: return Kl50EnableInhibitor::TransponderNotLocked;
        case 13U: return Kl50EnableInhibitor::Kl50AssemblyMode;
        case 14U: return Kl50EnableInhibitor::ElvControlOrQueryError;
        case 15U: return Kl50EnableInhibitor::ElvErrorCounterMaximum;
        case 16U: return Kl50EnableInhibitor::KeyNotLatched;
        default: return Kl50EnableInhibitor::UnknownDocumentedValue;
    }
}

CasStartConditions observeCasStartConditions(const StartSignalSnapshot& snapshot) {
    CasStartConditions conditions{};
    conditions.canObserveStartConditions =
        snapshot.kl15EnableInhibitor.known() &&
        snapshot.kl15DisableInhibitor.known() &&
        snapshot.kl50EnableInhibitor.known();
    if (snapshot.kl15EnableInhibitor.known()) {
        conditions.kl15Enable = decodeKl15EnableInhibitor(snapshot.kl15EnableInhibitor.value);
    }
    if (snapshot.kl15DisableInhibitor.known()) {
        conditions.kl15Disable = decodeKl15DisableInhibitor(snapshot.kl15DisableInhibitor.value);
    }
    if (snapshot.kl50EnableInhibitor.known()) {
        conditions.kl50Enable = decodeKl50EnableInhibitor(snapshot.kl50EnableInhibitor.value);
    }
    return conditions;
}

}  // namespace bmw::remote::domain
