#include "standard_tools/agent/tool.hpp"

namespace standard_tools::agent {

std::vector<ToolDefinition> ListTools() {
    return {
        ToolDefinition{
            .name = ToolHealth,
            .description = "Return agent health status.",
            .parameters = json::parse(R"({"type":"object","properties":{}})"),
        },
        ToolDefinition{
            .name = ToolListTools,
            .description = "List all registered tool names.",
            .parameters = json::parse(R"({"type":"object","properties":{}})"),
        },
        ToolDefinition{
            .name = ToolFetchOhlcv,
            .description = "Fetch OHLCV bars for a single ticker.",
            .parameters = json::parse(
                R"({"type":"object","properties":{)"
                R"("ticker":{"type":"string"},)"
                R"("start":{"type":"string","format":"date"},)"
                R"("end":{"type":"string","format":"date"},)"
                R"("interval":{"type":"string","enum":["daily","weekly","monthly"]},)"
                R"("provider":{"type":"string"}},)"
                R"("required":["ticker","start","end"]})"),
        },
    };
}

std::optional<ToolDefinition> FindTool(const std::string& name) {
    for (const auto& tool : ListTools()) {
        if (tool.name == name) {
            return tool;
        }
    }
    return std::nullopt;
}

}  // namespace standard_tools::agent
