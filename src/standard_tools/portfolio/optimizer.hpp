#pragma once

#include "standard_tools/portfolio/result.hpp"

#include <optional>
#include <string>
#include <vector>

namespace standard_tools::portfolio {

/// Objective constants for mean-variance optimization.
inline constexpr const char* kObjectiveMaxSharpe = "max_sharpe";
inline constexpr const char* kObjectiveMinVolatility = "min_volatility";
inline constexpr const char* kObjectiveTargetReturn = "target_return";
inline constexpr const char* kObjectiveTargetVolatility = "target_volatility";

/// Request for mean-variance optimization.
struct MeanVarianceRequest {
    /// Historical return series, one row per asset.
    std::vector<std::vector<double>> returns;

    /// Asset labels aligned with \p returns.
    std::vector<std::string> labels;

    /// Per-period risk-free rate used for the Sharpe ratio.
    double risk_free_rate = 0.0;

    /// Optimization objective.
    std::string objective = kObjectiveMaxSharpe;

    /// Required when objective is \p kObjectiveTargetReturn.
    std::optional<double> target_return;

    /// Required when objective is \p kObjectiveTargetVolatility.
    std::optional<double> target_volatility;
};

/// Run mean-variance optimization according to the request objective.
///
/// Implements a fast two-fund separation heuristic:
/// 1. Global minimum-variance portfolio.
/// 2. Maximum-Sharpe portfolio from the excess-return vector.
/// 3. Blend the two funds to satisfy target_return or target_volatility.
///
/// \throws InvalidCommandError for invalid objectives or dimensional mismatches.
/// \throws DataQualityError for empty/short/non-finite return series.
/// \throws InternalError if the covariance matrix cannot be inverted.
PortfolioResult MeanVariance(const MeanVarianceRequest& request);

}  // namespace standard_tools::portfolio
