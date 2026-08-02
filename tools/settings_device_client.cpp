#include "tools/settings_device_client.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

#include "bmw_remote/application/user_settings.hpp"
#include "bmw_remote/infrastructure/settings_payload.hpp"
#include "bmw_remote/infrastructure/settings_stream.hpp"

namespace bmw::remote::host {
namespace {

using Clock = std::chrono::steady_clock;

[[nodiscard]] std::uint32_t elapsedMilliseconds(
    const Clock::time_point startedAt) noexcept {
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now() - startedAt);
    if (elapsed.count() <= 0) {
        return 0U;
    }
    const auto milliseconds = static_cast<std::uint64_t>(elapsed.count());
    return milliseconds > UINT32_MAX
               ? UINT32_MAX
               : static_cast<std::uint32_t>(milliseconds);
}

[[nodiscard]] bool validateClientConfig(
    const SettingsDeviceClientConfig config,
    std::string& error) {
    if (config.interByteTimeoutMs == 0U ||
        config.responseTimeoutMs < config.interByteTimeoutMs) {
        error = "temporisations de liaison invalides";
        return false;
    }
    return true;
}

}  // namespace

bool SettingsDeviceClient::probe(
    infrastructure::SettingsDeviceIdentity& identity,
    std::string& error) {
    infrastructure::SettingsProtocolFrame request{};
    request.type = infrastructure::SettingsMessageType::IdentifyRequest;

    infrastructure::SettingsProtocolFrame response{};
    if (!exchange(
            request,
            infrastructure::SettingsMessageType::IdentifyResponse,
            response,
            error)) {
        return false;
    }

    infrastructure::SettingsDeviceIdentity received{};
    if (!infrastructure::decodeSettingsDeviceIdentity(
            response.payload, response.payloadSize, received)) {
        error = "identite du boitier invalide";
        return false;
    }
    identity = received;
    error.clear();
    return true;
}

bool SettingsDeviceClient::read(
    application::UserSettings& settings,
    std::string& error) {
    infrastructure::SettingsProtocolFrame request{};
    request.type = infrastructure::SettingsMessageType::ReadRequest;

    infrastructure::SettingsProtocolFrame response{};
    if (!exchange(
            request,
            infrastructure::SettingsMessageType::ReadResponse,
            response,
            error)) {
        return false;
    }
    if (response.payloadSize != infrastructure::UserSettingsPayloadSize) {
        error = "taille de configuration inattendue dans la reponse";
        return false;
    }

    application::UserSettings received{};
    if (!infrastructure::decodeUserSettingsPayload(
            response.payload, received)) {
        error = "configuration recue invalide";
        return false;
    }
    settings = received;
    error.clear();
    return true;
}

bool SettingsDeviceClient::writeAndVerify(
    const application::UserSettings& settings,
    std::string& error) {
    if (!application::validateUserSettings(settings).valid()) {
        error = "configuration refusee par les limites de securite";
        return false;
    }

    infrastructure::SettingsDeviceIdentity identity{};
    if (!probe(identity, error)) {
        error = "identification du boitier impossible : " + error;
        return false;
    }
    if (!infrastructure::hasCapability(
            identity,
            infrastructure::SettingsDeviceCapability::SettingsWrite) ||
        !infrastructure::hasCapability(
            identity,
            infrastructure::SettingsDeviceCapability::PersistentSettings)) {
        error = "le boitier n'annonce pas l'ecriture persistante des reglages";
        return false;
    }

    infrastructure::SettingsProtocolFrame request{};
    request.type = infrastructure::SettingsMessageType::WriteRequest;
    request.payloadSize = static_cast<std::uint16_t>(
        infrastructure::UserSettingsPayloadSize);
    if (!infrastructure::encodeUserSettingsPayload(settings, request.payload)) {
        error = "impossible d'encoder la configuration";
        return false;
    }

    infrastructure::SettingsProtocolFrame response{};
    if (!exchange(
            request,
            infrastructure::SettingsMessageType::WriteResponse,
            response,
            error)) {
        return false;
    }
    if (response.payloadSize != 0U) {
        error = "payload inattendu dans la reponse d'ecriture";
        return false;
    }

    application::UserSettings verified{};
    if (!read(verified, error)) {
        error = "ecriture acceptee mais relecture impossible : " + error;
        return false;
    }
    if (!infrastructure::userSettingsEqual(settings, verified)) {
        error = "la relecture du boitier differe de la configuration envoyee";
        return false;
    }
    error.clear();
    return true;
}

bool SettingsDeviceClient::exchange(
    infrastructure::SettingsProtocolFrame request,
    const infrastructure::SettingsMessageType expectedResponseType,
    infrastructure::SettingsProtocolFrame& response,
    std::string& error) {
    error.clear();
    if (!validateClientConfig(config_, error)) {
        return false;
    }

    request.requestId = allocateRequestId();
    infrastructure::SettingsProtocolCodec::EncodedFrame encoded{};
    std::size_t encodedSize = 0U;
    if (!infrastructure::SettingsProtocolCodec::encode(
            request, encoded, encodedSize)) {
        error = "impossible d'encoder la requete";
        return false;
    }
    if (!channel_.clearInput(error) ||
        !channel_.writeAll(
            encoded.data(),
            encodedSize,
            config_.responseTimeoutMs,
            error)) {
        if (error.empty()) {
            error = "echec d'envoi de la requete";
        }
        return false;
    }

    infrastructure::SettingsStreamReceiver receiver{
        infrastructure::SettingsStreamConfig{config_.interByteTimeoutMs}};
    const Clock::time_point startedAt = Clock::now();
    for (;;) {
        const std::uint32_t elapsed = elapsedMilliseconds(startedAt);
        if (elapsed >= config_.responseTimeoutMs) {
            error = "delai de reponse du boitier depasse";
            return false;
        }

        const std::uint32_t remaining = config_.responseTimeoutMs - elapsed;
        const std::uint32_t readTimeout = receiver.bufferedSize() == 0U
                                              ? remaining
                                              : std::min(
                                                    remaining,
                                                    config_.interByteTimeoutMs);
        std::uint8_t byte = 0U;
        const SettingsChannelReadStatus readStatus =
            channel_.readByte(byte, readTimeout, error);
        if (readStatus == SettingsChannelReadStatus::Failure) {
            if (error.empty()) {
                error = "echec de lecture de la reponse";
            }
            return false;
        }
        if (readStatus == SettingsChannelReadStatus::Timeout) {
            if (receiver.bufferedSize() != 0U) {
                error = "reponse partielle : delai inter-octets depasse";
                return false;
            }
            error = "delai de reponse du boitier depasse";
            return false;
        }

        const infrastructure::SettingsStreamEvent event = receiver.consume(
            byte,
            elapsedMilliseconds(startedAt));
        if (event.type == infrastructure::SettingsStreamEventType::None) {
            continue;
        }
        if (event.type != infrastructure::SettingsStreamEventType::FrameReady) {
            error = "trame de reponse rejetee : " +
                    std::string{infrastructure::toString(event.decodeStatus)};
            return false;
        }

        if (event.frame.requestId != request.requestId) {
            error = "identifiant de reponse inattendu";
            return false;
        }
        if (event.frame.type != expectedResponseType) {
            error = "type de reponse inattendu";
            return false;
        }
        if (event.frame.status != infrastructure::SettingsProtocolStatus::Ok) {
            error = "requete refusee par le boitier : " +
                    std::string{infrastructure::toString(event.frame.status)};
            return false;
        }

        response = event.frame;
        error.clear();
        return true;
    }
}

std::uint16_t SettingsDeviceClient::allocateRequestId() noexcept {
    const std::uint16_t allocated = nextRequestId_;
    ++nextRequestId_;
    if (nextRequestId_ == 0U) {
        nextRequestId_ = 1U;
    }
    return allocated;
}

}  // namespace bmw::remote::host
