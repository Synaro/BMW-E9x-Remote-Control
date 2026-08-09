#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "can_core/can_receiver.hpp"

namespace can_core {

inline constexpr std::uint16_t CaptureFormatVersion = 2U;
inline constexpr std::string_view CaptureV2CsvHeader =
    "timestamp_us,sequence,channel,bitrate,id,extended,dlc,data,direction,status";

template <std::size_t Size>
[[nodiscard]] constexpr bool hasText(
    const std::array<char, Size>& value) noexcept {
    return Size > 1U && value[0U] != '\0' && value[Size - 1U] == '\0';
}

struct CaptureSessionManifest final {
    static constexpr std::size_t TextCapacity = 64U;

    std::uint16_t formatVersion{CaptureFormatVersion};
    std::array<char, TextCapacity> sessionId{};
    std::array<char, TextCapacity> firmwareVersion{};
    std::array<char, TextCapacity> interfaceName{};
    std::array<char, TextCapacity> configurationId{};
    std::array<char, TextCapacity> vehicleInformation{};
    std::uint32_t bitrate{0U};
    bool listenOnly{true};
    CanReceiveStatistics statistics{};

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return formatVersion == CaptureFormatVersion &&
               hasText(sessionId) &&
               hasText(firmwareVersion) &&
               hasText(interfaceName) &&
               hasText(configurationId) &&
               (bitrate == bitrateValue(CanBitrate::Kbit100) ||
                bitrate == bitrateValue(CanBitrate::Kbit500)) &&
               listenOnly;
    }
};

}  // namespace can_core
