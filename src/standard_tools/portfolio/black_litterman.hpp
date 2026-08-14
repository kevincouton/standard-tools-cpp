#pragma once

#include "standard_tools/portfolio/result.hpp"

#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace standard_tools::portfolio {

using json = nlohmann::json;

/// Request for the canonical Black-Litterman model with explicit view matrices.
struct BlackLittermanRequest {
    /// Historical return series, one row per asset.
    std::vector<std::vector<double>> returns;

    /// Asset labels aligned with \p returns.
    std::vector<std::string> labels;

    /// Market capitalisation weights or absolute caps aligned with \p labels.
    std::vector<double> market_caps;

    /// K x N view matrix P; each row maps a view to a linear combination of
    /// asset returns.
    std::vector<std::vector<double>> p_matrix;

    /// K-element vector Q of view expected returns.
    std::vector<double> q_vector;

    /// Uncertainty scaling factor for the prior covariance.
    double tau = 0.05;

    /// Investor's risk-aversion coefficient delta.
    double risk_aversion = 2.5;
};

/// Request for the simplified expert-view variant of Black-Litterman.
struct BlackLittermanSimplifiedRequest {
    /// Historical return series, one row per asset.
    std::vector<std::vector<double>> returns;

    /// Asset labels aligned with \p returns.
    std::vector<std::string> labels;

    /// Market capitalisation weights or caps keyed by asset label.
    std::map<std::string, double> market_caps;

    /// Expected-return views keyed by asset label.
    std::map<std::string, double> views;

    /// Uncertainty scaling factor for the prior covariance.
    double tau = 0.05;

    /// Investor's risk-aversion coefficient delta.
    double risk_aversion = 2.5;
};

/// Result of a Black-Litterman optimization.
struct BlackLittermanResult {
    /// Optimized portfolio result with weights and metrics.
    PortfolioResult portfolio;

    /// Posterior expected returns keyed by asset label.
    std::map<std::string, double> expected_returns;

    /// Posterior covariance matrix.
    std::vector<std::vector<double>> covariance;
};

/// Run the canonical Black-Litterman model with explicit view matrices.
///
/// \throws InvalidCommandError for invalid parameters or dimensional mismatches.
/// \throws DataQualityError for degenerate inputs.
/// \throws InternalError if any matrix inversion fails.
BlackLittermanResult BlackLitterman(const BlackLittermanRequest& request);

/// Run the simplified Black-Litterman model using independent expert views.
///
/// Each view is treated as an independent, asset-specific view.
///
/// \throws InvalidCommandError for missing/unknown assets or invalid parameters.
/// \throws DataQualityError for degenerate inputs.
/// \throws InternalError if any matrix inversion fails.
BlackLittermanResult BlackLittermanSimplified(
    const BlackLittermanSimplifiedRequest& request);

inline void to_json(json& j, const BlackLittermanResult& r) {
    j = json::object();
    j["portfolio"] = r.portfolio;
    j["expected_returns"] = r.expected_returns;
    j["covariance"] = r.covariance;
}

}  // namespace standard_tools::portfolio
