#include "standard_tools/backtest/engine.hpp"
#include "standard_tools/backtest/monte_carlo.hpp"
#include "standard_tools/backtest/walk_forward.hpp"
#include "standard_tools/core/errors.hpp"
#include "standard_tools/core/value_objects.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

using Catch::Approx;
using namespace standard_tools;

namespace {

std::vector<core::OHLCV> MakeBar(
    int year, int month, int day, double open, double close, std::int64_t volume) {
    return {core::OHLCV{
        .date = core::MakeDate(year, month, day),
        .open = open,
        .high = std::max(open, close) + 0.5,
        .low = std::min(open, close) - 0.5,
        .close = close,
        .volume = volume}};
}

std::vector<core::OHLCV> MakeRisingSeries(std::size_t n, double start, double step) {
    std::vector<core::OHLCV> series;
    series.reserve(n);
    double price = start;
    for (std::size_t i = 0; i < n; ++i) {
        double open = price;
        double close = open + step;
        auto bar = MakeBar(2024, 1, 1 + static_cast<int>(i), open, close, 1'000'000);
        series.insert(series.end(), bar.begin(), bar.end());
        price = close;
    }
    return series;
}

std::vector<core::OHLCV> MakeConstantSeries(std::size_t n, double price) {
    std::vector<core::OHLCV> series;
    series.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        auto bar = MakeBar(2024, 1, 1 + static_cast<int>(i), price, price, 1'000'000);
        series.insert(series.end(), bar.begin(), bar.end());
    }
    return series;
}

}  // namespace

TEST_CASE("BuyAndHold total return", "[backtest]") {
    auto series = MakeRisingSeries(10, 100.0, 1.0);
    backtest::BacktestConfig config;
    config.initial_capital = 100'000.0;
    config.commission_rate = 0.0;

    backtest::BacktestEngine engine("buy_and_hold", config);
    auto result = engine.Run(series, {});

    // Bar 0 close=101, bar 1 open=101/close=102, ..., bar 9 close=110.
    // The buy signal on bar 0 executes at bar 1 open (101) and is held to the
    // last close (110). Expected total return is 110/101 - 1.
    double want_return = 110.0 / 101.0 - 1.0;
    REQUIRE(result.total_return == Approx(want_return).epsilon(1e-9));
    REQUIRE(result.final_equity > config.initial_capital);
    REQUIRE(result.trades.size() == 1);
    REQUIRE(result.trades[0].side == backtest::TradeSide::Long);
}

TEST_CASE("SmaCrossover generates trades", "[backtest]") {
    auto flat = MakeConstantSeries(30, 100.0);
    auto rise = MakeRisingSeries(15, 100.0, 1.0);
    flat.pop_back();
    flat.insert(flat.end(), rise.begin(), rise.end());

    backtest::BacktestConfig config;
    config.initial_capital = 100'000.0;

    backtest::BacktestEngine engine("sma_crossover", config);
    auto result = engine.Run(flat, {{"fast", 5.0}, {"slow", 10.0}});

    REQUIRE(!result.trades.empty());
    REQUIRE(result.metrics.trade_count >= 1);
}

TEST_CASE("Empty series errors", "[backtest]") {
    backtest::BacktestConfig config;
    backtest::BacktestEngine engine("buy_and_hold", config);
    REQUIRE_THROWS_AS(engine.Run({}, {}), core::InvalidCommandError);
}

TEST_CASE("Unknown strategy errors", "[backtest]") {
    REQUIRE_THROWS_AS(
        backtest::BacktestEngine("unknown_strategy", backtest::BacktestConfig{}),
        core::InvalidCommandError);
}

TEST_CASE("MonteCarlo from returns is deterministic with seed", "[backtest]") {
    backtest::MonteCarloSimulator sim1(100, 42);
    backtest::MonteCarloSimulator sim2(100, 42);
    std::vector<double> returns{0.01, -0.02, 0.015, -0.005, 0.02};

    auto result1 = sim1.FromReturns(returns, 100'000.0);
    auto result2 = sim2.FromReturns(returns, 100'000.0);

    REQUIRE(result1.simulations == 100);
    REQUIRE(result1.final_equity_ci.lower == Approx(result2.final_equity_ci.lower));
    REQUIRE(result1.final_equity_ci.upper == Approx(result2.final_equity_ci.upper));
    REQUIRE(result1.max_drawdown_ci.lower == Approx(result2.max_drawdown_ci.lower));
    REQUIRE(result1.max_drawdown_ci.upper == Approx(result2.max_drawdown_ci.upper));
}

TEST_CASE("WalkForward requires non-empty grid", "[backtest]") {
    backtest::WalkForwardRequest req;
    req.strategy = "buy_and_hold";
    req.series = MakeRisingSeries(20, 100.0, 1.0);
    req.train_size = 10;
    req.test_size = 5;

    REQUIRE_THROWS_AS(backtest::WalkForwardOptimizer{req}, core::InvalidCommandError);
}
