#pragma once

#include "standard_tools/screener/criteria.hpp"
#include "standard_tools/screener/fundamental_provider.hpp"
#include "standard_tools/screener/result.hpp"

#include <string>
#include <vector>

namespace standard_tools::screener {

/// Filters a universe of tickers by fundamental criteria.
class Screener {
public:
    explicit Screener(FundamentalProviderPtr provider);

    /// Fetches fundamental data for each ticker, applies the criteria, and
    /// returns the matches.
    ///
    /// Tickers that cannot be fetched or do not satisfy the criteria are
    /// recorded in ScreenResult::failed. Other provider errors are propagated
    /// as exceptions.
    ScreenResult Screen(const std::vector<std::string>& tickers,
                        const ScreenCriteria& criteria) const;

private:
    FundamentalProviderPtr provider_;
};

}  // namespace standard_tools::screener
