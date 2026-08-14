#pragma once

#include <string>
#include <vector>

namespace standard_tools::screener {

/// A snapshot of fundamental metrics for a single security.
struct FundamentalData {
    std::string ticker;
    double market_cap = 0.0;
    double pe_ratio = 0.0;
    double pb_ratio = 0.0;
    double dividend_yield = 0.0;
    double eps_growth = 0.0;
    double debt_to_equity = 0.0;
    double roe = 0.0;
};

/// The outcome of a screening run.
///
/// Matches are tickers that satisfied every configured criterion. Failed are
/// tickers that could not be fetched or did not satisfy the criteria.
struct ScreenResult {
    std::vector<FundamentalData> matches;
    std::vector<std::string> failed;
};

}  // namespace standard_tools::screener
