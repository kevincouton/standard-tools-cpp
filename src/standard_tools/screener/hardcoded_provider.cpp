#include "standard_tools/screener/hardcoded_provider.hpp"

#include "standard_tools/core/errors.hpp"

namespace standard_tools::screener {

HardcodedFundamentalProvider::HardcodedFundamentalProvider() {
    data_ = {
        {"AAPL",
         FundamentalData{
             .ticker = "AAPL",
             .market_cap = 3.4e12,
             .pe_ratio = 28.5,
             .pb_ratio = 8.2,
             .dividend_yield = 0.005,
             .eps_growth = 0.12,
             .debt_to_equity = 1.8,
             .roe = 0.45}},
        {"MSFT",
         FundamentalData{
             .ticker = "MSFT",
             .market_cap = 3.1e12,
             .pe_ratio = 32.0,
             .pb_ratio = 12.0,
             .dividend_yield = 0.007,
             .eps_growth = 0.15,
             .debt_to_equity = 0.5,
             .roe = 0.40}},
        {"GOOGL",
         FundamentalData{
             .ticker = "GOOGL",
             .market_cap = 2.1e12,
             .pe_ratio = 24.0,
             .pb_ratio = 6.5,
             .dividend_yield = 0.0,
             .eps_growth = 0.18,
             .debt_to_equity = 0.1,
             .roe = 0.30}},
        {"AMZN",
         FundamentalData{
             .ticker = "AMZN",
             .market_cap = 1.8e12,
             .pe_ratio = 42.0,
             .pb_ratio = 8.0,
             .dividend_yield = 0.0,
             .eps_growth = 0.20,
             .debt_to_equity = 0.8,
             .roe = 0.12}},
        {"TSLA",
         FundamentalData{
             .ticker = "TSLA",
             .market_cap = 8.0e11,
             .pe_ratio = 75.0,
             .pb_ratio = 15.0,
             .dividend_yield = 0.0,
             .eps_growth = 0.25,
             .debt_to_equity = 0.2,
             .roe = 0.20}},
    };
}

FundamentalData HardcodedFundamentalProvider::Fetch(const std::string& ticker) {
    auto it = data_.find(ticker);
    if (it == data_.end()) {
        throw core::NotFoundError{};
    }
    return it->second;
}

}  // namespace standard_tools::screener
