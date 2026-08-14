#pragma once

#include "standard_tools/screener/result.hpp"

#include <optional>

namespace standard_tools::screener {

/// Optional inclusive bounds on fundamental metrics.
///
/// A std::nullopt bound disables the corresponding filter. Bounds are inclusive.
struct ScreenCriteria {
    std::optional<double> pe_ratio_max;
    std::optional<double> pe_ratio_min;
    std::optional<double> pb_ratio_max;
    std::optional<double> pb_ratio_min;
    std::optional<double> market_cap_max;
    std::optional<double> market_cap_min;
    std::optional<double> dividend_yield_max;
    std::optional<double> dividend_yield_min;
    std::optional<double> eps_growth_max;
    std::optional<double> eps_growth_min;
    std::optional<double> debt_to_equity_max;
    std::optional<double> debt_to_equity_min;
    std::optional<double> roe_max;
    std::optional<double> roe_min;

    /// Returns true if `data` satisfies every configured criterion.
    ///
    /// A metric that is NaN or infinite causes the security to fail.
    bool Apply(const FundamentalData& data) const;
};

}  // namespace standard_tools::screener
