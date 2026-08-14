#pragma once

#include "standard_tools/analysis/result.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace standard_tools::analysis {

/// Option type for Black-Scholes pricing.
enum class OptionType {
    Call,
    Put,
};

/// Parameters required to price a European option.
struct BlackScholesParams {
    double spot = 0.0;
    double strike = 0.0;
    double risk_free_rate = 0.0;
    double volatility = 0.0;
    double time_to_maturity = 0.0;
    OptionType option_type = OptionType::Call;
};

/// Request carries the inputs for any analysis operation.
struct Request {
    std::string operation;

    // Regression inputs.
    std::vector<double> asset_returns;
    std::vector<double> benchmark_returns;

    // Cointegration inputs.
    std::vector<double> a_closes;
    std::vector<double> b_closes;

    // Hurst inputs.
    std::vector<double> prices;
    std::optional<int> max_lag;

    // PCA inputs.
    std::vector<std::vector<double>> returns_matrix;
    int n_components = 1;

    // Correlation inputs.
    std::map<std::string, std::vector<double>> returns_map;

    // Options inputs.
    std::optional<BlackScholesParams> black_scholes;
};

/// Operation names supported by the analysis calculator.
namespace operation {

constexpr const char* kRegression = "regression";
constexpr const char* kCointegration = "cointegration";
constexpr const char* kHurst = "hurst";
constexpr const char* kPca = "pca";
constexpr const char* kCorrelation = "correlation";
constexpr const char* kOptions = "options";

}  // namespace operation

/// Calculator dispatches analysis requests by operation name.
class AnalysisCalculator {
public:
    AnalysisCalculator() = default;

    /// Runs the analysis identified by request.operation and returns a Result.
    Result Calculate(const Request& request) const;

private:
    RegressionResult Regression(const Request& request) const;
    CointegrationResult Cointegration(const Request& request) const;
    HurstResult Hurst(const Request& request) const;
    PCAResult Pca(const Request& request) const;
    CorrelationResult Correlation(const Request& request) const;
    OptionPricingResult Options(const Request& request) const;
};

}  // namespace standard_tools::analysis
