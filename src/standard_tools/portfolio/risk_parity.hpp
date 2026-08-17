#pragma once

#include "standard_tools/portfolio/result.hpp"

#include <string>
#include <vector>

namespace standard_tools::portfolio {

/// Request for risk-parity allocation with equal per-asset risk budgets.
struct RiskParityRequest {
    /// Historical return series, one row per asset.
    std::vector<std::vector<double>> returns;

    /// Asset labels aligned with \p returns.
    std::vector<std::string> labels;
};

/// Compute risk-parity weights via the full covariance matrix.
///
/// Weights are chosen so that every asset contributes equally to total
/// portfolio risk, i.e. w_i * (cov * w)_i is the same for all i. The solver
/// uses a fixed-point iteration on risk contributions and therefore accounts
/// for correlations, unlike plain inverse-volatility weighting.
///
/// \throws InvalidCommandError for dimensional mismatches.
/// \throws DataQualityError for empty/short/non-finite return series or zero
///         volatility across all assets.
PortfolioResult RiskParity(const RiskParityRequest& request);

}  // namespace standard_tools::portfolio
