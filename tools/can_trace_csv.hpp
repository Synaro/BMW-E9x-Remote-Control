#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include "bmw_remote/infrastructure/can_frame.hpp"

namespace bmw::remote::host {

[[nodiscard]] bool parseCanonicalCanTrace(
    std::istream& input,
    std::vector<infrastructure::CanFrame>& frames,
    std::size_t maximumFrames,
    std::string& error);

[[nodiscard]] bool loadCanonicalCanTrace(
    const char* path,
    std::vector<infrastructure::CanFrame>& frames,
    std::size_t maximumFrames,
    std::string& error);

}  // namespace bmw::remote::host
