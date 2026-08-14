#pragma once

#include "standard_tools/screener/result.hpp"

#include <memory>
#include <string>

namespace standard_tools::screener {

/// Supplies fundamental data for an individual ticker.
class FundamentalProvider {
public:
    virtual ~FundamentalProvider() = default;

    /// Returns fundamental data for `ticker`.
    ///
    /// Implementations should throw standard_tools::core::NotFoundError when
    /// data for the ticker is not available.
    virtual FundamentalData Fetch(const std::string& ticker) = 0;
};

using FundamentalProviderPtr = std::shared_ptr<FundamentalProvider>;

}  // namespace standard_tools::screener
