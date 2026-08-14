#pragma once

#include "standard_tools/backtest/result.hpp"

#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace standard_tools::backtest {

/// Monte Carlo simulator that reshuffles trade or period returns.
class MonteCarloSimulator {
public:
    /// Creates a simulator with the requested number of iterations and an
    /// optional seed for reproducibility.
    MonteCarloSimulator(int simulations, std::optional<std::uint64_t> seed);

    /// Simulates by reshuffling the returns of completed trades.
    MonteCarloResult FromTrades(const std::vector<Trade>& trades, double initial_capital) const;

    /// Simulates by reshuffling a period return series.
    MonteCarloResult FromReturns(const std::vector<double>& returns, double initial_capital) const;

    int Simulations() const noexcept { return simulations_; }
    bool HasSeed() const noexcept { return seed_.has_value(); }
    std::uint64_t Seed() const { return seed_.value(); }

private:
    int simulations_ = 0;
    std::optional<std::uint64_t> seed_;
};

}  // namespace standard_tools::backtest
