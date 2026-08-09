#pragma once

#include <array>
#include <cstdint>

namespace can_core {

enum class CanDirection : std::uint8_t {
    Rx,
};

enum class CanFrameStatus : std::uint16_t {
    None = 0U,
    RemoteFrame = 1U << 0U,
    DlcNonCompliant = 1U << 1U,
    ErrorWarning = 1U << 2U,
    ErrorPassive = 1U << 3U,
    BusOff = 1U << 4U,
    PeripheralReset = 1U << 5U,
};

[[nodiscard]] constexpr CanFrameStatus operator|(
    const CanFrameStatus left,
    const CanFrameStatus right) noexcept {
    return static_cast<CanFrameStatus>(
        static_cast<std::uint16_t>(left) |
        static_cast<std::uint16_t>(right));
}

[[nodiscard]] constexpr bool hasStatus(
    const CanFrameStatus value,
    const CanFrameStatus flag) noexcept {
    return (static_cast<std::uint16_t>(value) &
            static_cast<std::uint16_t>(flag)) != 0U;
}

struct CanFrame final {
    static constexpr std::uint32_t MaximumStandardIdentifier = 0x7FFU;
    static constexpr std::uint32_t MaximumExtendedIdentifier = 0x1FFFFFFFU;
    static constexpr std::uint8_t MaximumDataLength = 8U;

    std::uint64_t timestampUs{0U};
    std::uint64_t sequence{0U};
    std::uint32_t bitrate{0U};
    std::uint32_t identifier{0U};
    std::uint8_t channel{0U};
    std::uint8_t dataLength{0U};
    bool extended{false};
    CanDirection direction{CanDirection::Rx};
    CanFrameStatus status{CanFrameStatus::None};
    std::array<std::uint8_t, MaximumDataLength> data{};

    [[nodiscard]] constexpr bool isValid() const noexcept {
        const std::uint32_t maximumIdentifier =
            extended ? MaximumExtendedIdentifier : MaximumStandardIdentifier;
        return dataLength <= MaximumDataLength &&
               identifier <= maximumIdentifier &&
               direction == CanDirection::Rx;
    }

    [[nodiscard]] constexpr std::uint32_t timestampMilliseconds() const noexcept {
        return static_cast<std::uint32_t>(timestampUs / 1'000U);
    }
};

class CanFrameConsumer {
public:
    virtual ~CanFrameConsumer() = default;
    virtual bool consume(const CanFrame& frame) noexcept = 0;
};

}  // namespace can_core
