/**
 * @file utils.h
 * @brief Cryptographic and time utility functions
 * 
 * Provides essential utility functions for API authentication (HMAC-SHA256 signing)
 * and timestamp generation for exchange API requests.
 */

#pragma once

#include <chrono>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <string>
#include <iomanip>
#include <sstream>

/**
 * @brief Generate HMAC-SHA256 signature for API authentication
 * 
 * Creates asignature using HMAC-SHA256
 * 
 * @param key Secret API key
 * @param data Query string or data to be signed
 * @return Hexadecimal string representation of the HMAC-SHA256 signature
 */
inline const std::string sign(const std::string& key, const std::string& data) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;

    HMAC(EVP_sha256(),
         key.data(), key.size(),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         hash, &len);

    std::ostringstream oss;
    for (unsigned int i = 0; i < len; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];

    return oss.str();
}

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
