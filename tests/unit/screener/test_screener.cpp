#include "standard_tools/screener/hardcoded_provider.hpp"
#include "standard_tools/screener/result.hpp"
#include "standard_tools/screener/service.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

using namespace standard_tools::screener;

namespace {

std::vector<std::string> GetTickers(const std::vector<FundamentalData>& matches) {
    std::vector<std::string> tickers;
    tickers.reserve(matches.size());
    for (const auto& data : matches) {
        tickers.push_back(data.ticker);
    }
    return tickers;
}

std::unordered_set<std::string> ToSet(const std::vector<std::string>& values) {
    return std::unordered_set<std::string>(values.begin(), values.end());
}

}  // namespace

TEST_CASE("Screener filters by P/E and ROE", "[screener]") {
    auto provider = std::make_shared<HardcodedFundamentalProvider>();
    Screener screener(provider);

    ScreenCriteria criteria;
    criteria.pe_ratio_max = 35.0;
    criteria.roe_min = 0.25;

    const std::vector<std::string> tickers = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA", "UNKNOWN"};
    const auto result = screener.Screen(tickers, criteria);

    const auto got = GetTickers(result.matches);
    const std::vector<std::string> want = {"AAPL", "MSFT", "GOOGL"};
    REQUIRE(got == want);

    const auto failed = ToSet(result.failed);
    const std::unordered_set<std::string> failed_want = {"AMZN", "TSLA", "UNKNOWN"};
    REQUIRE(failed == failed_want);
}

TEST_CASE("Screener empty criteria returns all available tickers", "[screener]") {
    auto provider = std::make_shared<HardcodedFundamentalProvider>();
    Screener screener(provider);

    const std::vector<std::string> tickers = {"AAPL", "MSFT", "UNKNOWN"};
    const auto result = screener.Screen(tickers, ScreenCriteria{});

    REQUIRE(result.matches.size() == 2);
    REQUIRE(result.failed.size() == 1);
    REQUIRE(result.failed.front() == "UNKNOWN");
}

TEST_CASE("Screener handles empty ticker list", "[screener]") {
    auto provider = std::make_shared<HardcodedFundamentalProvider>();
    Screener screener(provider);

    ScreenCriteria criteria;
    criteria.pe_ratio_max = 30.0;

    const auto result = screener.Screen({}, criteria);

    REQUIRE(result.matches.empty());
    REQUIRE(result.failed.empty());
}
