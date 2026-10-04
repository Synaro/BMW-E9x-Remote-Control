class TestTwaiSafety final : public bmw::remote::infrastructure::TwaiSafetyHal {
public:
    bool silentResult{true};
    bool inhibitResult{true};
    bool confirmationResult{true};
    std::uint8_t silentCalls{0U};
    std::uint8_t inhibitCalls{0U};
    std::uint8_t confirmationCalls{0U};

    bool holdHardwareSilent() noexcept override {
        ++silentCalls;
        return silentResult;
    }

    bool holdTransmitInhibited() noexcept override {
        ++inhibitCalls;
        return inhibitResult;
    }

    [[nodiscard]] bool safeStateConfirmed() const noexcept override {
        ++const_cast<TestTwaiSafety*>(this)->confirmationCalls;
        return confirmationResult;
    }
};

class TestTwaiModeGpio final : public bmw::remote::infrastructure::GpioHal {
public:
    bool configureResult{true};
    bool writeResult{true};
    std::array<bmw::remote::infrastructure::GpioLevel, 49U> levels{};
    std::array<bool, 49U> configured{};

    bool configureOutputInSafeState(
        const std::uint8_t pin,
        const bmw::remote::infrastructure::GpioLevel safeLevel) noexcept override {
        if (!configureResult || pin >= levels.size()) {
            return false;
        }
        configured[pin] = true;
        levels[pin] = safeLevel;
        return true;
    }

    bool write(
        const std::uint8_t pin,
        const bmw::remote::infrastructure::GpioLevel level) noexcept override {
        if (!writeResult || pin >= levels.size() || !configured[pin]) {
            return false;
        }
        levels[pin] = level;
        return true;
    }

    bool configureInput(const std::uint8_t) noexcept override {
        return true;
    }

    bool read(
        const std::uint8_t pin,
        bmw::remote::infrastructure::GpioLevel& level) noexcept override {
        if (pin >= levels.size() || !configured[pin]) {
            return false;
        }
        level = levels[pin];
        return true;
    }
};

can_core::CanFrame testCaptureFrame(
    const std::uint64_t timestampUs,
    const std::uint64_t sequence) {
    can_core::CanFrame frame{};
    frame.timestampUs = timestampUs;
    frame.sequence = sequence;
    frame.channel = 1U;
    frame.bitrate = 500'000U;
    frame.identifier = 0x123U;
    frame.dataLength = 2U;
    frame.data[0U] = static_cast<std::uint8_t>(sequence);
    frame.data[1U] = 0xA5U;
    return frame;
}

void testCanCoreFrameModelIsStrictAndReceiveOnly() {
    can_core::CanFrame standard = testCaptureFrame(123'456U, 7U);
    CHECK(standard.isValid());
    CHECK(standard.timestampMilliseconds() == 123U);
    CHECK(standard.direction == can_core::CanDirection::Rx);

    can_core::CanFrame extended = standard;
    extended.extended = true;
    extended.identifier = can_core::CanFrame::MaximumExtendedIdentifier;
    extended.status =
        can_core::CanFrameStatus::RemoteFrame |
        can_core::CanFrameStatus::ErrorWarning;
    CHECK(extended.isValid());
    CHECK(can_core::hasStatus(
        extended.status,
        can_core::CanFrameStatus::RemoteFrame));

    standard.identifier = can_core::CanFrame::MaximumStandardIdentifier + 1U;
    CHECK(!standard.isValid());
    extended.dataLength = can_core::CanFrame::MaximumDataLength + 1U;
    CHECK(!extended.isValid());
}

void testCanCoreQueueIsBoundedAndReportsOverflow() {
    can_core::BoundedSpscQueue<can_core::CanFrame, 3U> queue{};
    CHECK(queue.empty());
    CHECK(queue.capacity() == 3U);
    CHECK(queue.tryPush(testCaptureFrame(10U, 1U)));
    CHECK(queue.tryPush(testCaptureFrame(20U, 2U)));
    CHECK(queue.tryPush(testCaptureFrame(30U, 3U)));
    CHECK(queue.full());
    CHECK(!queue.tryPush(testCaptureFrame(40U, 4U)));
    CHECK(queue.size() == 3U);
}

void testCanCoreQueuePreservesTimestampAndSequenceOrder() {
    can_core::BoundedSpscQueue<can_core::CanFrame, 4U> queue{};
    CHECK(queue.tryPush(testCaptureFrame(1'000U, 41U)));
    CHECK(queue.tryPush(testCaptureFrame(1'000U, 42U)));
    CHECK(queue.tryPush(testCaptureFrame(1'001U, 43U)));

    can_core::CanFrame frame{};
    for (std::uint64_t expected = 41U; expected <= 43U; ++expected) {
        CHECK(queue.tryPop(frame));
        CHECK(frame.sequence == expected);
        CHECK(frame.timestampUs == (expected == 43U ? 1'001U : 1'000U));
    }
    CHECK(!queue.tryPop(frame));
    CHECK(queue.empty());
}

void testTwaiStartupRejectsUnsafeHardwareBarriers() {
    TestTwaiSafety unsafeSilent{};
    unsafeSilent.silentResult = false;
    CHECK(!bmw::remote::infrastructure::prepareTwaiListenOnlySafety(
        unsafeSilent));
    CHECK(unsafeSilent.silentCalls == 1U);
    CHECK(unsafeSilent.inhibitCalls == 0U);

    TestTwaiSafety unsafeInhibit{};
    unsafeInhibit.inhibitResult = false;
    CHECK(!bmw::remote::infrastructure::prepareTwaiListenOnlySafety(
        unsafeInhibit));
    CHECK(unsafeInhibit.confirmationCalls == 0U);

    TestTwaiSafety unconfirmed{};
    unconfirmed.confirmationResult = false;
    CHECK(!bmw::remote::infrastructure::prepareTwaiListenOnlySafety(
        unconfirmed));

    TestTwaiSafety safe{};
    CHECK(bmw::remote::infrastructure::prepareTwaiListenOnlySafety(safe));
    CHECK(safe.silentCalls == 1U);
    CHECK(safe.inhibitCalls == 1U);
    CHECK(safe.confirmationCalls == 1U);
}

void testTwaiListenOnlyConfigurationAccepts100KbitBenchRate() {
    bmw::remote::infrastructure::TwaiListenOnlyConfig config{};
    config.receiver.bitrate = can_core::CanBitrate::Kbit100;
    config.receivePin = 4;
    config.transmitPin = 5;
    CHECK(config.isValid());
    CHECK(can_core::bitrateValue(config.receiver.bitrate) == 100'000U);
}

void testTwaiListenOnlyConfigurationAccepts500KbitBenchRate() {
    bmw::remote::infrastructure::TwaiListenOnlyConfig config{};
    config.receiver.bitrate = can_core::CanBitrate::Kbit500;
    config.receivePin = 4;
    config.transmitPin = 5;
    CHECK(config.isValid());
    CHECK(can_core::bitrateValue(config.receiver.bitrate) == 500'000U);

    config.receiver.bitrate = static_cast<can_core::CanBitrate>(250'000U);
    CHECK(!config.isValid());
    config.receiver.bitrate = can_core::CanBitrate::Kbit500;
    config.transmitPin = config.receivePin;
    CHECK(!config.isValid());
}

void testTwaiDriverModeDefaultsToListenOnlyAndCanSelectNormal() {
    bmw::remote::infrastructure::TwaiDriverConfig config{};
    config.receivePin = 4;
    config.transmitPin = 5;
    CHECK(config.isValid());
    CHECK(config.operatingMode ==
          bmw::remote::infrastructure::TwaiOperatingMode::ListenOnly);

    config.operatingMode =
        bmw::remote::infrastructure::TwaiOperatingMode::Normal;
    CHECK(!config.isValid());
    config.hardwareTopology =
        bmw::remote::infrastructure::TwaiHardwareTopology::Phase3hBidirectional;
    config.transceiverModePins = {6, 15};
    CHECK(config.isValid());
}

void testPhase3hTransceiverModePinsActivateAndReturnToLowPower() {
    using bmw::remote::infrastructure::GpioLevel;
    using bmw::remote::infrastructure::TwaiTransceiverModePinConfig;

    TestTwaiModeGpio gpio{};
    const TwaiTransceiverModePinConfig pins{6, 15};
    CHECK(bmw::remote::infrastructure::activatePhase3hTransceiver(gpio, pins));
    CHECK(gpio.configured[6U]);
    CHECK(gpio.configured[15U]);
    CHECK(gpio.levels[6U] == GpioLevel::High);
    CHECK(gpio.levels[15U] == GpioLevel::High);

    bmw::remote::infrastructure::deactivatePhase3hTransceiver(gpio, pins);
    CHECK(gpio.levels[6U] == GpioLevel::Low);
    CHECK(gpio.levels[15U] == GpioLevel::Low);
}

void testCaptureV2SchemaAndManifestRemainReceiveOnly() {
    CHECK(can_core::CaptureFormatVersion == 2U);
    CHECK(can_core::CaptureV2CsvHeader ==
          "timestamp_us,sequence,channel,bitrate,id,extended,dlc,data,direction,status");

    can_core::CaptureSessionManifest manifest{};
    const auto setText = [](auto& destination, const char* source) {
        std::size_t index = 0U;
        while (source[index] != '\0' && index + 1U < destination.size()) {
            destination[index] = source[index];
            ++index;
        }
        destination[index] = '\0';
    };
    setText(manifest.sessionId, "bench-session");
    setText(manifest.firmwareVersion, "phase3-test");
    setText(manifest.interfaceName, "esp32s3-twai");
    setText(manifest.configurationId, "bench-only");
    manifest.bitrate = 100'000U;
    manifest.statistics.framesReceived = 12U;
    manifest.statistics.softwareQueueOverruns = 1U;
    CHECK(manifest.isValid());
    CHECK(manifest.listenOnly);
    CHECK(manifest.vehicleInformation[0U] == '\0');
}
