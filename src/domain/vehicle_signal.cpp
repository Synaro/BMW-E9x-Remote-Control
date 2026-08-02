#include "bmw_remote/domain/vehicle_signal.hpp"

namespace bmw::remote::domain {

const char* toString(const VehicleSignal signal) noexcept {
    switch (signal) {
        case VehicleSignal::BatteryMillivolts: return "battery_millivolts";
        case VehicleSignal::EngineRpm: return "engine_rpm";
        case VehicleSignal::HoodClosed: return "hood_closed";
        case VehicleSignal::DoorsClosed: return "doors_closed";
        case VehicleSignal::TrunkClosed: return "trunk_closed";
        case VehicleSignal::BrakePressed: return "brake_pressed";
        case VehicleSignal::ParkingBrakeApplied: return "parking_brake_applied";
        case VehicleSignal::Transmission: return "transmission";
        case VehicleSignal::Gear: return "gear";
        case VehicleSignal::CriticalFaultPresent: return "critical_fault_present";
        case VehicleSignal::Count: break;
    }
    return "unknown";
}

}  // namespace bmw::remote::domain
