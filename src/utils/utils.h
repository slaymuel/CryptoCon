#pragma once

#include <chrono>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <string>
#include <iomanip>
#include <sstream>

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

inline uint64_t currentTimestamp() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}