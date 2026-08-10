#include "standard_tools/audit/record.hpp"

#include <openssl/evp.h>

#include <iomanip>
#include <sstream>

namespace standard_tools::audit {

namespace {

ClockFn g_clock = []() { return std::chrono::system_clock::now(); };

std::string ToHex(const unsigned char* data, std::size_t len) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return oss.str();
}

}  // namespace

std::string HashBytes(const std::string& bytes) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, bytes.data(), bytes.size());
    EVP_DigestFinal_ex(ctx, hash, &len);
    EVP_MD_CTX_free(ctx);
    return ToHex(hash, len);
}

std::string HashJson(const json& value) {
    return HashBytes(value.dump());
}

std::string HashRecord(const DecisionRecord& record) {
    DecisionRecord copy = record;
    copy.record_hash = "";
    return HashJson(json(copy));
}

std::chrono::system_clock::time_point Now() {
    return g_clock();
}

void SetClock(ClockFn fn) {
    g_clock = std::move(fn);
}

void ResetClock() {
    g_clock = []() { return std::chrono::system_clock::now(); };
}

}  // namespace standard_tools::audit
