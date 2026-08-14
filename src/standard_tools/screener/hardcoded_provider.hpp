#pragma once

#include "standard_tools/screener/fundamental_provider.hpp"

#include <map>
#include <string>

namespace standard_tools::screener {

/// Static production stub for a small universe of well-known equities.
class HardcodedFundamentalProvider : public FundamentalProvider {
public:
    HardcodedFundamentalProvider();

    FundamentalData Fetch(const std::string& ticker) override;

private:
    std::map<std::string, FundamentalData> data_;
};

}  // namespace standard_tools::screener
