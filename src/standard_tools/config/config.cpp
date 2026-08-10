#include "standard_tools/config/config.hpp"

#include <toml++/toml.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <vector>

namespace standard_tools::config {

namespace {

std::string Trim(const std::string& s) {
    auto start = std::find_if_not(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
    auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) { return std::isspace(c); }).base();
    if (start >= end) return "";
    return std::string(start, end);
}

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::optional<std::string> GetEnv(const std::string& key) {
    const char* val = std::getenv(key.c_str());
    if (!val) return std::nullopt;
    return std::string(val);
}

void ApplyEnv(Config& cfg) {
    struct EnvMapping {
        const char* key;
        std::function<void(const std::string&)> apply;
    };

    std::vector<EnvMapping> mappings = {
        {"SQT_HTTP_PORT", [&](const std::string& v) { cfg.http_port = std::stoi(v); }},
        {"SQT_GRPC_PORT", [&](const std::string& v) { cfg.grpc_port = std::stoi(v); }},
        {"SQT_LOG_LEVEL", [&](const std::string& v) { cfg.log_level = v; }},
        {"SQT_DATABASE_URL", [&](const std::string& v) { cfg.database_url = v; }},
        {"SQT_CACHE_DIR", [&](const std::string& v) { cfg.cache_dir = v; }},
        {"SQT_AUDIT_DIR", [&](const std::string& v) { cfg.audit_dir = v; }},
        {"SQT_POLYGON__API_KEY", [&](const std::string& v) { cfg.polygon.api_key = v; }},
    };

    for (const auto& mapping : mappings) {
        if (auto value = GetEnv(mapping.key)) {
            mapping.apply(*value);
        }
    }
}

void ApplyToml(Config& cfg, const std::string& path) {
    toml::table tbl;
    try {
        tbl = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        throw std::runtime_error(std::string("failed to parse ") + path + ": " + e.what());
    }

    if (auto v = tbl["http_port"].value<int64_t>()) cfg.http_port = static_cast<int>(*v);
    if (auto v = tbl["grpc_port"].value<int64_t>()) cfg.grpc_port = static_cast<int>(*v);
    if (auto v = tbl["log_level"].value<std::string>()) cfg.log_level = *v;
    if (auto v = tbl["database_url"].value<std::string>()) cfg.database_url = *v;
    if (auto v = tbl["cache_dir"].value<std::string>()) cfg.cache_dir = *v;
    if (auto v = tbl["audit_dir"].value<std::string>()) cfg.audit_dir = *v;
    if (auto poly = tbl["polygon"].as_table()) {
        if (auto v = (*poly)["api_key"].value<std::string>()) cfg.polygon.api_key = *v;
    }
}

}  // namespace

Config Load(const std::vector<std::string>& paths) {
    Config cfg;

    for (const auto& path : paths) {
        ApplyToml(cfg, path);
    }

    ApplyEnv(cfg);

    cfg.log_level = Trim(ToLower(cfg.log_level));
    if (cfg.log_level.empty()) cfg.log_level = "info";
    return cfg;
}

}  // namespace standard_tools::config
