#pragma once

#include "standard_tools/marketdata/provider.hpp"

namespace standard_tools::marketdata {

class SyntheticProvider : public Provider {
public:
    std::string Name() const override { return "synthetic"; }

    std::vector<core::OHLCV> Fetch(
        const core::Ticker& ticker,
        core::BarInterval interval,
        const core::DateRange& range) override;

    std::future<FetchResult> FetchAsync(
        const core::Ticker& ticker,
        core::BarInterval interval,
        const core::DateRange& range) override;

    core::TickerInfo GetTickerInfo(const core::Ticker& ticker) override;
    core::FinancialRatios GetFinancialRatios(const core::Ticker& ticker) override;
    core::DataSetMetadata GetMetadata() override;
};

}  // namespace standard_tools::marketdata
