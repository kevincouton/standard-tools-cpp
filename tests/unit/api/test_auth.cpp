#include "standard_tools/api/auth.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace standard_tools::api;

TEST_CASE("Auth disabled allows every request", "[api][auth]") {
    const AuthConfig cfg{.enabled = false, .api_key = ""};
    REQUIRE(IsAuthorized(cfg, "/api/v1/agent/dispatch", ""));
}

TEST_CASE("Health endpoint is exempt from auth", "[api][auth]") {
    const AuthConfig cfg{.enabled = true, .api_key = "secret"};
    REQUIRE(IsAuthorized(cfg, "/health", ""));
}

TEST_CASE("Enabled auth with a configured key checks the presented key", "[api][auth]") {
    const AuthConfig cfg{.enabled = true, .api_key = "secret"};
    REQUIRE(IsAuthorized(cfg, "/api/v1/agent/dispatch", "secret"));
    REQUIRE(!IsAuthorized(cfg, "/api/v1/agent/dispatch", "wrong"));
    REQUIRE(!IsAuthorized(cfg, "/api/v1/agent/dispatch", ""));
}

TEST_CASE("Enabled auth without a configured key fails closed", "[api][auth]") {
    const AuthConfig cfg{.enabled = true, .api_key = ""};
    REQUIRE(!IsAuthorized(cfg, "/api/v1/agent/dispatch", ""));
    REQUIRE(!IsAuthorized(cfg, "/api/v1/agent/dispatch", "anything"));
    // The exemption still applies so health checks keep working.
    REQUIRE(IsAuthorized(cfg, "/health", ""));
}
