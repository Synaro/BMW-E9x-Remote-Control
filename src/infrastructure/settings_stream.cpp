#include "bmw_remote/infrastructure/settings_stream.hpp"

namespace bmw::remote::infrastructure {
namespace {

constexpr std::size_t PayloadSizeOffset = 10U;

[[nodiscard]] std::uint16_t readU16(
    const std::uint8_t* const source) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(source[0]) |
        static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(source[1]) << 8U));
}

}  // namespace

SettingsStreamEvent SettingsStreamReceiver::consume(
    const std::uint8_t byte,
    const std::uint32_t nowMs) noexcept {
    const bool timedOut = expired(nowMs);
    if (timedOut) {
        reset();
    }

    lastByteTimeMs_ = nowMs;
    const SettingsStreamEvent event = consumeSynchronized(byte);
    if (event.type != SettingsStreamEventType::None) {
        return event;
    }
    if (timedOut) {
        return SettingsStreamEvent{SettingsStreamEventType::TimedOut};
    }
    return event;
}

SettingsStreamEvent SettingsStreamReceiver::poll(
    const std::uint32_t nowMs) noexcept {
    if (!expired(nowMs)) {
        return SettingsStreamEvent{};
    }
    reset();
    return SettingsStreamEvent{SettingsStreamEventType::TimedOut};
}

void SettingsStreamReceiver::reset() noexcept {
    buffer_.fill(0U);
    bufferedSize_ = 0U;
    expectedSize_ = 0U;
    lastByteTimeMs_ = 0U;
}

SettingsStreamEvent SettingsStreamReceiver::consumeSynchronized(
    const std::uint8_t byte) noexcept {
    if (bufferedSize_ < SettingsProtocolCodec::Magic.size()) {
        if (byte == SettingsProtocolCodec::Magic[bufferedSize_]) {
            buffer_[bufferedSize_] = byte;
            ++bufferedSize_;
        } else if (byte == SettingsProtocolCodec::Magic[0U]) {
            buffer_[0U] = byte;
            bufferedSize_ = 1U;
        } else {
            bufferedSize_ = 0U;
        }
        return SettingsStreamEvent{};
    }

    if (bufferedSize_ >= buffer_.size()) {
        reset();
        return SettingsStreamEvent{
            SettingsStreamEventType::FrameRejected,
            SettingsFrameDecodeStatus::TooLong};
    }

    buffer_[bufferedSize_] = byte;
    ++bufferedSize_;
    if (bufferedSize_ == SettingsProtocolCodec::HeaderSize) {
        const std::uint16_t payloadSize =
            readU16(buffer_.data() + PayloadSizeOffset);
        if (payloadSize > UserSettingsPayloadSize) {
            reset();
            return SettingsStreamEvent{
                SettingsStreamEventType::FrameRejected,
                SettingsFrameDecodeStatus::PayloadTooLarge};
        }
        expectedSize_ = SettingsProtocolCodec::HeaderSize +
                        static_cast<std::size_t>(payloadSize) +
                        SettingsProtocolCodec::ChecksumSize;
    }

    if (expectedSize_ == 0U || bufferedSize_ < expectedSize_) {
        return SettingsStreamEvent{};
    }

    const SettingsFrameDecodeResult decoded =
        SettingsProtocolCodec::decode(buffer_.data(), bufferedSize_);
    reset();
    if (!decoded.valid()) {
        return SettingsStreamEvent{
            SettingsStreamEventType::FrameRejected,
            decoded.status};
    }
    return SettingsStreamEvent{
        SettingsStreamEventType::FrameReady,
        SettingsFrameDecodeStatus::Ok,
        decoded.frame};
}

bool SettingsStreamReceiver::expired(const std::uint32_t nowMs) const noexcept {
    return bufferedSize_ != 0U &&
           static_cast<std::uint32_t>(nowMs - lastByteTimeMs_) >
               config_.interByteTimeoutMs;
}

SettingsEndpointResult SettingsProtocolEndpoint::consume(
    const std::uint8_t byte,
    const std::uint32_t nowMs,
    const SettingsProtocolAccess access) noexcept {
    return handleStreamEvent(receiver_.consume(byte, nowMs), access);
}

SettingsEndpointResult SettingsProtocolEndpoint::poll(
    const std::uint32_t nowMs) noexcept {
    const SettingsStreamEvent event = receiver_.poll(nowMs);
    if (event.type == SettingsStreamEventType::TimedOut) {
        return SettingsEndpointResult{SettingsEndpointStatus::FrameTimedOut};
    }
    return SettingsEndpointResult{};
}

SettingsEndpointResult SettingsProtocolEndpoint::handleStreamEvent(
    const SettingsStreamEvent& event,
    const SettingsProtocolAccess access) noexcept {
    if (event.type == SettingsStreamEventType::None) {
        return SettingsEndpointResult{};
    }
    if (event.type == SettingsStreamEventType::TimedOut) {
        return SettingsEndpointResult{SettingsEndpointStatus::FrameTimedOut};
    }
    if (event.type == SettingsStreamEventType::FrameRejected) {
        return SettingsEndpointResult{
            SettingsEndpointStatus::FrameRejected,
            event.decodeStatus};
    }

    const SettingsProtocolFrame response = service_.handle(event.frame, access);
    SettingsProtocolCodec::EncodedFrame encoded{};
    std::size_t encodedSize = 0U;
    if (!SettingsProtocolCodec::encode(response, encoded, encodedSize) ||
        !transport_.send(encoded.data(), encodedSize)) {
        return SettingsEndpointResult{
            SettingsEndpointStatus::TransportFailure,
            SettingsFrameDecodeStatus::Ok,
            response.status};
    }
    return SettingsEndpointResult{
        SettingsEndpointStatus::ResponseSent,
        SettingsFrameDecodeStatus::Ok,
        response.status};
}

const char* toString(const SettingsStreamEventType type) noexcept {
    switch (type) {
        case SettingsStreamEventType::None: return "none";
        case SettingsStreamEventType::FrameReady: return "frame_ready";
        case SettingsStreamEventType::FrameRejected: return "frame_rejected";
        case SettingsStreamEventType::TimedOut: return "timed_out";
    }
    return "unknown";
}

const char* toString(const SettingsEndpointStatus status) noexcept {
    switch (status) {
        case SettingsEndpointStatus::None: return "none";
        case SettingsEndpointStatus::ResponseSent: return "response_sent";
        case SettingsEndpointStatus::FrameRejected: return "frame_rejected";
        case SettingsEndpointStatus::FrameTimedOut: return "frame_timed_out";
        case SettingsEndpointStatus::TransportFailure: return "transport_failure";
    }
    return "unknown";
}

}  // namespace bmw::remote::infrastructure
