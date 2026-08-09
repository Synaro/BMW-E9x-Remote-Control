#include "bmw_remote/infrastructure/twai_listen_only.hpp"

namespace bmw::remote::infrastructure {

bool GpioTwaiSafetyHal::holdHardwareSilent() noexcept {
    if (!config_.isValid()) {
        silentHeld_ = false;
        return false;
    }
    silentHeld_ = gpio_.configureOutputInSafeState(
        static_cast<std::uint8_t>(config_.silentPin),
        config_.silentLevel);
    return silentHeld_;
}

bool GpioTwaiSafetyHal::holdTransmitInhibited() noexcept {
    if (!config_.isValid()) {
        transmitInhibited_ = false;
        return false;
    }
    transmitInhibited_ = gpio_.configureOutputInSafeState(
        static_cast<std::uint8_t>(config_.transmitInhibitPin),
        config_.transmitInhibitLevel);
    return transmitInhibited_;
}

bool GpioTwaiSafetyHal::safeStateConfirmed() const noexcept {
    if (!config_.isValid() || !silentHeld_ || !transmitInhibited_) {
        return false;
    }
    GpioLevel silentLevel{};
    GpioLevel inhibitLevel{};
    return gpio_.read(
               static_cast<std::uint8_t>(config_.silentPin),
               silentLevel) &&
           gpio_.read(
               static_cast<std::uint8_t>(config_.transmitInhibitPin),
               inhibitLevel) &&
           silentLevel == config_.silentLevel &&
           inhibitLevel == config_.transmitInhibitLevel;
}

bool prepareTwaiListenOnlySafety(TwaiSafetyHal& safety) noexcept {
    return safety.holdHardwareSilent() &&
           safety.holdTransmitInhibited() &&
           safety.safeStateConfirmed();
}

}  // namespace bmw::remote::infrastructure
