#include "standard_tools/backtest/engine.hpp"

#include "standard_tools/core/errors.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace standard_tools::backtest {

namespace {

struct Position {
    TradeSide side = TradeSide::Long;
    double entry_price = 0.0;
    double quantity = 0.0;
    core::Date entry_date;
};

std::pair<Position, double> OpenLong(
    double price, double cash, double commission, const core::Date& date) {
    double quantity = 0.0;
    if (price != 0.0) {
        quantity = cash / (price * (1.0 + commission));
    }
    double comm = quantity * price * commission;
    return {
        Position{
            .side = TradeSide::Long,
            .entry_price = price,
            .quantity = quantity,
            .entry_date = date},
        cash - quantity * price - comm};
}

std::pair<Position, double> OpenShort(
    double price, double cash, double commission, const core::Date& date) {
    double quantity = 0.0;
    if (price != 0.0) {
        quantity = cash / (price * (1.0 + commission));
    }
    double comm = quantity * price * commission;
    // Short sale proceeds are credited to cash; the borrowed shares are a
    // liability tracked by PositionMarketValue, so cash alone can exceed the
    // account equity while the position is open.
    return {
        Position{
            .side = TradeSide::Short,
            .entry_price = price,
            .quantity = quantity,
            .entry_date = date},
        cash + quantity * price - comm};
}

std::pair<Trade, double> ClosePosition(
    const Position& pos, double price, double commission, const core::Date& date, double cash) {
    double entry_comm = pos.quantity * pos.entry_price * commission;
    double exit_comm = pos.quantity * price * commission;

    double pnl = 0.0;
    double new_cash = 0.0;
    switch (pos.side) {
        case TradeSide::Long:
            pnl = pos.quantity * (price - pos.entry_price) - entry_comm - exit_comm;
            new_cash = cash + pos.quantity * price - exit_comm;
            break;
        case TradeSide::Short:
            pnl = pos.quantity * (pos.entry_price - price) - entry_comm - exit_comm;
            // Entry proceeds were already credited at open; closing only pays
            // the share repurchase cost and the exit commission.
            new_cash = cash - pos.quantity * price - exit_comm;
            break;
    }

    return {
        Trade{
            .entry_date = pos.entry_date,
            .exit_date = date,
            .entry_price = pos.entry_price,
            .exit_price = price,
            .quantity = pos.quantity,
            .side = pos.side,
            .pnl = pnl},
        new_cash};
}

double PositionMarketValue(const Position& pos, double price) {
    switch (pos.side) {
        case TradeSide::Long:
            return pos.quantity * price;
        case TradeSide::Short:
            return -pos.quantity * price;
    }
    return 0.0;
}

struct ComputedMetrics {
    double total_return = 0.0;
    double max_drawdown = 0.0;
    std::optional<double> sharpe;
};

double ComputeMaxDrawdown(const std::vector<EquityPoint>& curve) {
    double peak = 0.0;
    double max_dd = 0.0;
    for (const auto& p : curve) {
        if (p.equity > peak) {
            peak = p.equity;
        }
        if (peak > 0.0) {
            double dd = (peak - p.equity) / peak;
            if (dd > max_dd) {
                max_dd = dd;
            }
        }
    }
    return -max_dd;
}

std::optional<double> ComputeSharpe(
    const std::vector<double>& returns, double risk_free_rate, std::size_t periods_per_year) {
    if (returns.empty() || periods_per_year == 0) {
        return std::nullopt;
    }

    double mean = 0.0;
    for (double r : returns) {
        mean += r;
    }
    mean /= static_cast<double>(returns.size());

    double variance = 0.0;
    for (double r : returns) {
        double d = r - mean;
        variance += d * d;
    }
    variance /= static_cast<double>(returns.size());
    double std = std::sqrt(variance);
    if (std == 0.0 || std::isnan(std)) {
        return std::nullopt;
    }

    double periodic_rf = risk_free_rate / static_cast<double>(periods_per_year);
    double sharpe = (mean - periodic_rf) / std * std::sqrt(static_cast<double>(periods_per_year));
    if (std::isnan(sharpe)) {
        return std::nullopt;
    }
    return sharpe;
}

ComputedMetrics ComputeMetrics(
    const std::vector<EquityPoint>& curve, const BacktestConfig& config) {
    if (curve.empty()) {
        return ComputedMetrics{};
    }

    double initial = curve.front().equity;
    double final = curve.back().equity;
    double total_return = 0.0;
    if (initial != 0.0) {
        total_return = final / initial - 1.0;
    }

    std::vector<double> returns;
    returns.reserve(curve.size() - 1);
    for (std::size_t i = 1; i < curve.size(); ++i) {
        double prev = curve[i - 1].equity;
        double curr = curve[i].equity;
        if (prev == 0.0) {
            returns.push_back(0.0);
        } else {
            returns.push_back(curr / prev - 1.0);
        }
    }

    double max_dd = ComputeMaxDrawdown(curve);
    auto sharpe = ComputeSharpe(returns, config.risk_free_rate, config.periods_per_year);

    return ComputedMetrics{
        .total_return = total_return,
        .max_drawdown = max_dd,
        .sharpe = sharpe};
}

}  // namespace

BacktestEngine::BacktestEngine(const std::string& strategy_name, BacktestConfig config)
    : strategy_(BuiltinStrategy(strategy_name)), config_(config) {
    if (!strategy_) {
        throw core::InvalidCommandError{"unknown strategy: " + strategy_name};
    }
}

BacktestEngine::BacktestEngine(std::unique_ptr<Strategy> strategy, BacktestConfig config)
    : strategy_(std::move(strategy)), config_(std::move(config)) {}

BacktestResult BacktestEngine::Run(
    const std::vector<core::OHLCV>& series,
    const std::unordered_map<std::string, double>& params) const {
    if (series.empty()) {
        throw core::InvalidCommandError{"backtest requires a non-empty price series"};
    }

    auto signals = strategy_->Signals(series, params);
    if (signals.size() != series.size()) {
        throw core::DataQualityError{"strategy signal count does not match series length"};
    }

    double commission = config_.commission_rate;
    double cash = config_.initial_capital;
    std::optional<Position> position;
    std::vector<Trade> trades;
    std::vector<EquityPoint> equity_curve;
    equity_curve.reserve(series.size());

    for (std::size_t i = 0; i < series.size(); ++i) {
        const auto& bar = series[i];

        // Execute the previous bar's signal at this bar's open.
        if (i > 0) {
            switch (signals[i - 1].signal) {
                case Signal::Buy:
                    if (!position.has_value() || position->side != TradeSide::Long) {
                        if (position.has_value()) {
                            auto [t, new_cash] =
                                ClosePosition(position.value(), bar.open, commission, bar.date, cash);
                            cash = new_cash;
                            trades.push_back(t);
                            position = std::nullopt;
                        }
                        auto [p, new_cash] = OpenLong(bar.open, cash, commission, bar.date);
                        cash = new_cash;
                        position = p;
                    }
                    break;
                case Signal::Sell:
                    if (!position.has_value() || position->side != TradeSide::Short) {
                        if (position.has_value()) {
                            auto [t, new_cash] =
                                ClosePosition(position.value(), bar.open, commission, bar.date, cash);
                            cash = new_cash;
                            trades.push_back(t);
                            position = std::nullopt;
                        }
                        auto [p, new_cash] = OpenShort(bar.open, cash, commission, bar.date);
                        cash = new_cash;
                        position = p;
                    }
                    break;
                case Signal::Hold:
                    break;
            }
        }

        // Mark to market at the close.
        double equity = cash;
        if (position.has_value()) {
            equity += PositionMarketValue(position.value(), bar.close);
        }
        equity_curve.push_back(EquityPoint{.date = bar.date, .equity = equity});
    }

    // Close any open position at the last close.
    if (position.has_value()) {
        const auto& last = series.back();
        auto [t, new_cash] = ClosePosition(position.value(), last.close, commission, last.date, cash);
        cash = new_cash;
        trades.push_back(t);
        if (!equity_curve.empty()) {
            equity_curve.back().equity = cash;
        }
        position = std::nullopt;
    }

    auto metrics = ComputeMetrics(equity_curve, config_);
    double win_rate = 0.0;
    if (!trades.empty()) {
        std::size_t wins = 0;
        for (const auto& t : trades) {
            if (t.pnl > 0.0) {
                ++wins;
            }
        }
        win_rate = static_cast<double>(wins) / static_cast<double>(trades.size());
    }

    double final_equity = config_.initial_capital;
    if (!equity_curve.empty()) {
        final_equity = equity_curve.back().equity;
    }

    std::size_t trade_count = trades.size();
    return BacktestResult{
        .final_equity = final_equity,
        .total_return = metrics.total_return,
        .equity_curve = std::move(equity_curve),
        .trades = std::move(trades),
        .metrics = Metrics{
            .max_drawdown = metrics.max_drawdown,
            .sharpe = metrics.sharpe,
            .win_rate = win_rate,
            .trade_count = trade_count}};
}

}  // namespace standard_tools::backtest
