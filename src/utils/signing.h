#pragma once

#include <string>

namespace trade_connector {
/// Generate HMAC-SHA256 hex signature for API authentication.
const std::string signHMAC(const std::string& key, const std::string& data);

}