#include "bmw_remote/simulation/synthetic_can.hpp"

#include <array>

namespace bmw::remote::simulation {
namespace {

constexpr std::array<domain::SignalSupport, domain::vehicleSignalCount()> VerifiedSignals = {
    domain::SignalSupport::Verified,
    domain::SignalSupport::Verified,
    domain::SignalSupport::Verified,
    domain::SignalSupport::Verified,
    domain::SignalSupport::Verified,
    domain::SignalSupport::Verified,
    domain::SignalSupport::Verified,
    domain::SignalSupport::Verified,
    domain::SignalSupport::Verified,
    domain::SignalSupport::Verified,
};

constexpr domain::VehicleProfile SyntheticProfile = {
    "synthetic-offline-automatic",
    "Synthetic offline automatic vehicle",
    domain::VehiclePlatform::Unknown,
    domain::BodyVariant::Unknown,
    0U,
    0U,
    "synthetic",
    domain::FuelType::Unknown,
    domain::Transmission::Automatic,
    domain::QualificationStage::BenchValidated,
    VerifiedSignals,
};

[[nodiscard]] constexpr std::uint16_t decodeLittleEndian16(
    const std::uint8_t low,
    const std::uint8_t high) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(low) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(high) << 8U));
}

void encodeLittleEndian16(
    const std::uint16_t value,
    std::uint8_t& low,
    std::uint8_t& high) noexcept {
    low = static_cast<std::uint8_t>(value & 0xFFU);
    high = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

[[nodiscard]] bool addPowertrainSignals(
    const infrastructure::CanFrame& frame,
    infrastructure::DecodedSignalBatch& output) noexcept {
    using domain::VehicleSignal;

    const std::uint16_t engineRpm = decodeLittleEndian16(frame.data[0], frame.data[1]);
    const std::uint16_t batteryMillivolts =
        decodeLittleEndian16(frame.data[2], frame.data[3]);

    return output.add(VehicleSignal::EngineRpm, engineRpm) &&
           output.add(VehicleSignal::BatteryMillivolts, batteryMillivolts) &&
           output.add(VehicleSignal::Transmission, frame.data[4]) &&
           output.add(VehicleSignal::Gear, frame.data[5]) &&
           output.add(VehicleSignal::CriticalFaultPresent, frame.data[6]);
}

[[nodiscard]] bool addBodySignals(
    const infrastructure::CanFrame& frame,
    infrastructure::DecodedSignalBatch& output) noexcept {
    using domain::VehicleSignal;
    const std::uint8_t flags = frame.data[0];

    return output.add(VehicleSignal::HoodClosed, (flags >> 0U) & 0x01U) &&
           output.add(VehicleSignal::DoorsClosed, (flags >> 1U) & 0x01U) &&
           output.add(VehicleSignal::TrunkClosed, (flags >> 2U) & 0x01U) &&
           output.add(VehicleSignal::BrakePressed, (flags >> 3U) & 0x01U) &&
           output.add(VehicleSignal::ParkingBrakeApplied, (flags >> 4U) & 0x01U);
}

}  // namespace

const domain::VehicleProfile& syntheticVehicleProfile() noexcept {
    return SyntheticProfile;
}

infrastructure::DecodeResult SyntheticCanDecoder::decode(
    const infrastructure::CanFrame& frame,
    infrastructure::DecodedSignalBatch& output) const noexcept {
    using infrastructure::DecodeResult;

    if (frame.identifier != SyntheticCanProtocol::PowertrainFrameIdentifier &&
        frame.identifier != SyntheticCanProtocol::BodyFrameIdentifier) {
        return DecodeResult::Ignored;
    }

    if (!frame.extended ||
        frame.dataLength != infrastructure::CanFrame::MaximumDataLength ||
        frame.data[7] != SyntheticCanProtocol::Signature) {
        return DecodeResult::Invalid;
    }

    if (frame.identifier == SyntheticCanProtocol::PowertrainFrameIdentifier) {
        return addPowertrainSignals(frame, output)
                   ? DecodeResult::Decoded
                   : DecodeResult::Invalid;
    }

    for (std::size_t index = 1U; index < 7U; ++index) {
        if (frame.data[index] != 0U) {
            return DecodeResult::Invalid;
        }
    }

    if ((frame.data[0] & 0xE0U) != 0U) {
        return DecodeResult::Invalid;
    }

    return addBodySignals(frame, output) ? DecodeResult::Decoded : DecodeResult::Invalid;
}

infrastructure::CanFrame makeSyntheticPowertrainFrame(
    const std::uint32_t timestampMs,
    const SyntheticPowertrainState state) noexcept {
    infrastructure::CanFrame frame{};
    frame.timestampMs = timestampMs;
    frame.identifier = SyntheticCanProtocol::PowertrainFrameIdentifier;
    frame.extended = true;
    frame.dataLength = infrastructure::CanFrame::MaximumDataLength;
    encodeLittleEndian16(state.engineRpm, frame.data[0], frame.data[1]);
    encodeLittleEndian16(
        state.batteryMillivolts, frame.data[2], frame.data[3]);
    frame.data[4] = static_cast<std::uint8_t>(state.transmission);
    frame.data[5] = static_cast<std::uint8_t>(state.gear);
    frame.data[6] = state.criticalFaultPresent ? 1U : 0U;
    frame.data[7] = SyntheticCanProtocol::Signature;
    return frame;
}

infrastructure::CanFrame makeSyntheticBodyFrame(
    const std::uint32_t timestampMs,
    const SyntheticBodyState state) noexcept {
    infrastructure::CanFrame frame{};
    frame.timestampMs = timestampMs;
    frame.identifier = SyntheticCanProtocol::BodyFrameIdentifier;
    frame.extended = true;
    frame.dataLength = infrastructure::CanFrame::MaximumDataLength;
    frame.data[0] = static_cast<std::uint8_t>(
        (state.hoodClosed ? 1U << 0U : 0U) |
        (state.doorsClosed ? 1U << 1U : 0U) |
        (state.trunkClosed ? 1U << 2U : 0U) |
        (state.brakePressed ? 1U << 3U : 0U) |
        (state.parkingBrakeApplied ? 1U << 4U : 0U));
    frame.data[7] = SyntheticCanProtocol::Signature;
    return frame;
}

}  // namespace bmw::remote::simulation
