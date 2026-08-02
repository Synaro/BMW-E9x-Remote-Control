#include "tools/can_trace_csv.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <istream>
#include <limits>
#include <string>
#include <system_error>

namespace bmw::remote::host {
namespace {

constexpr const char* Header = "timestamp_ms,identifier,extended,dlc,data_hex";

void setError(
    std::string& error,
    const std::size_t lineNumber,
    const char* const message) {
    error = "line " + std::to_string(lineNumber) + ": " + message;
}

[[nodiscard]] bool splitFields(
    const std::string& line,
    std::array<std::string, 5U>& fields) {
    std::size_t start = 0U;

    for (std::size_t index = 0U; index < fields.size() - 1U; ++index) {
        const std::size_t separator = line.find(',', start);
        if (separator == std::string::npos) {
            return false;
        }
        fields[index] = line.substr(start, separator - start);
        start = separator + 1U;
    }

    if (line.find(',', start) != std::string::npos) {
        return false;
    }
    fields.back() = line.substr(start);
    return true;
}

template <typename T>
[[nodiscard]] bool parseUnsigned(
    const std::string& text,
    const int base,
    T& output) {
    if (text.empty()) {
        return false;
    }

    std::uint64_t parsed = 0U;
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto result = std::from_chars(begin, end, parsed, base);

    if (result.ec != std::errc{} || result.ptr != end ||
        parsed > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
        return false;
    }

    output = static_cast<T>(parsed);
    return true;
}

[[nodiscard]] bool parseIdentifier(
    const std::string& text,
    std::uint32_t& identifier) {
    if (text.size() <= 2U || text[0] != '0' || (text[1] != 'x' && text[1] != 'X')) {
        return false;
    }
    return parseUnsigned(text.substr(2U), 16, identifier);
}

[[nodiscard]] bool parseBoolean(const std::string& text, bool& value) {
    if (text == "1" || text == "true") {
        value = true;
        return true;
    }
    if (text == "0" || text == "false") {
        value = false;
        return true;
    }
    return false;
}

[[nodiscard]] int hexNibble(const char character) noexcept {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

[[nodiscard]] bool parseData(
    const std::string& text,
    const std::uint8_t dataLength,
    infrastructure::CanFrame& frame) noexcept {
    if (text.size() != static_cast<std::size_t>(dataLength) * 2U) {
        return false;
    }

    for (std::size_t index = 0U; index < dataLength; ++index) {
        const int high = hexNibble(text[index * 2U]);
        const int low = hexNibble(text[index * 2U + 1U]);
        if (high < 0 || low < 0) {
            return false;
        }
        frame.data[index] = static_cast<std::uint8_t>((high << 4U) | low);
    }
    return true;
}

[[nodiscard]] bool parseFrame(
    const std::array<std::string, 5U>& fields,
    infrastructure::CanFrame& frame) {
    if (!parseUnsigned(fields[0], 10, frame.timestampMs) ||
        !parseIdentifier(fields[1], frame.identifier) ||
        !parseBoolean(fields[2], frame.extended) ||
        !parseUnsigned(fields[3], 10, frame.dataLength) ||
        !parseData(fields[4], frame.dataLength, frame)) {
        return false;
    }
    return frame.isValid();
}

}  // namespace

bool parseCanonicalCanTrace(
    std::istream& input,
    std::vector<infrastructure::CanFrame>& frames,
    const std::size_t maximumFrames,
    std::string& error) {
    std::vector<infrastructure::CanFrame> parsedFrames{};
    std::string line{};
    std::size_t lineNumber = 0U;

    error.clear();
    if (maximumFrames == 0U) {
        error = "maximum frame count must be greater than zero";
        return false;
    }

    if (!std::getline(input, line)) {
        error = "trace is empty";
        return false;
    }
    ++lineNumber;
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    if (line == std::string{"\xEF\xBB\xBF"} + Header) {
        line.erase(0U, 3U);
    }
    if (line != Header) {
        setError(error, lineNumber, "invalid header");
        return false;
    }

    while (std::getline(input, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (parsedFrames.size() >= maximumFrames) {
            setError(error, lineNumber, "maximum frame count exceeded");
            return false;
        }

        std::array<std::string, 5U> fields{};
        infrastructure::CanFrame frame{};
        if (!splitFields(line, fields) || !parseFrame(fields, frame)) {
            setError(error, lineNumber, "invalid CAN frame");
            return false;
        }
        if (parsedFrames.empty() && frame.timestampMs != 0U) {
            setError(error, lineNumber, "first timestamp must be zero");
            return false;
        }
        if (!parsedFrames.empty() &&
            frame.timestampMs < parsedFrames.back().timestampMs) {
            setError(error, lineNumber, "timestamps must be monotonic");
            return false;
        }
        parsedFrames.push_back(frame);
    }

    if (parsedFrames.empty()) {
        error = "trace contains no CAN frames";
        return false;
    }

    frames.swap(parsedFrames);
    return true;
}

bool loadCanonicalCanTrace(
    const char* const path,
    std::vector<infrastructure::CanFrame>& frames,
    const std::size_t maximumFrames,
    std::string& error) {
    if (path == nullptr || path[0] == '\0') {
        error = "trace path is empty";
        return false;
    }

    std::ifstream input{path};
    if (!input.is_open()) {
        error = "unable to open trace file";
        return false;
    }
    return parseCanonicalCanTrace(input, frames, maximumFrames, error);
}

}  // namespace bmw::remote::host
