#include "tools/serial_settings_channel.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)

#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#endif

namespace bmw::remote::host {

#if defined(_WIN32)
namespace {

using Clock = std::chrono::steady_clock;

[[nodiscard]] HANDLE nativeHandle(void* const handle) noexcept {
    return static_cast<HANDLE>(handle);
}

[[nodiscard]] std::string windowsError(
    const std::string_view operation,
    const DWORD code = GetLastError()) {
    char* message = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                        FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD size = FormatMessageA(
        flags,
        nullptr,
        code,
        0U,
        reinterpret_cast<char*>(&message),
        0U,
        nullptr);

    std::string result{operation};
    result += " (erreur Windows " + std::to_string(code) + ")";
    if (size != 0U && message != nullptr) {
        std::string detail{message, static_cast<std::size_t>(size)};
        while (!detail.empty() &&
               std::isspace(static_cast<unsigned char>(detail.back())) != 0) {
            detail.pop_back();
        }
        if (!detail.empty()) {
            result += ": " + detail;
        }
    }
    if (message != nullptr) {
        LocalFree(message);
    }
    return result;
}

[[nodiscard]] bool normalizedPortPath(
    const std::string& portName,
    std::string& path) {
    std::string normalized = portName;
    constexpr const char* Prefix = "\\\\.\\";
    if (normalized.rfind(Prefix, 0U) == 0U) {
        normalized.erase(0U, 4U);
    }
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });

    if (normalized.size() < 4U || normalized.size() > 6U ||
        normalized.rfind("COM", 0U) != 0U) {
        return false;
    }
    for (std::size_t index = 3U; index < normalized.size(); ++index) {
        if (std::isdigit(static_cast<unsigned char>(normalized[index])) == 0) {
            return false;
        }
    }
    const unsigned long number = std::stoul(normalized.substr(3U));
    if (number == 0UL || number > 256UL) {
        return false;
    }
    path = std::string{Prefix} + normalized;
    return true;
}

[[nodiscard]] bool setTimeouts(
    const HANDLE handle,
    const std::uint32_t readTimeoutMs,
    const std::uint32_t writeTimeoutMs,
    std::string& error) {
    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0U;
    timeouts.ReadTotalTimeoutConstant = readTimeoutMs;
    timeouts.WriteTotalTimeoutMultiplier = 0U;
    timeouts.WriteTotalTimeoutConstant = writeTimeoutMs;
    if (SetCommTimeouts(handle, &timeouts) == FALSE) {
        error = windowsError("impossible de regler les delais du port serie");
        return false;
    }
    return true;
}

[[nodiscard]] std::uint32_t remainingMilliseconds(
    const Clock::time_point startedAt,
    const std::uint32_t timeoutMs) noexcept {
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now() - startedAt);
    if (elapsed.count() <= 0) {
        return timeoutMs;
    }
    const auto milliseconds = static_cast<std::uint64_t>(elapsed.count());
    if (milliseconds >= timeoutMs) {
        return 0U;
    }
    return timeoutMs - static_cast<std::uint32_t>(milliseconds);
}

}  // namespace
#endif

SerialSettingsChannel::~SerialSettingsChannel() {
    close();
}

bool SerialSettingsChannel::open(
    const std::string& portName,
    const std::uint32_t baudRate,
    std::string& error) {
    close();
#if defined(_WIN32)
    std::string path{};
    if (!normalizedPortPath(portName, path)) {
        error = "nom de port invalide ; format attendu : COM3";
        return false;
    }
    if (baudRate == 0U) {
        error = "debit serie invalide";
        return false;
    }

    const HANDLE handle = CreateFileA(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0U,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = windowsError("impossible d'ouvrir " + portName);
        return false;
    }

    DCB configuration{};
    configuration.DCBlength = static_cast<DWORD>(sizeof(configuration));
    if (GetCommState(handle, &configuration) == FALSE) {
        error = windowsError("impossible de lire la configuration serie");
        CloseHandle(handle);
        return false;
    }
    configuration.BaudRate = static_cast<DWORD>(baudRate);
    configuration.ByteSize = 8U;
    configuration.Parity = NOPARITY;
    configuration.StopBits = ONESTOPBIT;
    configuration.fBinary = TRUE;
    configuration.fParity = FALSE;
    configuration.fOutxCtsFlow = FALSE;
    configuration.fOutxDsrFlow = FALSE;
    configuration.fDtrControl = DTR_CONTROL_DISABLE;
    configuration.fDsrSensitivity = FALSE;
    configuration.fTXContinueOnXoff = TRUE;
    configuration.fOutX = FALSE;
    configuration.fInX = FALSE;
    configuration.fErrorChar = FALSE;
    configuration.fNull = FALSE;
    configuration.fRtsControl = RTS_CONTROL_DISABLE;
    configuration.fAbortOnError = FALSE;
    if (SetCommState(handle, &configuration) == FALSE) {
        error = windowsError("impossible de configurer le port serie");
        CloseHandle(handle);
        return false;
    }
    if (SetupComm(handle, 256U, 256U) == FALSE) {
        error = windowsError("impossible de dimensionner les tampons serie");
        CloseHandle(handle);
        return false;
    }

    handle_ = handle;
    if (!clearInput(error)) {
        close();
        return false;
    }
    error.clear();
    return true;
#else
    (void)portName;
    (void)baudRate;
    error = "liaison serie USB disponible uniquement sous Windows";
    return false;
#endif
}

void SerialSettingsChannel::close() noexcept {
#if defined(_WIN32)
    if (handle_ != nullptr) {
        CloseHandle(nativeHandle(handle_));
        handle_ = nullptr;
    }
#else
    handle_ = nullptr;
#endif
}

bool SerialSettingsChannel::clearInput(std::string& error) {
#if defined(_WIN32)
    if (!isOpen()) {
        error = "port serie non ouvert";
        return false;
    }
    if (PurgeComm(
            nativeHandle(handle_),
            PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR) ==
        FALSE) {
        error = windowsError("impossible de purger le port serie");
        return false;
    }
    error.clear();
    return true;
#else
    error = "liaison serie USB disponible uniquement sous Windows";
    return false;
#endif
}

bool SerialSettingsChannel::writeAll(
    const std::uint8_t* const data,
    const std::size_t size,
    const std::uint32_t timeoutMs,
    std::string& error) {
#if defined(_WIN32)
    if (!isOpen() || (data == nullptr && size != 0U) || timeoutMs == 0U ||
        size > std::numeric_limits<DWORD>::max()) {
        error = "parametres d'ecriture serie invalides";
        return false;
    }

    const Clock::time_point startedAt = Clock::now();
    std::size_t offset = 0U;
    while (offset < size) {
        const std::uint32_t remaining =
            remainingMilliseconds(startedAt, timeoutMs);
        if (remaining == 0U ||
            !setTimeouts(nativeHandle(handle_), 0U, remaining, error)) {
            if (error.empty()) {
                error = "delai d'ecriture serie depasse";
            }
            return false;
        }

        DWORD written = 0U;
        const DWORD requested = static_cast<DWORD>(size - offset);
        if (WriteFile(
                nativeHandle(handle_),
                data + offset,
                requested,
                &written,
                nullptr) == FALSE) {
            error = windowsError("echec d'ecriture serie");
            return false;
        }
        if (written == 0U) {
            error = "delai d'ecriture serie depasse";
            return false;
        }
        offset += static_cast<std::size_t>(written);
    }
    error.clear();
    return true;
#else
    (void)data;
    (void)size;
    (void)timeoutMs;
    error = "liaison serie USB disponible uniquement sous Windows";
    return false;
#endif
}

SettingsChannelReadStatus SerialSettingsChannel::readByte(
    std::uint8_t& byte,
    const std::uint32_t timeoutMs,
    std::string& error) {
#if defined(_WIN32)
    if (!isOpen() || timeoutMs == 0U) {
        error = "parametres de lecture serie invalides";
        return SettingsChannelReadStatus::Failure;
    }
    if (!setTimeouts(nativeHandle(handle_), timeoutMs, 0U, error)) {
        return SettingsChannelReadStatus::Failure;
    }

    DWORD received = 0U;
    if (ReadFile(
            nativeHandle(handle_),
            &byte,
            1U,
            &received,
            nullptr) == FALSE) {
        error = windowsError("echec de lecture serie");
        return SettingsChannelReadStatus::Failure;
    }
    if (received == 0U) {
        error.clear();
        return SettingsChannelReadStatus::Timeout;
    }
    if (received != 1U) {
        error = "taille de lecture serie inattendue";
        return SettingsChannelReadStatus::Failure;
    }
    error.clear();
    return SettingsChannelReadStatus::Data;
#else
    (void)byte;
    (void)timeoutMs;
    error = "liaison serie USB disponible uniquement sous Windows";
    return SettingsChannelReadStatus::Failure;
#endif
}

std::vector<std::string> listSerialPorts(std::string& error) {
    std::vector<std::string> ports{};
#if defined(_WIN32)
    char target[512]{};
    for (unsigned int number = 1U; number <= 256U; ++number) {
        const std::string name = "COM" + std::to_string(number);
        SetLastError(ERROR_SUCCESS);
        const DWORD result = QueryDosDeviceA(
            name.c_str(), target, static_cast<DWORD>(sizeof(target)));
        if (result != 0U || GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            ports.push_back(name);
        }
    }
    error.clear();
#else
    error = "detection des ports disponible uniquement sous Windows";
#endif
    return ports;
}

}  // namespace bmw::remote::host
