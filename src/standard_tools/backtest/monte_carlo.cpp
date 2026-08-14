#include "standard_tools/backtest/monte_carlo.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace standard_tools::backtest {

namespace {

std::mt19937_64 MakeRNG(std::optional<std::uint64_t> seed) {
    if (seed.has_value()) {
        return std::mt19937_64{seed.value()};
    }
    std::random_device rd;
    std::seed_seq seq{rd(), rd(), rd(), rd()};
    return std::mt19937_64{seq};
}

std::pair<double, double> SimulatePath(const std::vector<double>& returns, double initial) {
    double equity = initial;
    double peak = equity;
    double max_dd = 0.0;

    for (double r : returns) {
        equity *= 1.0 + r;
        if (equity > peak) {
            peak = equity;
        }
        if (peak > 0.0) {
            double dd = (peak - equity) / peak;
            if (dd > max_dd) {
                max_dd = dd;
            }
        }
    }

    return {equity, -max_dd};
}

double Percentile(const std::vector<double>& values, double quantile) {
    if (values.empty()) {
        return 0.0;
    }
    std::size_t index = static_cast<std::size_t>(
        quantile * static_cast<double>(values.size() - 1) + 0.5);
    if (index >= values.size()) {
        index = values.size() - 1;
    }
    return values[index];
}

}  // namespace

MonteCarloSimulator::MonteCarloSimulator(int simulations, std::optional<std::uint64_t> seed)
    : simulations_(simulations < 0 ? 0 : simulations), seed_(seed) {}

MonteCarloResult MonteCarloSimulator::FromTrades(
    const std::vector<Trade>& trades, double initial_capital) const {
    std::vector<double> returns;
    returns.reserve(trades.size());
    for (const auto& t : trades) {
        double cost = t.entry_price * t.quantity;
        if (cost == 0.0) {
            returns.push_back(0.0);
        } else {
            returns.push_back(t.pnl / cost);
        }
    }
    return FromReturns(returns, initial_capital);
}

MonteCarloResult MonteCarloSimulator::FromReturns(
    const std::vector<double>& returns, double initial_capital) const {
    if (returns.empty() || simulations_ == 0) {
        return MonteCarloResult{
            .simulations = simulations_,
            .final_equity_ci = ConfidenceInterval{.lower = initial_capital, .upper = initial_capital},
            .max_drawdown_ci = ConfidenceInterval{.lower = 0.0, .upper = 0.0},
            .initial_capital = initial_capital};
    }

    auto rng = MakeRNG(seed_);
    std::vector<double> final_equities;
    std::vector<double> max_drawdowns;
    final_equities.reserve(simulations_);
    max_drawdowns.reserve(simulations_);

    for (int i = 0; i < simulations_; ++i) {
        auto shuffled = returns;
        std::shuffle(shuffled.begin(), shuffled.end(), rng);
        auto [equity, max_dd] = SimulatePath(shuffled, initial_capital);
        final_equities.push_back(equity);
        max_drawdowns.push_back(max_dd);
    }

    std::sort(final_equities.begin(), final_equities.end());
    std::sort(max_drawdowns.begin(), max_drawdowns.end());

    return MonteCarloResult{
        .simulations = simulations_,
        .final_equity_ci =
            ConfidenceInterval{
                .lower = Percentile(final_equities, 0.05),
                .upper = Percentile(final_equities, 0.95)},
        .max_drawdown_ci =
            ConfidenceInterval{
                .lower = Percentile(max_drawdowns, 0.05),
                .upper = Percentile(max_drawdowns, 0.95)},
        .initial_capital = initial_capital};
}

}  // namespace standard_tools::backtest
