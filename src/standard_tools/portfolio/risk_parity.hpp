#pragma once

#include "standard_tools/portfolio/result.hpp"

#include <string>
#include <vector>

namespace standard_tools::portfolio {

/// Request for inverse-volatility risk-parity allocation.
struct RiskParityRequest {
    /// Historical return series, one row per asset.
    std::vector<std::vector<double>> returns;

    /// Asset labels aligned with \p returns.
    std::vector<std::string> labels;
};

/// Compute inverse-volatility risk-parity weights.
///
/// The weight of each asset is proportional to the inverse of its sample
/// volatility. Assets with lower volatility receive larger weights so that each
/// asset contributes roughly equally to the portfolio's total volatility
/// budget.
///
/// \throws InvalidCommandError for dimensional mismatches.
/// \throws DataQualityError for empty/short/non-finite return series or zero
///         volatility across all assets.
PortfolioResult RiskParity(const RiskParityRequest& request);

}  // namespace standard_tools::portfolio
