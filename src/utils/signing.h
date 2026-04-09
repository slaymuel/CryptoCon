#pragma once

#include <string>

/**
 * @brief Generate HMAC-SHA256 signature for API authentication
 * 
 * Creates a signature using HMAC-SHA256
 * 
 * @param key Secret API key
 * @param data Query string or data to be signed
 * @return Hexadecimal string representation of the HMAC-SHA256 signature
 */
const std::string signHMAC(const std::string& key, const std::string& data);