#pragma once

#include <optional>
#include <string>
#include <vector>

namespace standard_tools::config {

struct PolygonConfig {
    std::string api_key;
};

struct Config {
    int http_port = 8080;
    int grpc_port = 50051;
    std::string log_level = "info";
    std::string database_url;
    std::string cache_dir;
    std::string audit_dir;
    // API-key auth for the REST server; fails closed when enabled without a key.
    bool auth_enabled = true;
    std::string api_key;
    PolygonConfig polygon;
};

// Load configuration from optional TOML files and environment variables.
// Precedence (lowest to highest): defaults < TOML files < env vars.
// Nested keys in env vars use double underscores, e.g. SQT_POLYGON__API_KEY.
Config Load(const std::vector<std::string>& paths = {});

}  // namespace standard_tools::config
