#include "bmw_remote/infrastructure/vehicle_state_assembler.hpp"

namespace bmw::remote::infrastructure {

bool DecodedSignalBatch::add(
    const domain::VehicleSignal signal,
    const std::uint32_t value) noexcept {
    if (count >= signals.size()) {
        return false;
    }

    signals[count] = DecodedSignal{signal, value};
    ++count;
    return true;
}

VehicleStateAssembler::VehicleStateAssembler(
    const CanFrameDecoder& decoder,
    const SignalFreshnessConfig freshness) noexcept
    : decoder_(decoder), freshness_(freshness) {}

bool VehicleStateAssembler::consume(const CanFrame& frame) noexcept {
    ++statistics_.consumedFrames;

    if (!frame.isValid()) {
        ++statistics_.rejectedFrames;
        return false;
    }

    DecodedSignalBatch batch{};
    const DecodeResult result = decoder_.decode(frame, batch);

    if (result == DecodeResult::Ignored) {
        ++statistics_.ignoredFrames;
        return true;
    }

    if (result == DecodeResult::Invalid || batch.count == 0U || !validateBatch(batch)) {
        ++statistics_.rejectedFrames;
        return false;
    }

    applyBatch(batch, frame.timestampMs);
    ++statistics_.decodedFrames;
    statistics_.decodedSignals += static_cast<std::uint32_t>(batch.count);
    return true;
}

domain::VehicleState VehicleStateAssembler::snapshot(const std::uint32_t nowMs) const noexcept {
    domain::VehicleState vehicle{};
    vehicle.batteryMillivolts = observe(
        batteryMillivolts_, nowMs, freshness_.batteryMaximumAgeMs);
    vehicle.engineRpm = observe(engineRpm_, nowMs, freshness_.engineRpmMaximumAgeMs);
    vehicle.hoodClosed = observe(hoodClosed_, nowMs, freshness_.bodyMaximumAgeMs);
    vehicle.doorsClosed = observe(doorsClosed_, nowMs, freshness_.bodyMaximumAgeMs);
    vehicle.trunkClosed = observe(trunkClosed_, nowMs, freshness_.bodyMaximumAgeMs);
    vehicle.brakePressed = observe(brakePressed_, nowMs, freshness_.bodyMaximumAgeMs);
    vehicle.parkingBrakeApplied = observe(
        parkingBrakeApplied_, nowMs, freshness_.bodyMaximumAgeMs);
    vehicle.transmission = observe(
        transmission_, nowMs, freshness_.transmissionMaximumAgeMs);
    vehicle.gear = observe(gear_, nowMs, freshness_.transmissionMaximumAgeMs);
    vehicle.criticalFaultPresent = observe(
        criticalFaultPresent_, nowMs, freshness_.faultMaximumAgeMs);
    return vehicle;
}

void VehicleStateAssembler::reset() noexcept {
    statistics_ = {};
    batteryMillivolts_ = {};
    engineRpm_ = {};
    hoodClosed_ = {};
    doorsClosed_ = {};
    trunkClosed_ = {};
    brakePressed_ = {};
    parkingBrakeApplied_ = {};
    transmission_ = {};
    gear_ = {};
    criticalFaultPresent_ = {};
}

bool VehicleStateAssembler::validateBatch(const DecodedSignalBatch& batch) const noexcept {
    if (batch.count > batch.signals.size()) {
        return false;
    }

    for (std::size_t index = 0U; index < batch.count; ++index) {
        if (!validateSignal(batch.signals[index])) {
            return false;
        }

        for (std::size_t previous = 0U; previous < index; ++previous) {
            if (batch.signals[previous].signal == batch.signals[index].signal) {
                return false;
            }
        }
    }
    return true;
}

bool VehicleStateAssembler::validateSignal(const DecodedSignal& signal) const noexcept {
    switch (signal.signal) {
        case domain::VehicleSignal::BatteryMillivolts:
            return signal.value <= 20'000U;

        case domain::VehicleSignal::EngineRpm:
            return signal.value <= 10'000U;

        case domain::VehicleSignal::HoodClosed:
        case domain::VehicleSignal::DoorsClosed:
        case domain::VehicleSignal::TrunkClosed:
        case domain::VehicleSignal::BrakePressed:
        case domain::VehicleSignal::ParkingBrakeApplied:
        case domain::VehicleSignal::CriticalFaultPresent:
            return signal.value <= 1U;

        case domain::VehicleSignal::Transmission:
            return signal.value <= static_cast<std::uint32_t>(domain::Transmission::Manual);

        case domain::VehicleSignal::Gear:
            return signal.value <= static_cast<std::uint32_t>(domain::Gear::Drive);

        case domain::VehicleSignal::Count:
            return false;
    }
    return false;
}

void VehicleStateAssembler::applyBatch(
    const DecodedSignalBatch& batch,
    const std::uint32_t timestampMs) noexcept {
    for (std::size_t index = 0U; index < batch.count; ++index) {
        applySignal(batch.signals[index], timestampMs);
    }
}

void VehicleStateAssembler::applySignal(
    const DecodedSignal& signal,
    const std::uint32_t timestampMs) noexcept {
    switch (signal.signal) {
        case domain::VehicleSignal::BatteryMillivolts:
            batteryMillivolts_ = {
                static_cast<std::uint16_t>(signal.value), timestampMs, true};
            break;

        case domain::VehicleSignal::EngineRpm:
            engineRpm_ = {static_cast<std::uint16_t>(signal.value), timestampMs, true};
            break;

        case domain::VehicleSignal::HoodClosed:
            hoodClosed_ = {signal.value != 0U, timestampMs, true};
            break;

        case domain::VehicleSignal::DoorsClosed:
            doorsClosed_ = {signal.value != 0U, timestampMs, true};
            break;

        case domain::VehicleSignal::TrunkClosed:
            trunkClosed_ = {signal.value != 0U, timestampMs, true};
            break;

        case domain::VehicleSignal::BrakePressed:
            brakePressed_ = {signal.value != 0U, timestampMs, true};
            break;

        case domain::VehicleSignal::ParkingBrakeApplied:
            parkingBrakeApplied_ = {signal.value != 0U, timestampMs, true};
            break;

        case domain::VehicleSignal::Transmission:
            transmission_ = {
                static_cast<domain::Transmission>(signal.value), timestampMs, true};
            break;

        case domain::VehicleSignal::Gear:
            gear_ = {static_cast<domain::Gear>(signal.value), timestampMs, true};
            break;

        case domain::VehicleSignal::CriticalFaultPresent:
            criticalFaultPresent_ = {signal.value != 0U, timestampMs, true};
            break;

        case domain::VehicleSignal::Count:
            break;
    }
}

}  // namespace bmw::remote::infrastructure
