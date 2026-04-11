/// @file utils.h
/// @brief Time utilities, logging helpers, and string helpers.

#pragma once

#include <chrono>
#include <string>

namespace trade_connector {

inline void null_logger(const std::string&) {}

/// Current Unix timestamp in milliseconds.
inline uint64_t currentTimeMillis() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

/// Append numeric value to string via std::to_chars (no allocation).
template<typename T>
void appendNumber(std::string& str, const T& value) {
    char buffer[32]; 
    auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (ec == std::errc{}) {
        str.append(buffer, ptr - buffer);
    }
}

inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

}