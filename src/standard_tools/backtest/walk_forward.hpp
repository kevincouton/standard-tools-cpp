#pragma once

#include "standard_tools/backtest/engine.hpp"
#include "standard_tools/backtest/result.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace standard_tools::backtest {

/// Request for walk-forward optimization.
struct WalkForwardRequest {
    std::string strategy;
    std::vector<core::OHLCV> series;
    std::unordered_map<std::string, std::vector<double>> param_grid;
    std::size_t train_size = 0;
    std::size_t test_size = 0;
    OptimizationMetric metric = OptimizationMetric::TotalReturn;
    BacktestConfig config;
};

/// Walk-forward optimizer.
class WalkForwardOptimizer {
public:
    explicit WalkForwardOptimizer(WalkForwardRequest request);

    /// Runs walk-forward optimization over the supplied series.
    WalkForwardResult Run() const;

    const WalkForwardRequest& GetRequest() const noexcept { return request_; }

private:
    WalkForwardRequest request_;
};

}  // namespace standard_tools::backtest
