#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <variant>
#include <vector>

namespace standard_tools::analysis {

using json = nlohmann::json;

/// Result of a single-variable OLS regression.
struct RegressionResult {
    double alpha = 0.0;
    double beta = 0.0;
    double r_squared = 0.0;
    std::vector<double> residuals;
};

/// Result of an Engle-Granger cointegration test.
struct CointegrationResult {
    double hedge_ratio = 0.0;
    double adf_statistic = 0.0;
    double half_life = 0.0;
    double current_z_score = 0.0;
};

/// Result of a Hurst exponent estimation.
struct HurstResult {
    double exponent = 0.0;
    std::string interpretation;
};

/// Result of principal component analysis via eigendecomposition of the
/// sample covariance matrix.
struct PCAResult {
    std::vector<std::string> labels;
    std::vector<double> explained_variance_ratio;
    std::vector<std::vector<double>> loadings;
    std::vector<std::vector<double>> factor_returns;
};

/// Result of a Pearson correlation matrix computation.
struct CorrelationResult {
    std::vector<std::string> labels;
    std::vector<std::vector<double>> matrix;
    double average = 0.0;
    double min = 0.0;
    double max = 0.0;
};

/// Result of a Black-Scholes option valuation.
struct OptionPricingResult {
    double price = 0.0;
    double delta = 0.0;
    double gamma = 0.0;
    double vega = 0.0;
    double theta = 0.0;
    double rho = 0.0;
};

/// Sealed result type produced by every analysis operation.
using Result = std::variant<
    RegressionResult,
    CointegrationResult,
    HurstResult,
    PCAResult,
    CorrelationResult,
    OptionPricingResult>;

inline void to_json(json& j, const RegressionResult& r) {
    j = json::object();
    j["alpha"] = r.alpha;
    j["beta"] = r.beta;
    j["r_squared"] = r.r_squared;
    j["residuals"] = r.residuals;
}

inline void to_json(json& j, const CointegrationResult& r) {
    j = json::object();
    j["hedge_ratio"] = r.hedge_ratio;
    j["adf_statistic"] = r.adf_statistic;
    j["half_life"] = r.half_life;
    j["current_z_score"] = r.current_z_score;
}

inline void to_json(json& j, const HurstResult& r) {
    j = json::object();
    j["exponent"] = r.exponent;
    j["interpretation"] = r.interpretation;
}

inline void to_json(json& j, const PCAResult& r) {
    j = json::object();
    j["labels"] = r.labels;
    j["explained_variance_ratio"] = r.explained_variance_ratio;
    j["loadings"] = r.loadings;
    j["factor_returns"] = r.factor_returns;
}

inline void to_json(json& j, const CorrelationResult& r) {
    j = json::object();
    j["labels"] = r.labels;
    j["matrix"] = r.matrix;
    j["average"] = r.average;
    j["min"] = r.min;
    j["max"] = r.max;
}

inline void to_json(json& j, const OptionPricingResult& r) {
    j = json::object();
    j["price"] = r.price;
    j["delta"] = r.delta;
    j["gamma"] = r.gamma;
    j["vega"] = r.vega;
    j["theta"] = r.theta;
    j["rho"] = r.rho;
}

inline void to_json(json& j, const Result& r) {
    std::visit([&j](const auto& v) { j = v; }, r);
}

}  // namespace standard_tools::analysis
