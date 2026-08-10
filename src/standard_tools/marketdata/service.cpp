#include "standard_tools/marketdata/service.hpp"

#include "standard_tools/core/errors.hpp"

#include <sstream>

namespace standard_tools::marketdata {

Service::Service(std::string default_provider, std::shared_ptr<Cache> cache)
    : default_provider_(std::move(default_provider)), cache_(std::move(cache)) {}

void Service::Register(ProviderPtr provider) {
    std::lock_guard<std::mutex> lock(mu_);
    providers_[provider->Name()] = std::move(provider);
}

ProviderPtr Service::ResolveProvider(const std::string& name) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = providers_.find(name);
    if (it == providers_.end()) {
        throw core::ProviderNotAvailableError{name};
    }
    return it->second;
}

std::string CacheKey(
    const std::string& provider,
    const core::Ticker& ticker,
    core::BarInterval interval,
    const core::DateRange& range) {
    std::ostringstream oss;
    oss << provider << ':' << ticker.Symbol() << ':' << core::ToString(interval) << ':'
        << core::FormatDate(range.Start()) << ':' << core::FormatDate(range.End());
    return oss.str();
}

std::vector<core::OHLCV> Service::Fetch(
    const core::Ticker& ticker,
    core::BarInterval interval,
    const core::DateRange& range,
    const std::string& provider_name) {
    std::string name = provider_name.empty() ? default_provider_ : provider_name;
    auto provider = ResolveProvider(name);
    auto key = CacheKey(name, ticker, interval, range);
    if (auto cached = cache_->Get(key)) {
        return *cached;
    }
    auto series = provider->Fetch(ticker, interval, range);
    cache_->Put(key, series);
    return series;
}

std::future<FetchResult> Service::FetchAsync(
    const core::Ticker& ticker,
    core::BarInterval interval,
    const core::DateRange& range,
    const std::string& provider_name) {
    std::string name = provider_name.empty() ? default_provider_ : provider_name;
    auto provider = ResolveProvider(name);
    return provider->FetchAsync(ticker, interval, range);
}

core::TickerInfo Service::GetTickerInfo(const core::Ticker& ticker) {
    auto provider = ResolveProvider(default_provider_);
    return provider->GetTickerInfo(ticker);
}

core::FinancialRatios Service::GetFinancialRatios(const core::Ticker& ticker) {
    auto provider = ResolveProvider(default_provider_);
    return provider->GetFinancialRatios(ticker);
}

core::DataSetMetadata Service::GetMetadata() {
    auto provider = ResolveProvider(default_provider_);
    return provider->GetMetadata();
}

}  // namespace standard_tools::marketdata
