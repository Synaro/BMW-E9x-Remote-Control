#pragma once

#include <array>
#include <cstdint>

namespace bmw::remote::infrastructure {

struct CanFrame final {
    static constexpr std::uint32_t MaximumStandardIdentifier = 0x7FFU;
    static constexpr std::uint32_t MaximumExtendedIdentifier = 0x1FFFFFFFU;
    static constexpr std::uint8_t MaximumDataLength = 8U;

    std::uint32_t timestampMs{0U};
    std::uint32_t identifier{0U};
    bool extended{false};
    std::uint8_t dataLength{0U};
    std::array<std::uint8_t, MaximumDataLength> data{};

    [[nodiscard]] constexpr bool isValid() const noexcept {
        const std::uint32_t maximumIdentifier =
            extended ? MaximumExtendedIdentifier : MaximumStandardIdentifier;
        return dataLength <= MaximumDataLength && identifier <= maximumIdentifier;
    }
};

class CanFrameConsumer {
public:
    virtual ~CanFrameConsumer() = default;
    virtual bool consume(const CanFrame& frame) noexcept = 0;
};

}  // namespace bmw::remote::infrastructure
