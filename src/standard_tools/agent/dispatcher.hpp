#pragma once

#include "standard_tools/agent/tool.hpp"
#include "standard_tools/analysis/calculator.hpp"
#include "standard_tools/indicators/calculator.hpp"
#include "standard_tools/marketdata/service.hpp"
#include "standard_tools/metrics/risk_return.hpp"
#include "standard_tools/screener/service.hpp"

#include <memory>

namespace standard_tools::agent {

class Dispatcher {
public:
    Dispatcher(
        std::shared_ptr<marketdata::Service> market_data,
        std::shared_ptr<indicators::IndicatorCalculator> indicators,
        std::shared_ptr<metrics::RiskReturnCalculator> metrics,
        std::shared_ptr<analysis::AnalysisCalculator> analysis,
        std::shared_ptr<screener::Screener> screener);

    ToolResult Dispatch(const ToolCall& call);

private:
    ToolResult FetchOhlcv(const json& args);
    ToolResult CalculateIndicator(const json& args);
    ToolResult CalculateReturnMetrics(const json& args);
    ToolResult CalculateRiskMetrics(const json& args);
    ToolResult RunAnalysis(const json& args);
    ToolResult RunBacktest(const json& args);
    ToolResult OptimizePortfolio(const json& args);
    ToolResult RiskParityPortfolio(const json& args);
    ToolResult BlackLittermanPortfolio(const json& args);
    ToolResult ScreenStocks(const json& args);

    std::shared_ptr<marketdata::Service> market_data_;
    std::shared_ptr<indicators::IndicatorCalculator> indicators_;
    std::shared_ptr<metrics::RiskReturnCalculator> metrics_;
    std::shared_ptr<analysis::AnalysisCalculator> analysis_;
    std::shared_ptr<screener::Screener> screener_;
};

}  // namespace standard_tools::agent
