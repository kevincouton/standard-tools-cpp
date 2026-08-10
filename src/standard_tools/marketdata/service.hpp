#pragma once

#include "standard_tools/marketdata/cache.hpp"
#include "standard_tools/marketdata/provider.hpp"

#include <future>
#include <map>
#include <mutex>
#include <string>

namespace standard_tools::marketdata {

class Service {
public:
    Service(std::string default_provider, std::shared_ptr<Cache> cache);

    void Register(ProviderPtr provider);

    std::vector<core::OHLCV> Fetch(
        const core::Ticker& ticker,
        core::BarInterval interval,
        const core::DateRange& range,
        const std::string& provider_name = "");

    std::future<FetchResult> FetchAsync(
        const core::Ticker& ticker,
        core::BarInterval interval,
        const core::DateRange& range,
        const std::string& provider_name = "");

    core::TickerInfo GetTickerInfo(const core::Ticker& ticker);
    core::FinancialRatios GetFinancialRatios(const core::Ticker& ticker);
    core::DataSetMetadata GetMetadata();

private:
    ProviderPtr ResolveProvider(const std::string& name);

    std::string default_provider_;
    std::map<std::string, ProviderPtr> providers_;
    std::mutex mu_;
    std::shared_ptr<Cache> cache_;
};

}  // namespace standard_tools::marketdata
