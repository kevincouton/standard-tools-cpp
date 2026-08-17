#include "standard_tools/portfolio/black_litterman.hpp"
#include "standard_tools/portfolio/optimizer.hpp"
#include "standard_tools/portfolio/risk_parity.hpp"
#include "standard_tools/portfolio/result.hpp"

#include "standard_tools/core/errors.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

using namespace standard_tools::portfolio;
using namespace standard_tools::core;

namespace {

constexpr double kEpsilon = 1e-6;

std::vector<std::vector<double>> SampleReturns() {
    // Three assets with distinct volatilities over 12 periods.
    return {
        {0.01, 0.02, -0.01, 0.015, 0.005, 0.01, -0.005, 0.02, 0.01, -0.01, 0.015, 0.005},
        {0.005, 0.01, 0.005, 0.01, 0.005, 0.01, 0.005, 0.01, 0.005, 0.01, 0.005, 0.01},
        {0.03, -0.02, 0.04, -0.01, 0.05, -0.03, 0.02, -0.01, 0.03, -0.02, 0.01, 0.02},
    };
}

std::vector<std::string> SampleLabels() {
    return {"equity", "bond", "commodity"};
}

double SumWeights(const PortfolioResult& result) {
    double sum = 0.0;
    for (const auto& [label, w] : result.weights) {
        (void)label;
        sum += w;
    }
    return sum;
}

}  // namespace

TEST_CASE("MeanVariance max_sharpe produces valid weights and metrics", "[portfolio]") {
    MeanVarianceRequest req;
    req.returns = SampleReturns();
    req.labels = SampleLabels();
    req.risk_free_rate = 0.0;
    req.objective = kObjectiveMaxSharpe;

    const auto result = MeanVariance(req);

    REQUIRE(std::abs(SumWeights(result) - 1.0) <= kEpsilon);
    REQUIRE(result.weights.size() == 3);
    REQUIRE(std::isfinite(result.expected_return));
    REQUIRE(std::isfinite(result.volatility));
    REQUIRE(result.volatility >= 0.0);
    REQUIRE(std::isfinite(result.sharpe_ratio));
}

TEST_CASE("MeanVariance min_volatility produces lower volatility than max_sharpe",
          "[portfolio]") {
    MeanVarianceRequest req;
    req.returns = SampleReturns();
    req.labels = SampleLabels();
    req.risk_free_rate = 0.0;

    req.objective = kObjectiveMaxSharpe;
    const auto max_sharpe = MeanVariance(req);

    req.objective = kObjectiveMinVolatility;
    const auto min_vol = MeanVariance(req);

    REQUIRE(min_vol.volatility <= max_sharpe.volatility + kEpsilon);
    REQUIRE(std::abs(SumWeights(min_vol) - 1.0) <= kEpsilon);
}

TEST_CASE("MeanVariance target_return blends between extremes", "[portfolio]") {
    MeanVarianceRequest req;
    req.returns = SampleReturns();
    req.labels = SampleLabels();
    req.risk_free_rate = 0.0;

    req.objective = kObjectiveMaxSharpe;
    const auto max_sharpe = MeanVariance(req);
    req.objective = kObjectiveMinVolatility;
    const auto min_vol = MeanVariance(req);

    const double mid_return = (max_sharpe.expected_return + min_vol.expected_return) / 2.0;
    req.objective = kObjectiveTargetReturn;
    req.target_return = mid_return;
    const auto target = MeanVariance(req);

    REQUIRE(std::abs(SumWeights(target) - 1.0) <= kEpsilon);
    REQUIRE(target.expected_return <= max_sharpe.expected_return + kEpsilon);
    REQUIRE(target.expected_return >= min_vol.expected_return - kEpsilon);
}

TEST_CASE("MeanVariance target_volatility respects feasible range", "[portfolio]") {
    MeanVarianceRequest req;
    req.returns = SampleReturns();
    req.labels = SampleLabels();
    req.risk_free_rate = 0.0;

    req.objective = kObjectiveMaxSharpe;
    const auto max_sharpe = MeanVariance(req);
    req.objective = kObjectiveMinVolatility;
    const auto min_vol = MeanVariance(req);

    const double mid_vol = (max_sharpe.volatility + min_vol.volatility) / 2.0;
    req.objective = kObjectiveTargetVolatility;
    req.target_volatility = mid_vol;
    const auto target = MeanVariance(req);

    REQUIRE(std::abs(SumWeights(target) - 1.0) <= kEpsilon);
    REQUIRE(target.volatility <= max_sharpe.volatility + kEpsilon);
    REQUIRE(target.volatility >= min_vol.volatility - kEpsilon);
}

TEST_CASE("MeanVariance rejects invalid inputs", "[portfolio]") {
    MeanVarianceRequest req;
    req.returns = SampleReturns();
    req.labels = SampleLabels();
    req.risk_free_rate = 0.0;
    req.objective = kObjectiveMaxSharpe;

    REQUIRE_NOTHROW(MeanVariance(req));

    req.objective = "unknown";
    REQUIRE_THROWS_AS(MeanVariance(req), InvalidCommandError);

    req.objective = kObjectiveTargetReturn;
    req.target_return = std::nullopt;
    REQUIRE_THROWS_AS(MeanVariance(req), InvalidCommandError);

    req.objective = kObjectiveMaxSharpe;
    req.risk_free_rate = std::numeric_limits<double>::infinity();
    REQUIRE_THROWS_AS(MeanVariance(req), InvalidCommandError);

    req.risk_free_rate = 0.0;
    auto bad_returns = req.returns;
    bad_returns[0].pop_back();
    req.returns = bad_returns;
    REQUIRE_THROWS_AS(MeanVariance(req), DataQualityError);
}

TEST_CASE("RiskParity produces valid inverse-volatility weights", "[portfolio]") {
    RiskParityRequest req;
    req.returns = SampleReturns();
    req.labels = SampleLabels();

    const auto result = RiskParity(req);

    REQUIRE(std::abs(SumWeights(result) - 1.0) <= kEpsilon);
    REQUIRE(result.weights.size() == 3);
    REQUIRE(result.weights.at("bond") > result.weights.at("commodity"));
    REQUIRE(std::isfinite(result.volatility));
    REQUIRE(result.volatility >= 0.0);
}

TEST_CASE("RiskParity equalizes risk contributions under correlation", "[portfolio]") {
    // Asset A is uncorrelated with B and C; B and C are identical series
    // (correlation 1). All three have the same variance, so inverse-volatility
    // weighting would give 1/3 each. True risk parity must account for the
    // correlation: with cov = s*[[1,0,0],[0,1,1],[0,1,1]], equal risk
    // contributions require a^2 = 2*b^2, i.e. w_A = 1/(1+sqrt(2)) ~ 0.41421
    // and w_B = w_C ~ 0.29289.
    RiskParityRequest req;
    req.returns = {
        {1.0, -1.0, 1.0, -1.0},
        {1.0, 1.0, -1.0, -1.0},
        {1.0, 1.0, -1.0, -1.0},
    };
    req.labels = {"a", "b", "c"};

    const auto result = RiskParity(req);

    REQUIRE(std::abs(SumWeights(result) - 1.0) <= kEpsilon);
    const double want_a = 1.0 / (1.0 + std::sqrt(2.0));
    const double want_b = (1.0 - want_a) / 2.0;
    REQUIRE(result.weights.at("a") == Catch::Approx(want_a).margin(1e-3));
    REQUIRE(result.weights.at("b") == Catch::Approx(want_b).margin(1e-3));
    REQUIRE(result.weights.at("c") == Catch::Approx(want_b).margin(1e-3));
}

TEST_CASE("RiskParity rejects degenerate returns", "[portfolio]") {
    RiskParityRequest req;
    req.returns = {{1.0, 1.0, 1.0}, {1.0, 1.0, 1.0}};
    req.labels = {"a", "b"};

    REQUIRE_THROWS_AS(RiskParity(req), DataQualityError);
}

TEST_CASE("BlackLitterman simplified produces valid weights", "[portfolio]") {
    BlackLittermanSimplifiedRequest req;
    req.returns = SampleReturns();
    req.labels = SampleLabels();
    req.market_caps = {{"equity", 1000.0}, {"bond", 500.0}, {"commodity", 300.0}};
    req.views = {{"equity", 0.02}, {"commodity", 0.01}};
    req.tau = 0.05;
    req.risk_aversion = 2.5;

    const auto result = BlackLittermanSimplified(req);

    REQUIRE(std::abs(SumWeights(result.portfolio) - 1.0) <= kEpsilon);
    REQUIRE(result.portfolio.weights.size() == 3);
    REQUIRE(result.expected_returns.size() == 3);
    REQUIRE(result.covariance.size() == 3);
    REQUIRE(std::isfinite(result.portfolio.volatility));
    REQUIRE(result.portfolio.volatility >= 0.0);
}

TEST_CASE("BlackLitterman simplified rejects invalid inputs", "[portfolio]") {
    BlackLittermanSimplifiedRequest req;
    req.returns = SampleReturns();
    req.labels = SampleLabels();
    req.market_caps = {{"equity", 1000.0}, {"bond", 500.0}, {"commodity", 300.0}};
    req.views = {{"equity", 0.02}};
    req.tau = 0.05;
    req.risk_aversion = 2.5;

    REQUIRE_NOTHROW(BlackLittermanSimplified(req));

    req.market_caps.erase("bond");
    REQUIRE_THROWS_AS(BlackLittermanSimplified(req), InvalidCommandError);

    req.market_caps = {{"equity", 1000.0}, {"bond", 500.0}, {"commodity", 300.0}};
    req.views = {{"unknown", 0.02}};
    REQUIRE_THROWS_AS(BlackLittermanSimplified(req), InvalidCommandError);
}

TEST_CASE("BlackLitterman explicit matrix produces valid result", "[portfolio]") {
    BlackLittermanRequest req;
    req.returns = SampleReturns();
    req.labels = SampleLabels();
    req.market_caps = {1000.0, 500.0, 300.0};
    req.p_matrix = {{1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}};
    req.q_vector = {0.02, 0.01};
    req.tau = 0.05;
    req.risk_aversion = 2.5;

    const auto result = BlackLitterman(req);

    REQUIRE(std::abs(SumWeights(result.portfolio) - 1.0) <= kEpsilon);
    REQUIRE(result.covariance.size() == 3);
    for (const auto& row : result.covariance) {
        REQUIRE(row.size() == 3);
    }
}
