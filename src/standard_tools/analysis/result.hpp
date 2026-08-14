#pragma once

#include <string>
#include <variant>
#include <vector>

namespace standard_tools::analysis {

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

/// Result of a deterministic PCA stub.
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

}  // namespace standard_tools::analysis
