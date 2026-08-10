#pragma once

#include "standard_tools/core/value_objects.hpp"

#include <future>
#include <memory>
#include <string>
#include <vector>

namespace standard_tools::marketdata {

struct FetchResult {
    std::vector<core::OHLCV> bars;
    std::optional<std::string> error;
};

class Provider {
public:
    virtual ~Provider() = default;

    virtual std::string Name() const = 0;

    virtual std::vector<core::OHLCV> Fetch(
        const core::Ticker& ticker,
        core::BarInterval interval,
        const core::DateRange& range) = 0;

    virtual std::future<FetchResult> FetchAsync(
        const core::Ticker& ticker,
        core::BarInterval interval,
        const core::DateRange& range) = 0;

    virtual core::TickerInfo GetTickerInfo(const core::Ticker& ticker) = 0;
    virtual core::FinancialRatios GetFinancialRatios(const core::Ticker& ticker) = 0;
    virtual core::DataSetMetadata GetMetadata() = 0;
};

using ProviderPtr = std::shared_ptr<Provider>;

}  // namespace standard_tools::marketdata
