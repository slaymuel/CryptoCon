#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <string>
#include <iomanip>
#include <sstream>
#include "signing.h"

namespace trade_connector{

const std::string signHMAC(const std::string& key, const std::string& data) {
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

}