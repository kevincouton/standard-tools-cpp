#pragma once

#include "standard_tools/core/value_objects.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace standard_tools::backtest {

/// A single point on the equity curve.
struct EquityPoint {
    core::Date date;
    double equity = 0.0;
};

/// Side of a completed trade.
enum class TradeSide {
    Long,
    Short,
};

/// A completed round-trip trade.
struct Trade {
    core::Date entry_date;
    core::Date exit_date;
    double entry_price = 0.0;
    double exit_price = 0.0;
    double quantity = 0.0;
    TradeSide side = TradeSide::Long;
    double pnl = 0.0;
};

/// Performance metrics derived from a backtest run.
struct Metrics {
    /// Maximum peak-to-trough drawdown as a negative fraction.
    double max_drawdown = 0.0;
    /// Annualised Sharpe ratio, if computable.
    std::optional<double> sharpe;
    /// Fraction of completed trades with positive PnL.
    double win_rate = 0.0;
    /// Number of completed round-trip trades.
    std::size_t trade_count = 0;
};

/// Result of a single backtest run.
struct BacktestResult {
    /// Final account value after the last bar.
    double final_equity = 0.0;
    /// Fractional return over the backtest period.
    double total_return = 0.0;
    /// Daily equity trajectory aligned to the input series.
    std::vector<EquityPoint> equity_curve;
    /// Completed round-trip trades.
    std::vector<Trade> trades;
    /// Derived performance statistics.
    Metrics metrics;
};

/// Configuration for a single backtest run.
struct BacktestConfig {
    /// Starting cash balance.
    double initial_capital = 100'000.0;
    /// Fractional commission paid per side (e.g. 0.001 for 10 bps).
    double commission_rate = 0.0;
    /// Number of bars per year used to annualize the Sharpe ratio.
    std::size_t periods_per_year = 252;
    /// Annualized risk-free rate used by the Sharpe ratio.
    double risk_free_rate = 0.0;
};

/// Confidence interval for a Monte Carlo metric.
struct ConfidenceInterval {
    double lower = 0.0;
    double upper = 0.0;
};

/// Result of a Monte Carlo simulation.
struct MonteCarloResult {
    int simulations = 0;
    ConfidenceInterval final_equity_ci;
    ConfidenceInterval max_drawdown_ci;
    double initial_capital = 0.0;
};

/// Parameter set selected for an out-of-sample window.
struct ParamWindow {
    core::Date start;
    std::unordered_map<std::string, double> params;
};

/// Result of a walk-forward optimization run.
struct WalkForwardResult {
    std::vector<EquityPoint> equity_curve;
    std::vector<Trade> trades;
    double total_return = 0.0;
    double max_drawdown = 0.0;
    std::optional<double> sharpe;
    std::size_t number_of_trades = 0;
    double win_rate = 0.0;
    std::vector<ParamWindow> selected_params;
};

/// Objective used by walk-forward optimization.
enum class OptimizationMetric {
    TotalReturn,
    Sharpe,
    WinRate,
};

}  // namespace standard_tools::backtest
