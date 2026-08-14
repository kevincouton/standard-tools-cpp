#pragma once

#include "standard_tools/agent/dispatcher.hpp"
#include "standard_tools/analysis/calculator.hpp"
#include "standard_tools/audit/writer.hpp"
#include "standard_tools/indicators/calculator.hpp"
#include "standard_tools/marketdata/service.hpp"
#include "standard_tools/metrics/risk_return.hpp"
#include "standard_tools/screener/service.hpp"

#include <memory>

namespace standard_tools::api {

struct AppState {
    std::shared_ptr<agent::Dispatcher> dispatcher;
    std::shared_ptr<marketdata::Service> market_data;
    std::shared_ptr<audit::Writer> audit_writer;
    std::shared_ptr<indicators::IndicatorCalculator> indicators;
    std::shared_ptr<metrics::RiskReturnCalculator> metrics;
    std::shared_ptr<analysis::AnalysisCalculator> analysis;
    std::shared_ptr<screener::Screener> screener;
};

}  // namespace standard_tools::api
