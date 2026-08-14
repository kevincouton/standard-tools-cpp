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
        ToolDefinition{
            .name = ToolCalculateIndicator,
            .description = "Calculate a technical indicator for a ticker.",
            .parameters = json::parse(
                R"({"type":"object","properties":{)"
                R"("indicator":{"type":"string"},)"
                R"("ticker":{"type":"string"},)"
                R"("start":{"type":"string","format":"date"},)"
                R"("end":{"type":"string","format":"date"},)"
                R"("interval":{"type":"string","enum":["daily","weekly","monthly"]},)"
                R"("params":{"type":"object"}},)"
                R"("required":["indicator","ticker","start","end"]})"),
        },
        ToolDefinition{
            .name = ToolCalculateReturnMetrics,
            .description = "Calculate return metrics for a ticker's close prices.",
            .parameters = json::parse(
                R"({"type":"object","properties":{)"
                R"("ticker":{"type":"string"},)"
                R"("start":{"type":"string","format":"date"},)"
                R"("end":{"type":"string","format":"date"},)"
                R"("interval":{"type":"string","enum":["daily","weekly","monthly"]},)"
                R"("risk_free_rate":{"type":"number"}},)"
                R"("required":["ticker","start","end"]})"),
        },
        ToolDefinition{
            .name = ToolCalculateRiskMetrics,
            .description = "Calculate risk metrics for a ticker's close prices.",
            .parameters = json::parse(
                R"({"type":"object","properties":{)"
                R"("ticker":{"type":"string"},)"
                R"("start":{"type":"string","format":"date"},)"
                R"("end":{"type":"string","format":"date"},)"
                R"("interval":{"type":"string","enum":["daily","weekly","monthly"]},)"
                R"("risk_free_rate":{"type":"number"}},)"
                R"("required":["ticker","start","end"]})"),
        },
        ToolDefinition{
            .name = ToolRunAnalysis,
            .description = "Run an analysis operation (regression, cointegration, hurst, pca, correlation, options).",
            .parameters = json::parse(
                R"({"type":"object","properties":{)"
                R"("operation":{"type":"string","enum":["regression","cointegration","hurst","pca","correlation","options"]},)"
                R"("request":{"type":"object"}},)"
                R"("required":["operation","request"]})"),
        },
        ToolDefinition{
            .name = ToolRunBacktest,
            .description = "Run a backtest strategy over a ticker.",
            .parameters = json::parse(
                R"({"type":"object","properties":{)"
                R"("strategy":{"type":"string"},)"
                R"("ticker":{"type":"string"},)"
                R"("start":{"type":"string","format":"date"},)"
                R"("end":{"type":"string","format":"date"},)"
                R"("interval":{"type":"string","enum":["daily","weekly","monthly"]},)"
                R"("params":{"type":"object"},)"
                R"("config":{"type":"object"}},)"
                R"("required":["strategy","ticker","start","end"]})"),
        },
        ToolDefinition{
            .name = ToolOptimizePortfolio,
            .description = "Run mean-variance portfolio optimization.",
            .parameters = json::parse(
                R"({"type":"object","properties":{)"
                R"("request":{"type":"object"}},)"
                R"("required":["request"]})"),
        },
        ToolDefinition{
            .name = ToolRiskParityPortfolio,
            .description = "Run inverse-volatility risk-parity portfolio allocation.",
            .parameters = json::parse(
                R"({"type":"object","properties":{)"
                R"("request":{"type":"object"}},)"
                R"("required":["request"]})"),
        },
        ToolDefinition{
            .name = ToolBlackLittermanPortfolio,
            .description = "Run simplified Black-Litterman portfolio optimization.",
            .parameters = json::parse(
                R"({"type":"object","properties":{)"
                R"("request":{"type":"object"}},)"
                R"("required":["request"]})"),
        },
        ToolDefinition{
            .name = ToolScreenStocks,
            .description = "Screen a universe of tickers by fundamental criteria.",
            .parameters = json::parse(
                R"({"type":"object","properties":{)"
                R"("tickers":{"type":"array","items":{"type":"string"}},)"
                R"("criteria":{"type":"object"}},)"
                R"("required":["tickers"]})"),
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
