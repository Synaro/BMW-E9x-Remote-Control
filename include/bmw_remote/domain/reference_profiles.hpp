#pragma once

#include "bmw_remote/domain/vehicle_profile.hpp"

namespace bmw::remote::domain::profiles {

[[nodiscard]] const VehicleProfile& e90_2009_n47d20c_automatic() noexcept;
[[nodiscard]] const VehicleProfileRegistry& registry() noexcept;

}  // namespace bmw::remote::domain::profiles
