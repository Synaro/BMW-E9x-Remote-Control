#if defined(ESP_PLATFORM)

#include "bmw_remote/infrastructure/esp_idf_hal.hpp"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace bmw::remote::infrastructure {
namespace {

[[nodiscard]] bool validPin(const std::uint8_t pin) noexcept {
    return GPIO_IS_VALID_GPIO(pin);
}

[[nodiscard]] gpio_num_t gpioNumber(const std::uint8_t pin) noexcept {
    return static_cast<gpio_num_t>(pin);
}

[[nodiscard]] std::uint32_t rawLevel(const GpioLevel level) noexcept {
    return level == GpioLevel::High ? 1U : 0U;
}

}  // namespace

bool EspIdfGpioHal::configureOutputInSafeState(
    const std::uint8_t pin,
    const GpioLevel safeLevel) noexcept {
    if (!validPin(pin) ||
        gpio_set_level(gpioNumber(pin), rawLevel(safeLevel)) != ESP_OK) {
        return false;
    }

    gpio_config_t config{};
    config.pin_bit_mask = std::uint64_t{1U} << pin;
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    return gpio_config(&config) == ESP_OK;
}

bool EspIdfGpioHal::write(
    const std::uint8_t pin,
    const GpioLevel level) noexcept {
    return validPin(pin) &&
           gpio_set_level(gpioNumber(pin), rawLevel(level)) == ESP_OK;
}

bool EspIdfGpioHal::configureInput(const std::uint8_t pin) noexcept {
    if (!validPin(pin)) {
        return false;
    }

    gpio_config_t config{};
    config.pin_bit_mask = std::uint64_t{1U} << pin;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    return gpio_config(&config) == ESP_OK;
}

bool EspIdfGpioHal::read(
    const std::uint8_t pin,
    GpioLevel& level) noexcept {
    if (!validPin(pin)) {
        return false;
    }
    level = gpio_get_level(gpioNumber(pin)) == 0
        ? GpioLevel::Low
        : GpioLevel::High;
    return true;
}

std::uint32_t EspIdfMonotonicTimeHal::nowMs() const noexcept {
    return static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(esp_timer_get_time()) / 1'000ULL);
}

void EspIdfMonotonicTimeHal::delayMs(
    const std::uint32_t durationMs) noexcept {
    const TickType_t ticks = pdMS_TO_TICKS(durationMs);
    vTaskDelay(ticks == 0U ? 1U : ticks);
}

}  // namespace bmw::remote::infrastructure

#endif
