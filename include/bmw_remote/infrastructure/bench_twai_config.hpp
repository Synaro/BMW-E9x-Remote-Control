#pragma once

#include "bmw_remote/infrastructure/twai_listen_only.hpp"

#ifndef BMW_REMOTE_BENCH_ONLY_TWAI_ENABLED
#define BMW_REMOTE_BENCH_ONLY_TWAI_ENABLED 0
#endif
#ifndef BMW_REMOTE_BENCH_ONLY_TWAI_RX_GPIO
#define BMW_REMOTE_BENCH_ONLY_TWAI_RX_GPIO -1
#endif
#ifndef BMW_REMOTE_BENCH_ONLY_TWAI_TX_GPIO
#define BMW_REMOTE_BENCH_ONLY_TWAI_TX_GPIO -1
#endif
#ifndef BMW_REMOTE_BENCH_ONLY_SILENT_GPIO
#define BMW_REMOTE_BENCH_ONLY_SILENT_GPIO -1
#endif
#ifndef BMW_REMOTE_BENCH_ONLY_TX_INHIBIT_GPIO
#define BMW_REMOTE_BENCH_ONLY_TX_INHIBIT_GPIO -1
#endif
#ifndef BMW_REMOTE_BENCH_ONLY_SILENT_LEVEL
#define BMW_REMOTE_BENCH_ONLY_SILENT_LEVEL 1
#endif
#ifndef BMW_REMOTE_BENCH_ONLY_TX_INHIBIT_LEVEL
#define BMW_REMOTE_BENCH_ONLY_TX_INHIBIT_LEVEL 1
#endif
#ifndef BMW_REMOTE_BENCH_ONLY_BITRATE
#define BMW_REMOTE_BENCH_ONLY_BITRATE 100000
#endif
#ifndef BMW_REMOTE_BENCH_ONLY_CHANNEL
#define BMW_REMOTE_BENCH_ONLY_CHANNEL 0
#endif
#ifndef BMW_REMOTE_BENCH_ONLY_NORMAL_MODE
#define BMW_REMOTE_BENCH_ONLY_NORMAL_MODE 0
#endif
#ifndef BMW_REMOTE_BENCH_ONLY_PHASE3H_HARDWARE
#define BMW_REMOTE_BENCH_ONLY_PHASE3H_HARDWARE 0
#endif
#ifndef BMW_REMOTE_BENCH_ONLY_TJA_STB_GPIO
#define BMW_REMOTE_BENCH_ONLY_TJA_STB_GPIO -1
#endif
#ifndef BMW_REMOTE_BENCH_ONLY_TJA_EN_GPIO
#define BMW_REMOTE_BENCH_ONLY_TJA_EN_GPIO -1
#endif

namespace bmw::remote::infrastructure {

struct BenchOnlyTwaiConfiguration final {
    bool enabled{false};
    TwaiSafetyPinConfig safety{};
    TwaiDriverConfig acquisition{};

    [[nodiscard]] constexpr bool isValid() const noexcept {
        if (!enabled) {
            return true;
        }
        if (!acquisition.isValid()) {
            return false;
        }
        if (acquisition.hardwareTopology ==
            TwaiHardwareTopology::Phase3hBidirectional) {
            return true;
        }
        return acquisition.operatingMode == TwaiOperatingMode::ListenOnly &&
               safety.isValid() &&
               safety.silentPin != acquisition.receivePin &&
               safety.silentPin != acquisition.transmitPin &&
               safety.transmitInhibitPin != acquisition.receivePin &&
               safety.transmitInhibitPin != acquisition.transmitPin;
    }
};

[[nodiscard]] constexpr GpioLevel configuredLevel(
    const int level) noexcept {
    return level == 0 ? GpioLevel::Low : GpioLevel::High;
}

[[nodiscard]] constexpr can_core::CanBitrate configuredBenchBitrate() noexcept {
    return BMW_REMOTE_BENCH_ONLY_BITRATE == 100000
        ? can_core::CanBitrate::Kbit100
        : (BMW_REMOTE_BENCH_ONLY_BITRATE == 500000
               ? can_core::CanBitrate::Kbit500
               : static_cast<can_core::CanBitrate>(
                     BMW_REMOTE_BENCH_ONLY_BITRATE));
}

[[nodiscard]] constexpr BenchOnlyTwaiConfiguration
compiledBenchOnlyTwaiConfiguration() noexcept {
    return {
        BMW_REMOTE_BENCH_ONLY_TWAI_ENABLED == 1,
        {
            BMW_REMOTE_BENCH_ONLY_SILENT_GPIO,
            configuredLevel(BMW_REMOTE_BENCH_ONLY_SILENT_LEVEL),
            BMW_REMOTE_BENCH_ONLY_TX_INHIBIT_GPIO,
            configuredLevel(BMW_REMOTE_BENCH_ONLY_TX_INHIBIT_LEVEL),
        },
        {
            {
                configuredBenchBitrate(),
                static_cast<std::uint8_t>(BMW_REMOTE_BENCH_ONLY_CHANNEL),
            },
            BMW_REMOTE_BENCH_ONLY_TWAI_RX_GPIO,
            BMW_REMOTE_BENCH_ONLY_TWAI_TX_GPIO,
            64U,
            10U,
            8U,
            0,
            BMW_REMOTE_BENCH_ONLY_NORMAL_MODE == 1
                ? TwaiOperatingMode::Normal
                : TwaiOperatingMode::ListenOnly,
            BMW_REMOTE_BENCH_ONLY_PHASE3H_HARDWARE == 1
                ? TwaiHardwareTopology::Phase3hBidirectional
                : TwaiHardwareTopology::LegacyBarrieredBench,
            {
                BMW_REMOTE_BENCH_ONLY_TJA_STB_GPIO,
                BMW_REMOTE_BENCH_ONLY_TJA_EN_GPIO,
            },
        },
    };
}

}  // namespace bmw::remote::infrastructure
