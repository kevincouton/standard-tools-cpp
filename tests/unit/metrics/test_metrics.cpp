#include "standard_tools/core/errors.hpp"
#include "standard_tools/metrics/metrics.hpp"
#include "standard_tools/metrics/risk_return.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <limits>
#include <vector>

using namespace standard_tools::metrics;
using namespace standard_tools::core;

namespace {

constexpr double kEpsilon = 1e-9;

bool IsNaN(double v) { return std::isnan(v); }

bool ApproxEqual(double a, double b, double tol = kEpsilon) {
    if (IsNaN(a) && IsNaN(b)) {
        return true;
    }
    return std::abs(a - b) <= tol;
}

}  // namespace

TEST_CASE("RiskReturnCalculator computes known series correctly", "[metrics]") {
    const std::vector<double> closes = {100.0, 110.0, 105.0, 120.0};
    const RiskReturnCalculator calc;

    const auto ret = calc.CalculateReturnMetrics(closes);
    const auto risk = calc.CalculateRiskMetrics(closes);

    REQUIRE(ApproxEqual(ret.cumulative_return, 0.2));
    REQUIRE(ApproxEqual(risk.max_drawdown, -5.0 / 110.0));
    REQUIRE(ApproxEqual(risk.var_95, -5.0 / 110.0));
    REQUIRE(ApproxEqual(risk.cvar_95, -5.0 / 110.0));
    REQUIRE(risk.volatility > 0.0);
    REQUIRE(ret.annualized_volatility == risk.volatility);
    REQUIRE(ret.cagr > ret.cumulative_return);
}

TEST_CASE("RiskReturnCalculator handles constant prices", "[metrics]") {
    const std::vector<double> closes = {100.0, 100.0, 100.0, 100.0};
    const RiskReturnCalculator calc;

    const auto ret = calc.CalculateReturnMetrics(closes);
    const auto risk = calc.CalculateRiskMetrics(closes);

    REQUIRE(ApproxEqual(ret.cumulative_return, 0.0));
    REQUIRE(ApproxEqual(ret.cagr, 0.0));
    REQUIRE(ApproxEqual(ret.annualized_volatility, 0.0));
    REQUIRE(ApproxEqual(risk.max_drawdown, 0.0));
    REQUIRE(IsNaN(risk.sharpe_ratio));
    REQUIRE(!IsNaN(risk.sortino_ratio));
    REQUIRE(risk.sortino_ratio < 0.0);
    REQUIRE(IsNaN(risk.calmar_ratio));
    REQUIRE(ApproxEqual(risk.var_95, 0.0));
    REQUIRE(ApproxEqual(risk.cvar_95, 0.0));
}

TEST_CASE("RiskReturnCalculator handles monotonic increase", "[metrics]") {
    const std::vector<double> closes = {100.0, 110.0, 121.0};
    const RiskReturnCalculator calc;

    const auto ret = calc.CalculateReturnMetrics(closes);
    const auto risk = calc.CalculateRiskMetrics(closes);

    REQUIRE(ApproxEqual(ret.cumulative_return, 0.21));
    REQUIRE(ApproxEqual(risk.max_drawdown, 0.0));
    REQUIRE(IsNaN(risk.sharpe_ratio));
    REQUIRE(IsNaN(risk.sortino_ratio));
    REQUIRE(IsNaN(risk.calmar_ratio));
}

TEST_CASE("RiskReturnCalculator handles monotonic decrease", "[metrics]") {
    const std::vector<double> closes = {100.0, 90.0, 81.0};
    const RiskReturnCalculator calc;

    const auto ret = calc.CalculateReturnMetrics(closes);
    const auto risk = calc.CalculateRiskMetrics(closes);

    REQUIRE(ApproxEqual(ret.cumulative_return, -0.19));
    REQUIRE(ApproxEqual(risk.max_drawdown, -0.19));
    REQUIRE(ApproxEqual(risk.var_95, -0.1));
    REQUIRE(ApproxEqual(risk.cvar_95, -0.1));
}

TEST_CASE("RiskReturnCalculator rejects insufficient data", "[metrics]") {
    const RiskReturnCalculator calc;

    REQUIRE_THROWS_AS(calc.CalculateReturnMetrics({}), InsufficientDataError);
    REQUIRE_THROWS_AS(calc.CalculateReturnMetrics({100.0}), InsufficientDataError);
    REQUIRE_THROWS_AS(calc.CalculateRiskMetrics({}), InsufficientDataError);
    REQUIRE_THROWS_AS(calc.CalculateRiskMetrics({100.0}), InsufficientDataError);
}

TEST_CASE("RiskReturnCalculator rejects invalid prices", "[metrics]") {
    const RiskReturnCalculator calc;

    REQUIRE_THROWS_AS(calc.CalculateReturnMetrics({100.0, 0.0, 101.0}), InvalidPricesError);
    REQUIRE_THROWS_AS(calc.CalculateReturnMetrics({100.0, -10.0, 101.0}), InvalidPricesError);
    REQUIRE_THROWS_AS(
        calc.CalculateReturnMetrics({100.0, std::numeric_limits<double>::quiet_NaN(), 101.0}),
        InvalidPricesError);
    REQUIRE_THROWS_AS(
        calc.CalculateReturnMetrics({100.0, std::numeric_limits<double>::infinity(), 101.0}),
        InvalidPricesError);
    REQUIRE_THROWS_AS(
        calc.CalculateReturnMetrics({100.0, -std::numeric_limits<double>::infinity(), 101.0}),
        InvalidPricesError);
}

TEST_CASE("RiskReturnCalculator respects risk-free rate", "[metrics]") {
    const std::vector<double> closes = {100.0, 101.0, 102.0, 103.0, 104.0};

    const RiskReturnCalculator calc_with_rf;
    const RiskReturnCalculator calc_no_rf;

    const auto risk_with_rf = calc_with_rf.CalculateRiskMetrics(closes, 0.02);
    const auto risk_no_rf = calc_no_rf.CalculateRiskMetrics(closes, 0.0);

    REQUIRE(!IsNaN(risk_with_rf.sharpe_ratio));
    REQUIRE(!IsNaN(risk_no_rf.sharpe_ratio));
    REQUIRE(risk_no_rf.sharpe_ratio > risk_with_rf.sharpe_ratio);
}
