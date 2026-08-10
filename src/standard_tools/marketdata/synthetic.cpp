#include "standard_tools/marketdata/synthetic.hpp"

#include <date/date.h>

#include <algorithm>
#include <chrono>
#include <future>

namespace standard_tools::marketdata {

namespace {

core::Date AddDays(core::Date d, int days) {
    return d + std::chrono::days(days);
}

}  // namespace

std::vector<core::OHLCV> SyntheticProvider::Fetch(
    const core::Ticker& ticker,
    core::BarInterval interval,
    const core::DateRange& range) {
    (void)interval;
    (void)ticker;

    std::vector<core::OHLCV> bars;
    double price = 100.0;

    for (core::Date d = range.Start(); d <= range.End(); d = AddDays(d, 1)) {
        double open = price;
        double close = price + 1.0;
        double high = std::max(open, close) + 0.5;
        double low = std::min(open, close) - 0.5;
        bars.push_back(core::OHLCV{
            .date = d,
            .open = open,
            .high = high,
            .low = low,
            .close = close,
            .volume = 1'000'000,
        });
        price = close;
    }
    return bars;
}

std::future<FetchResult> SyntheticProvider::FetchAsync(
    const core::Ticker& ticker,
    core::BarInterval interval,
    const core::DateRange& range) {
    return std::async(std::launch::async, [this, &ticker, interval, &range]() {
        try {
            auto bars = Fetch(ticker, interval, range);
            return FetchResult{.bars = std::move(bars)};
        } catch (const std::exception& e) {
            return FetchResult{.error = e.what()};
        }
    });
}

core::TickerInfo SyntheticProvider::GetTickerInfo(const core::Ticker& ticker) {
    return core::TickerInfo{
        .symbol = ticker.Symbol(),
        .name = ticker.Symbol() + " Inc.",
        .sector = "Technology",
        .industry = "Software",
        .employees = 1000,
        .city = "New York",
        .country = "USA",
        .website = "https://example.com",
    };
}

core::FinancialRatios SyntheticProvider::GetFinancialRatios(const core::Ticker& ticker) {
    core::FinancialRatios ratios;
    ratios.symbol = ticker.Symbol();
    return ratios;
}

core::DataSetMetadata SyntheticProvider::GetMetadata() {
    return core::DataSetMetadata{
        .provider = "synthetic",
        .adjusted = false,
        .survivorship_free = false,
        .point_in_time = false,
        .frequency = "daily",
        .timezone = "UTC",
        .retrieved_at = std::chrono::system_clock::now(),
    };
}

}  // namespace standard_tools::marketdata
