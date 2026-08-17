#include "standard_tools/backtest/walk_forward.hpp"

#include "standard_tools/core/errors.hpp"

#include <algorithm>
#include <cstddef>
#include <unordered_map>
#include <vector>

namespace standard_tools::backtest {

namespace {

std::vector<std::unordered_map<std::string, double>> BuildParamCombinations(
    const std::unordered_map<std::string, std::vector<double>>& grid) {
    constexpr std::size_t kMaxWalkForwardCombinations = 10'000;

    std::vector<std::string> keys;
    keys.reserve(grid.size());
    for (const auto& kv : grid) {
        keys.push_back(kv.first);
    }

    std::vector<std::unordered_map<std::string, double>> combinations;
    combinations.push_back({});

    for (const auto& key : keys) {
        auto it = grid.find(key);
        if (it == grid.end() || it->second.empty()) {
            continue;
        }
        std::vector<std::unordered_map<std::string, double>> next;
        next.reserve(combinations.size() * it->second.size());
        for (const auto& base : combinations) {
            for (double value : it->second) {
                auto extended = base;
                extended[key] = value;
                next.push_back(std::move(extended));
            }
        }
        combinations = std::move(next);
        if (combinations.size() > kMaxWalkForwardCombinations) {
            throw core::InvalidCommandError{
                "parameter grid produces more than " +
                std::to_string(kMaxWalkForwardCombinations) + " combinations"};
        }
    }

    return combinations;
}

double ScoreResult(const BacktestResult& result, OptimizationMetric metric) {
    switch (metric) {
        case OptimizationMetric::Sharpe:
            return result.metrics.sharpe.value_or(0.0);
        case OptimizationMetric::WinRate:
            return result.metrics.win_rate;
        case OptimizationMetric::TotalReturn:
        default:
            return result.total_return;
    }
}

std::unordered_map<std::string, double> Optimize(
    const std::string& strategy_name,
    const std::vector<core::OHLCV>& train,
    const std::vector<std::unordered_map<std::string, double>>& combinations,
    const BacktestConfig& config,
    OptimizationMetric metric) {
    double best_score = 0.0;
    std::unordered_map<std::string, double> best_params;
    bool found = false;

    for (const auto& params : combinations) {
        try {
            BacktestEngine engine(strategy_name, config);
            auto result = engine.Run(train, params);
            double score = ScoreResult(result, metric);
            if (!found || score > best_score) {
                best_score = score;
                best_params = params;
                found = true;
            }
        } catch (...) {
            // Skip parameter combinations that fail to run.
            continue;
        }
    }

    if (!found) {
        throw core::DataQualityError{"no parameter combination produced a result"};
    }
    return best_params;
}

WalkForwardResult CombineResults(
    const std::vector<BacktestResult>& results,
    const std::vector<ParamWindow>& selected_params,
    const BacktestConfig& config) {
    if (results.empty()) {
        throw core::InvalidCommandError{"no out-of-sample windows were generated"};
    }

    std::vector<EquityPoint> equity_curve;
    std::vector<Trade> trades;
    double cumulative_capital = 0.0;

    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        double window_initial = result.equity_curve.empty() ? config.initial_capital
                                                            : result.equity_curve.front().equity;
        double window_final = result.equity_curve.empty() ? window_initial
                                                          : result.equity_curve.back().equity;

        double scale = 0.0;
        if (i == 0) {
            scale = window_initial == 0.0 ? 0.0 : config.initial_capital / window_initial;
        } else {
            scale = window_initial == 0.0 ? 0.0 : cumulative_capital / window_initial;
        }

        for (const auto& p : result.equity_curve) {
            equity_curve.push_back(EquityPoint{.date = p.date, .equity = p.equity * scale});
        }
        for (const auto& t : result.trades) {
            trades.push_back(Trade{
                .entry_date = t.entry_date,
                .exit_date = t.exit_date,
                .entry_price = t.entry_price,
                .exit_price = t.exit_price,
                .quantity = t.quantity,
                .side = t.side,
                .pnl = t.pnl * scale});
        }

        cumulative_capital = window_final * scale;
    }

    // Deduplicate by date in case windows overlap.
    std::sort(
        equity_curve.begin(), equity_curve.end(), [](const auto& a, const auto& b) {
            return a.date < b.date;
        });
    equity_curve.erase(
        std::unique(
            equity_curve.begin(), equity_curve.end(),
            [](const auto& a, const auto& b) { return a.date == b.date; }),
        equity_curve.end());

    double peak = 0.0;
    double max_dd = 0.0;
    for (const auto& p : equity_curve) {
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
    double total_return = 0.0;
    if (!equity_curve.empty() && equity_curve.front().equity != 0.0) {
        total_return = equity_curve.back().equity / equity_curve.front().equity - 1.0;
    }

    double win_rate = 0.0;
    std::size_t number_of_trades = trades.size();
    if (!trades.empty()) {
        std::size_t wins = 0;
        for (const auto& t : trades) {
            if (t.pnl > 0.0) {
                ++wins;
            }
        }
        win_rate = static_cast<double>(wins) / static_cast<double>(trades.size());
    }

    return WalkForwardResult{
        .equity_curve = std::move(equity_curve),
        .trades = std::move(trades),
        .total_return = total_return,
        .max_drawdown = -max_dd,
        .number_of_trades = number_of_trades,
        .win_rate = win_rate,
        .selected_params = selected_params};
}

}  // namespace

WalkForwardOptimizer::WalkForwardOptimizer(WalkForwardRequest request)
    : request_(std::move(request)) {
    constexpr std::size_t kMaxWalkForwardWindow = 10'000;
    BacktestEngine engine(request_.strategy, request_.config);
    if (request_.train_size == 0 || request_.test_size == 0) {
        throw core::InvalidCommandError{"train and test sizes must be positive"};
    }
    if (request_.train_size > kMaxWalkForwardWindow || request_.test_size > kMaxWalkForwardWindow) {
        throw core::InvalidCommandError{
            "train and test sizes must be <= " + std::to_string(kMaxWalkForwardWindow)};
    }
    if (request_.param_grid.empty()) {
        throw core::InvalidCommandError{"walk-forward requires a non-empty parameter grid"};
    }
}

WalkForwardResult WalkForwardOptimizer::Run() const {
    const auto& series = request_.series;
    if (series.size() < request_.train_size + request_.test_size) {
        throw core::InvalidCommandError{"series is too short for walk-forward configuration"};
    }

    auto combinations = BuildParamCombinations(request_.param_grid);
    if (combinations.empty()) {
        throw core::InvalidCommandError{"walk-forward requires a non-empty parameter grid"};
    }

    std::vector<BacktestResult> test_results;
    std::vector<ParamWindow> selected_params;

    std::size_t start = 0;
    while (start + request_.train_size + request_.test_size <= series.size()) {
        std::size_t train_end = start + request_.train_size;
        std::size_t test_end = train_end + request_.test_size;

        std::vector<core::OHLCV> train(series.begin() + start, series.begin() + train_end);
        std::vector<core::OHLCV> test(series.begin() + train_end, series.begin() + test_end);

        auto best_params = Optimize(
            request_.strategy, train, combinations, request_.config, request_.metric);

        BacktestEngine engine(request_.strategy, request_.config);
        auto result = engine.Run(test, best_params);

        selected_params.push_back(ParamWindow{.start = test.front().date, .params = best_params});
        test_results.push_back(std::move(result));

        start += request_.test_size;
    }

    return CombineResults(test_results, selected_params, request_.config);
}

}  // namespace standard_tools::backtest
