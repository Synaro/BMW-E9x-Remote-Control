#pragma once

#include <cstddef>
#include <cstdint>

namespace bmw::remote::infrastructure {

[[nodiscard]] std::uint32_t crc32(
    const std::uint8_t* data,
    std::size_t size) noexcept;

}  // namespace bmw::remote::infrastructure
