#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace standard_tools::agent {

using json = nlohmann::json;

struct ToolDefinition {
    std::string name;
    std::string description;
    json parameters;
};

inline void to_json(json& j, const ToolDefinition& t) {
    j = json{
        {"name", t.name},
        {"description", t.description},
        {"parameters", t.parameters},
    };
}

inline void from_json(const json& j, ToolDefinition& t) {
    j.at("name").get_to(t.name);
    j.at("description").get_to(t.description);
    j.at("parameters").get_to(t.parameters);
}

struct ToolCall {
    std::string name;
    json arguments;
};

struct ToolResult {
    json output;
};

inline ToolResult OkResult(json output) {
    return ToolResult{.output = std::move(output)};
}

inline ToolResult ErrorResult(const std::string& message) {
    return ToolResult{.output = json{{"error", message}}};
}

constexpr const char* ToolHealth = "health";
constexpr const char* ToolListTools = "list_tools";
constexpr const char* ToolFetchOhlcv = "fetch_ohlcv";

std::vector<ToolDefinition> ListTools();
std::optional<ToolDefinition> FindTool(const std::string& name);

}  // namespace standard_tools::agent
