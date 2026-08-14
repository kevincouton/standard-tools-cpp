#pragma once

#include "standard_tools/core/value_objects.hpp"

#include <chrono>
#include <cstddef>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace standard_tools::backtest {

using json = nlohmann::json;

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

inline std::string ToString(TradeSide side) {
    switch (side) {
        case TradeSide::Long:
            return "long";
        case TradeSide::Short:
            return "short";
    }
    return "unknown";
}

inline void to_json(json& j, const EquityPoint& p) {
    j = json::object();
    j["date"] = core::FormatDate(p.date);
    j["equity"] = p.equity;
}

inline void to_json(json& j, const Trade& t) {
    j = json::object();
    j["entry_date"] = core::FormatDate(t.entry_date);
    j["exit_date"] = core::FormatDate(t.exit_date);
    j["entry_price"] = t.entry_price;
    j["exit_price"] = t.exit_price;
    j["quantity"] = t.quantity;
    j["side"] = ToString(t.side);
    j["pnl"] = t.pnl;
}

inline void to_json(json& j, const Metrics& m) {
    j = json::object();
    j["max_drawdown"] = m.max_drawdown;
    j["sharpe"] = m.sharpe.has_value() ? json(*m.sharpe) : json(nullptr);
    j["win_rate"] = m.win_rate;
    j["trade_count"] = m.trade_count;
}

inline void to_json(json& j, const BacktestResult& r) {
    j = json::object();
    j["final_equity"] = r.final_equity;
    j["total_return"] = r.total_return;
    j["equity_curve"] = r.equity_curve;
    j["trades"] = r.trades;
    j["metrics"] = r.metrics;
}

inline void to_json(json& j, const ConfidenceInterval& ci) {
    j = json::object();
    j["lower"] = ci.lower;
    j["upper"] = ci.upper;
}

inline void to_json(json& j, const MonteCarloResult& r) {
    j = json::object();
    j["simulations"] = r.simulations;
    j["final_equity_ci"] = r.final_equity_ci;
    j["max_drawdown_ci"] = r.max_drawdown_ci;
    j["initial_capital"] = r.initial_capital;
}

inline void to_json(json& j, const ParamWindow& w) {
    j = json::object();
    j["start"] = core::FormatDate(w.start);
    j["params"] = w.params;
}

inline void to_json(json& j, const WalkForwardResult& r) {
    j = json::object();
    j["equity_curve"] = r.equity_curve;
    j["trades"] = r.trades;
    j["total_return"] = r.total_return;
    j["max_drawdown"] = r.max_drawdown;
    j["sharpe"] = r.sharpe.has_value() ? json(*r.sharpe) : json(nullptr);
    j["number_of_trades"] = r.number_of_trades;
    j["win_rate"] = r.win_rate;
    j["selected_params"] = r.selected_params;
}

}  // namespace standard_tools::backtest
