#pragma once

#include "standard_tools/core/json_serialization.hpp"
#include "standard_tools/core/value_objects.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace standard_tools::audit {

using json = nlohmann::json;
using Date = core::Date;

struct DecisionRecord {
    std::string request_id;
    Date recorded_at;
    std::string tool_name;
    json input;
    std::string input_hash;
    json output;
    std::string output_hash;
    std::string status;
    std::string error;
    std::string git_commit_sha;
    std::string package_version;
    std::int64_t random_seed = 0;
    std::string prev_record_hash;
    std::string record_hash;
};

inline void to_json(nlohmann::json& j, const DecisionRecord& r) {
    j = nlohmann::json{
        {"request_id", r.request_id},
        {"recorded_at", core::FormatDate(r.recorded_at)},
        {"tool_name", r.tool_name},
        {"input", r.input},
        {"input_hash", r.input_hash},
        {"output", r.output},
        {"output_hash", r.output_hash},
        {"status", r.status},
        {"error", r.error},
        {"git_commit_sha", r.git_commit_sha},
        {"package_version", r.package_version},
        {"random_seed", r.random_seed},
        {"prev_record_hash", r.prev_record_hash},
        {"record_hash", r.record_hash},
    };
}

inline void from_json(const nlohmann::json& j, DecisionRecord& r) {
    j.at("request_id").get_to(r.request_id);
    r.recorded_at = core::ParseDate(j.at("recorded_at").get<std::string>());
    j.at("tool_name").get_to(r.tool_name);
    j.at("input").get_to(r.input);
    j.at("input_hash").get_to(r.input_hash);
    j.at("output").get_to(r.output);
    j.at("output_hash").get_to(r.output_hash);
    j.at("status").get_to(r.status);
    j.at("error").get_to(r.error);
    j.at("git_commit_sha").get_to(r.git_commit_sha);
    j.at("package_version").get_to(r.package_version);
    j.at("random_seed").get_to(r.random_seed);
    j.at("prev_record_hash").get_to(r.prev_record_hash);
    j.at("record_hash").get_to(r.record_hash);
}

// Helpers for stable hashing.
std::string HashBytes(const std::string& bytes);
std::string HashJson(const json& value);
std::string HashRecord(const DecisionRecord& record);

// Clock function so tests can override recorded_at.
using ClockFn = std::function<std::chrono::system_clock::time_point()>;
std::chrono::system_clock::time_point Now();
void SetClock(ClockFn fn);
void ResetClock();

}  // namespace standard_tools::audit
