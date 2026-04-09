/**
 * @file utils.h
 * @brief Cryptographic and time utility functions
 * 
 * Provides essential utility functions for API authentication (HMAC-SHA256 signing)
 * and timestamp generation for exchange API requests.
 */

#pragma once

#include <chrono>
#include <string>

/**
 * @brief Get current Unix timestamp in milliseconds
 * 
 * Returns the current system time as milliseconds since Unix epoch (January 1, 1970).
 * 
 * @return Current timestamp in milliseconds since epoch
 */
inline uint64_t currentTimestamp() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

/**
 * @brief Append number to string using stack buffer and std::to_chars
 */
template<typename T>
void appendNumber(std::string& str, T value) {
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