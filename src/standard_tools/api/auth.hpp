#pragma once

#include <crow.h>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <utility>

namespace standard_tools::api {

struct AuthConfig {
    bool enabled = true;
    std::string api_key;
};

/// Process-wide auth settings, configured once at startup from Config.
inline AuthConfig g_auth_config;

inline void ConfigureAuth(bool enabled, std::string api_key) {
    g_auth_config.enabled = enabled;
    g_auth_config.api_key = std::move(api_key);
}

/// Upper bound on incoming request bodies (16 MiB).
constexpr std::size_t kMaxBodySize = 16ULL * 1024 * 1024;

/// Constant-time equality for API-key comparison.
inline bool ConstantTimeEqual(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    volatile std::uint8_t diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<std::uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
}

/// Returns true when a request to \p url presenting \p presented_key is
/// authorized under \p cfg. Fails closed: when auth is enabled but no key is
/// configured, every non-exempt request is rejected.
inline bool IsAuthorized(
    const AuthConfig& cfg, const std::string& url, const std::string& presented_key) {
    if (!cfg.enabled) {
        return true;
    }
    if (url == "/health") {
        return true;
    }
    if (cfg.api_key.empty()) {
        return false;
    }
    return ConstantTimeEqual(presented_key, cfg.api_key);
}

/// Crow middleware enforcing API-key auth.
struct ApiKeyAuth {
    struct context {};

    void before_handle(crow::request& req, crow::response& res, context&) {
        if (!IsAuthorized(g_auth_config, req.url, req.get_header_value("X-API-Key"))) {
            res.code = 401;
            res.set_header("Content-Type", "application/json");
            res.body = nlohmann::json{{"error", "unauthorized"}, {"code", 401}}.dump();
            res.end();
        }
    }

    void after_handle(crow::request&, crow::response&, context&) {}
};

/// Crow middleware rejecting request bodies that exceed the size limit.
struct PayloadLimitMiddleware {
    struct context {};

    void before_handle(crow::request& req, crow::response& res, context&) {
        if (req.body.size() > kMaxBodySize) {
            res.code = 413;
            res.set_header("Content-Type", "application/json");
            res.body = nlohmann::json{
                {"error", "request body too large"},
                {"code", 413},
                {"max_bytes", kMaxBodySize}
            }.dump();
            res.end();
        }
    }

    void after_handle(crow::request&, crow::response&, context&) {}
};

/// Application type with auth and payload-limit middleware installed.
using App = crow::App<ApiKeyAuth, PayloadLimitMiddleware>;

}  // namespace standard_tools::api
