#pragma once

#include "standard_tools/core/errors.hpp"
#include "standard_tools/metrics/metrics.hpp"

#include <vector>

namespace standard_tools::metrics {

/// Default annualized risk-free rate used when no rate is supplied (2%).
inline constexpr double kDefaultRiskFreeRate = 0.02;

/// Conventional number of trading days used to annualize daily metrics.
inline constexpr double kTradingDaysPerYear = 252.0;

/// Computes risk and return metrics from a series of close prices.
///
/// Prices must be positive, finite, and contain at least two observations.
/// Simple returns are derived from adjacent closes and annualized using 252
/// trading days. Ratios that are undefined for the input series (for example,
/// Sharpe when volatility is zero) are returned as NaN.
class RiskReturnCalculator {
public:
    RiskReturnCalculator() = default;

    /// Compute cumulative and annualized return metrics.
    ///
    /// \param series Close-price series, oldest to newest.
    /// \param risk_free_rate Annualized risk-free rate as a decimal.
    /// \return ReturnMetrics for the series.
    ReturnMetrics CalculateReturnMetrics(const std::vector<double>& series,
                                         double risk_free_rate = kDefaultRiskFreeRate) const;

    /// Compute risk and risk-adjusted performance metrics.
    ///
    /// \param series Close-price series, oldest to newest.
    /// \param risk_free_rate Annualized risk-free rate as a decimal.
    /// \return RiskMetrics for the series.
    RiskMetrics CalculateRiskMetrics(const std::vector<double>& series,
                                     double risk_free_rate = kDefaultRiskFreeRate) const;
};

}  // namespace standard_tools::metrics
