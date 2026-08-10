#include "standard_tools/core/value_objects.hpp"
#include "standard_tools/marketdata/cache.hpp"
#include "standard_tools/marketdata/service.hpp"
#include "standard_tools/marketdata/synthetic.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace standard_tools;

TEST_CASE("SyntheticProvider generates daily bars", "[marketdata]") {
    marketdata::SyntheticProvider provider;
    auto ticker = core::Ticker("TEST");
    auto range = core::DateRange(core::MakeDate(2024, 1, 1), core::MakeDate(2024, 1, 5));
    auto bars = provider.Fetch(ticker, core::BarInterval::Daily, range);
    REQUIRE(bars.size() == 5);
    REQUIRE(bars.front().open == 100.0);
}

TEST_CASE("SyntheticProvider provides ticker info", "[marketdata]") {
    marketdata::SyntheticProvider provider;
    auto ticker = core::Ticker("AAPL");
    auto info = provider.GetTickerInfo(ticker);
    REQUIRE(info.symbol == "AAPL");
    REQUIRE(!info.name.empty());
}

TEST_CASE("Service registers and resolves providers", "[marketdata]") {
    auto cache = std::make_shared<marketdata::InMemoryCache>();
    marketdata::Service service("synthetic", cache);
    service.Register(std::make_shared<marketdata::SyntheticProvider>());

    auto ticker = core::Ticker("AAPL");
    auto range = core::DateRange(core::MakeDate(2024, 1, 1), core::MakeDate(2024, 1, 2));
    auto bars = service.Fetch(ticker, core::BarInterval::Daily, range);
    REQUIRE(!bars.empty());
}
