#pragma once

#if defined(ESP_PLATFORM)

#include "bmw_remote/infrastructure/hardware_abstraction.hpp"

namespace bmw::remote::infrastructure {

class EspIdfGpioHal final : public GpioHal {
public:
    bool configureOutputInSafeState(
        std::uint8_t pin,
        GpioLevel safeLevel) noexcept override;
    bool write(
        std::uint8_t pin,
        GpioLevel level) noexcept override;
    bool configureInput(std::uint8_t pin) noexcept override;
    bool read(
        std::uint8_t pin,
        GpioLevel& level) noexcept override;
};

class EspIdfMonotonicTimeHal final : public MonotonicTimeHal {
public:
    [[nodiscard]] std::uint32_t nowMs() const noexcept override;
    void delayMs(std::uint32_t durationMs) noexcept override;
};

}  // namespace bmw::remote::infrastructure

#endif
