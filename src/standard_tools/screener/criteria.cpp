#include "standard_tools/screener/criteria.hpp"

#include <cmath>

namespace standard_tools::screener {

namespace {

bool IsFinite(double value) {
    return std::isfinite(value);
}

bool ApplyMax(const std::optional<double>& limit, double value) {
    if (!limit.has_value()) {
        return true;
    }
    if (!IsFinite(value)) {
        return false;
    }
    return value <= limit.value();
}

bool ApplyMin(const std::optional<double>& limit, double value) {
    if (!limit.has_value()) {
        return true;
    }
    if (!IsFinite(value)) {
        return false;
    }
    return value >= limit.value();
}

}  // namespace

bool ScreenCriteria::Apply(const FundamentalData& data) const {
    if (!ApplyMax(pe_ratio_max, data.pe_ratio)) return false;
    if (!ApplyMin(pe_ratio_min, data.pe_ratio)) return false;
    if (!ApplyMax(pb_ratio_max, data.pb_ratio)) return false;
    if (!ApplyMin(pb_ratio_min, data.pb_ratio)) return false;
    if (!ApplyMax(market_cap_max, data.market_cap)) return false;
    if (!ApplyMin(market_cap_min, data.market_cap)) return false;
    if (!ApplyMax(dividend_yield_max, data.dividend_yield)) return false;
    if (!ApplyMin(dividend_yield_min, data.dividend_yield)) return false;
    if (!ApplyMax(eps_growth_max, data.eps_growth)) return false;
    if (!ApplyMin(eps_growth_min, data.eps_growth)) return false;
    if (!ApplyMax(debt_to_equity_max, data.debt_to_equity)) return false;
    if (!ApplyMin(debt_to_equity_min, data.debt_to_equity)) return false;
    if (!ApplyMax(roe_max, data.roe)) return false;
    if (!ApplyMin(roe_min, data.roe)) return false;
    return true;
}

}  // namespace standard_tools::screener
