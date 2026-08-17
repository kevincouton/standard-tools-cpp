#include "standard_tools/analysis/calculator.hpp"
#include "standard_tools/analysis/result.hpp"
#include "standard_tools/core/errors.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <map>
#include <string>
#include <variant>
#include <vector>

using namespace standard_tools::analysis;
using namespace standard_tools::core;
using Catch::Approx;

TEST_CASE("Regression computes alpha, beta and rSquared", "[analysis]") {
    AnalysisCalculator calc;
    Request req;
    req.operation = operation::kRegression;
    req.asset_returns = {1.0, 2.0, 3.0, 4.0, 5.0};
    req.benchmark_returns = {1.0, 2.0, 3.0, 4.0, 5.0};

    const auto result = std::get<RegressionResult>(calc.Calculate(req));
    REQUIRE(result.beta == Approx(1.0));
    REQUIRE(result.alpha == Approx(0.0).margin(1e-12));
    REQUIRE(result.r_squared == Approx(1.0));
    REQUIRE(result.residuals.size() == req.asset_returns.size());
}

TEST_CASE("Regression rejects mismatched lengths", "[analysis]") {
    AnalysisCalculator calc;
    Request req;
    req.operation = operation::kRegression;
    req.asset_returns = {1.0, 2.0};
    req.benchmark_returns = {1.0};
    REQUIRE_THROWS_AS(calc.Calculate(req), DataQualityError);
}

TEST_CASE("Cointegration computes hedge ratio and z-score", "[analysis]") {
    AnalysisCalculator calc;
    Request req;
    req.operation = operation::kCointegration;
    // Generate two cointegrated price series with a small mean-reverting spread.
    for (int i = 0; i < 30; ++i) {
        const double b = 50.0 + 0.25 * static_cast<double>(i);
        const double spread = 0.1 * std::sin(static_cast<double>(i));
        req.b_closes.push_back(b);
        req.a_closes.push_back(100.0 + 2.0 * (b - 50.0) + spread);
    }

    const auto result = std::get<CointegrationResult>(calc.Calculate(req));
    REQUIRE(result.hedge_ratio == Approx(2.0).margin(1e-2));
    REQUIRE(std::isfinite(result.adf_statistic));
    REQUIRE(std::isfinite(result.half_life));
    REQUIRE(std::isfinite(result.current_z_score));
}

TEST_CASE("Cointegration rejects too few observations", "[analysis]") {
    AnalysisCalculator calc;
    Request req;
    req.operation = operation::kCointegration;
    req.a_closes = {1.0, 2.0};
    req.b_closes = {1.0, 2.0};
    REQUIRE_THROWS_AS(calc.Calculate(req), DataQualityError);
}

TEST_CASE("Hurst exponent is bounded and has interpretation", "[analysis]") {
    AnalysisCalculator calc;
    Request req;
    req.operation = operation::kHurst;
    // Construct 60 positive prices from a deterministic trend.
    double price = 100.0;
    for (int i = 0; i < 60; ++i) {
        req.prices.push_back(price);
        price *= 1.001;
    }

    const auto result = std::get<HurstResult>(calc.Calculate(req));
    REQUIRE(result.exponent >= 0.0);
    REQUIRE(result.exponent <= 1.0);
    REQUIRE(!result.interpretation.empty());
}

TEST_CASE("Hurst rejects non-positive prices", "[analysis]") {
    AnalysisCalculator calc;
    Request req;
    req.operation = operation::kHurst;
    req.prices.resize(60, 1.0);
    req.prices[10] = 0.0;
    REQUIRE_THROWS_AS(calc.Calculate(req), DataQualityError);
}

TEST_CASE("PCA returns deterministic explained variance ratios", "[analysis]") {
    AnalysisCalculator calc;
    Request req;
    req.operation = operation::kPca;
    req.returns_matrix = {
        {1.0, 2.0, 3.0, 4.0, 5.0},
        {5.0, 4.0, 3.0, 2.0, 1.0},
    };
    req.n_components = 2;

    const auto result = std::get<PCAResult>(calc.Calculate(req));
    REQUIRE(result.labels.size() == 2);
    REQUIRE(result.explained_variance_ratio.size() == 2);
    REQUIRE(result.loadings.size() == 2);
    REQUIRE(result.factor_returns.size() == 2);

    double sum = 0.0;
    for (double v : result.explained_variance_ratio) {
        sum += v;
    }
    REQUIRE(sum == Approx(1.0).margin(1e-12));
}

TEST_CASE("PCA extracts the shared factor of correlated series", "[analysis]") {
    AnalysisCalculator calc;
    Request req;
    req.operation = operation::kPca;
    req.returns_matrix = {
        {1.0, 2.0, 3.0, 4.0, 5.0},
        {1.0, 2.0, 3.0, 4.0, 5.0},
    };
    req.n_components = 2;

    const auto result = std::get<PCAResult>(calc.Calculate(req));

    // Perfectly correlated series: PC1 must capture all variance with
    // equal-magnitude loadings; a variance-sorted one-hot stub cannot do this.
    REQUIRE(result.explained_variance_ratio[0] == Approx(1.0).margin(1e-9));
    REQUIRE(result.explained_variance_ratio[1] == Approx(0.0).margin(1e-9));
    const double inv_sqrt2 = 1.0 / std::sqrt(2.0);
    REQUIRE(std::abs(result.loadings[0][0]) == Approx(inv_sqrt2).margin(1e-9));
    REQUIRE(std::abs(result.loadings[0][1]) == Approx(inv_sqrt2).margin(1e-9));
    // Loadings form a unit vector.
    const double norm = std::hypot(result.loadings[0][0], result.loadings[0][1]);
    REQUIRE(norm == Approx(1.0).margin(1e-12));
}

TEST_CASE("PCA rejects invalid n_components", "[analysis]") {
    AnalysisCalculator calc;
    Request req;
    req.operation = operation::kPca;
    req.returns_matrix = {{1.0, 2.0, 3.0}};
    req.n_components = 2;
    REQUIRE_THROWS_AS(calc.Calculate(req), DataQualityError);
}

TEST_CASE("Correlation returns perfect diagonal and aggregates", "[analysis]") {
    AnalysisCalculator calc;
    Request req;
    req.operation = operation::kCorrelation;
    req.returns_map["A"] = {1.0, 2.0, 3.0, 4.0, 5.0};
    req.returns_map["B"] = {1.0, 2.0, 3.0, 4.0, 5.0};
    req.returns_map["C"] = {5.0, 4.0, 3.0, 2.0, 1.0};

    const auto result = std::get<CorrelationResult>(calc.Calculate(req));
    REQUIRE(result.labels == std::vector<std::string>{"A", "B", "C"});
    REQUIRE(result.matrix.size() == 3);
    REQUIRE(result.matrix[0][0] == Approx(1.0));
    REQUIRE(result.matrix[0][1] == Approx(1.0));
    REQUIRE(result.matrix[0][2] == Approx(-1.0).margin(1e-12));
    REQUIRE(result.min >= -1.0);
    REQUIRE(result.max <= 1.0);
}

TEST_CASE("Correlation rejects empty input", "[analysis]") {
    AnalysisCalculator calc;
    Request req;
    req.operation = operation::kCorrelation;
    REQUIRE_THROWS_AS(calc.Calculate(req), DataQualityError);
}

TEST_CASE("Options prices a call and a put", "[analysis]") {
    AnalysisCalculator calc;
    Request req;
    req.operation = operation::kOptions;

    BlackScholesParams params;
    params.spot = 100.0;
    params.strike = 100.0;
    params.risk_free_rate = 0.05;
    params.volatility = 0.20;
    params.time_to_maturity = 1.0;
    params.option_type = OptionType::Call;
    req.black_scholes = params;

    const auto call = std::get<OptionPricingResult>(calc.Calculate(req));
    REQUIRE(call.price > 0.0);
    REQUIRE(call.delta > 0.0);
    REQUIRE(call.delta < 1.0);
    REQUIRE(call.gamma > 0.0);
    REQUIRE(call.vega > 0.0);

    req.black_scholes->option_type = OptionType::Put;
    const auto put = std::get<OptionPricingResult>(calc.Calculate(req));
    REQUIRE(put.price > 0.0);
    REQUIRE(put.delta < 0.0);
    REQUIRE(put.delta > -1.0);
}

TEST_CASE("Options rejects missing params", "[analysis]") {
    AnalysisCalculator calc;
    Request req;
    req.operation = operation::kOptions;
    REQUIRE_THROWS_AS(calc.Calculate(req), DataQualityError);
}

TEST_CASE("Unknown analysis operation throws", "[analysis]") {
    AnalysisCalculator calc;
    Request req;
    req.operation = "unknown";
    REQUIRE_THROWS_AS(calc.Calculate(req), InvalidCommandError);
}
