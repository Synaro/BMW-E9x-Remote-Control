#pragma once

#include <cstdint>

namespace bmw::remote::infrastructure {

enum class GpioLevel : std::uint8_t {
    Low,
    High,
};

class GpioHal {
public:
    virtual ~GpioHal() = default;

    virtual bool configureOutputInSafeState(
        std::uint8_t pin,
        GpioLevel safeLevel) noexcept = 0;
    virtual bool write(
        std::uint8_t pin,
        GpioLevel level) noexcept = 0;
    virtual bool configureInput(std::uint8_t pin) noexcept = 0;
    virtual bool read(
        std::uint8_t pin,
        GpioLevel& level) noexcept = 0;
};

class MonotonicTimeHal {
public:
    virtual ~MonotonicTimeHal() = default;

    [[nodiscard]] virtual std::uint32_t nowMs() const noexcept = 0;
    virtual void delayMs(std::uint32_t durationMs) noexcept = 0;
};

// Safety-only contract reserved for the future TWAI adapter. It intentionally
// exposes no receive or transmit operation in Phase 2.
class FutureTwaiSafetyHal {
public:
    virtual ~FutureTwaiSafetyHal() = default;

    virtual bool holdHardwareSilent() noexcept = 0;
    virtual bool holdTransmitInhibited() noexcept = 0;
    [[nodiscard]] virtual bool safeStateConfirmed() const noexcept = 0;
};

}  // namespace bmw::remote::infrastructure
