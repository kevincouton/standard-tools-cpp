#pragma once

#include "standard_tools/core/errors.hpp"

#include <cmath>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

namespace standard_tools::portfolio {

using json = nlohmann::json;

/// Result of a portfolio optimization.
struct PortfolioResult {
    /// Optimized asset weights keyed by asset label.
    std::map<std::string, double> weights;

    /// Expected per-period return of the portfolio.
    double expected_return = 0.0;

    /// Expected per-period volatility of the portfolio.
    double volatility = 0.0;

    /// Risk-adjusted return using the supplied risk-free rate.
    double sharpe_ratio = 0.0;

    /// Maximum acceptable deviation of the sum of weights from 1.0.
    static constexpr double kWeightSumTolerance = 1e-6;

    /// Validate that the result has sane, properly-summed weights and finite
    /// metrics.
    ///
    /// \throws DataQualityError if weights are missing or non-finite.
    /// \throws InvalidCommandError if weights do not sum to approximately one.
    void Validate() const {
        if (weights.empty()) {
            throw core::DataQualityError{"result contains no weights"};
        }
        double sum = 0.0;
        for (const auto& [label, w] : weights) {
            if (std::isnan(w) || std::isinf(w)) {
                throw core::DataQualityError{"weight for " + label + " is non-finite"};
            }
            sum += w;
        }
        if (std::abs(sum - 1.0) > kWeightSumTolerance) {
            throw core::InvalidCommandError{"weights sum to " + std::to_string(sum) +
                                          ", expected 1.0"};
        }
        if (std::isnan(expected_return) || std::isinf(expected_return)) {
            throw core::DataQualityError{"expected return is non-finite"};
        }
        if (std::isnan(volatility) || std::isinf(volatility) || volatility < 0.0) {
            throw core::DataQualityError{"volatility is non-finite or negative"};
        }
        if (std::isnan(sharpe_ratio) || std::isinf(sharpe_ratio)) {
            throw core::DataQualityError{"sharpe ratio is non-finite"};
        }
    }
};

inline void to_json(json& j, const PortfolioResult& r) {
    j = json::object();
    j["weights"] = r.weights;
    j["expected_return"] = r.expected_return;
    j["volatility"] = r.volatility;
    j["sharpe_ratio"] = r.sharpe_ratio;
}

}  // namespace standard_tools::portfolio
