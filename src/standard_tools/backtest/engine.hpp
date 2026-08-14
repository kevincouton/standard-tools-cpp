#pragma once

#include "standard_tools/backtest/result.hpp"
#include "standard_tools/backtest/strategy.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace standard_tools::backtest {

/// Vectorized backtest engine with next-bar-open execution.
class BacktestEngine {
public:
    /// Creates an engine for the named built-in strategy.
    BacktestEngine(const std::string& strategy_name, BacktestConfig config);

    /// Creates an engine with a custom strategy.
    BacktestEngine(std::unique_ptr<Strategy> strategy, BacktestConfig config);

    /// Runs the strategy over the supplied price series using the provided
    /// parameters. Signals generated on bar i are executed at the open of bar
    /// i+1. Any open position is closed at the last bar's close.
    BacktestResult Run(
        const std::vector<core::OHLCV>& series,
        const std::unordered_map<std::string, double>& params) const;

    const Strategy& GetStrategy() const { return *strategy_; }
    const BacktestConfig& GetConfig() const noexcept { return config_; }

private:
    std::unique_ptr<Strategy> strategy_;
    BacktestConfig config_;
};

}  // namespace standard_tools::backtest
