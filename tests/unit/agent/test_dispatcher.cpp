#include "standard_tools/agent/dispatcher.hpp"
#include "standard_tools/analysis/calculator.hpp"
#include "standard_tools/indicators/calculator.hpp"
#include "standard_tools/marketdata/service.hpp"
#include "standard_tools/marketdata/synthetic.hpp"
#include "standard_tools/metrics/risk_return.hpp"
#include "standard_tools/screener/hardcoded_provider.hpp"
#include "standard_tools/screener/service.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace standard_tools;

namespace {

std::shared_ptr<agent::Dispatcher> MakeDispatcher(
    std::shared_ptr<marketdata::Service> market_svc) {
    auto indicators = std::make_shared<indicators::IndicatorCalculator>();
    auto metrics_calc = std::make_shared<metrics::RiskReturnCalculator>();
    auto analysis_calc = std::make_shared<analysis::AnalysisCalculator>();
    auto screener = std::make_shared<screener::Screener>(
        std::make_shared<screener::HardcodedFundamentalProvider>());
    return std::make_shared<agent::Dispatcher>(
        market_svc, indicators, metrics_calc, analysis_calc, screener);
}

}  // namespace

TEST_CASE("Dispatcher returns health", "[agent]") {
    auto cache = std::make_shared<marketdata::InMemoryCache>();
    auto svc = std::make_shared<marketdata::Service>("synthetic", cache);
    svc->Register(std::make_shared<marketdata::SyntheticProvider>());
    auto dispatcher = MakeDispatcher(svc);

    auto result = dispatcher->Dispatch({agent::ToolHealth, nullptr});
    REQUIRE(result.output["status"] == "ok");
}

TEST_CASE("Dispatcher lists tools", "[agent]") {
    auto cache = std::make_shared<marketdata::InMemoryCache>();
    auto svc = std::make_shared<marketdata::Service>("synthetic", cache);
    svc->Register(std::make_shared<marketdata::SyntheticProvider>());
    auto dispatcher = MakeDispatcher(svc);

    auto result = dispatcher->Dispatch({agent::ToolListTools, nullptr});
    REQUIRE(result.output.is_array());
    REQUIRE(!result.output.empty());
}

TEST_CASE("Dispatcher fetches OHLCV", "[agent]") {
    auto cache = std::make_shared<marketdata::InMemoryCache>();
    auto svc = std::make_shared<marketdata::Service>("synthetic", cache);
    svc->Register(std::make_shared<marketdata::SyntheticProvider>());
    auto dispatcher = MakeDispatcher(svc);

    agent::ToolCall call;
    call.name = agent::ToolFetchOhlcv;
    call.arguments = {
        {"ticker", "AAPL"},
        {"start", "2024-01-01"},
        {"end", "2024-01-03"},
    };
    auto result = dispatcher->Dispatch(call);
    REQUIRE(result.output.is_array());
    REQUIRE(result.output.size() == 3);
}

TEST_CASE("Dispatcher rejects unknown tools", "[agent]") {
    auto cache = std::make_shared<marketdata::InMemoryCache>();
    auto svc = std::make_shared<marketdata::Service>("synthetic", cache);
    auto dispatcher = MakeDispatcher(svc);
    REQUIRE_THROWS(dispatcher->Dispatch({"unknown", nullptr}));
}

TEST_CASE("Dispatcher calculates indicator", "[agent]") {
    auto cache = std::make_shared<marketdata::InMemoryCache>();
    auto svc = std::make_shared<marketdata::Service>("synthetic", cache);
    svc->Register(std::make_shared<marketdata::SyntheticProvider>());
    auto dispatcher = MakeDispatcher(svc);

    agent::ToolCall call;
    call.name = agent::ToolCalculateIndicator;
    call.arguments = {
        {"indicator", "sma"},
        {"ticker", "AAPL"},
        {"start", "2024-01-01"},
        {"end", "2024-01-31"},
        {"params", {{"period", 10}}},
    };
    auto result = dispatcher->Dispatch(call);
    REQUIRE(result.output["name"] == "sma");
    REQUIRE(result.output.contains("values"));
}

TEST_CASE("Dispatcher runs analysis", "[agent]") {
    auto cache = std::make_shared<marketdata::InMemoryCache>();
    auto svc = std::make_shared<marketdata::Service>("synthetic", cache);
    svc->Register(std::make_shared<marketdata::SyntheticProvider>());
    auto dispatcher = MakeDispatcher(svc);

    agent::ToolCall call;
    call.name = agent::ToolRunAnalysis;
    call.arguments = {
        {"operation", "regression"},
        {"request",
         {{"asset_returns", {0.01, 0.02, -0.01, 0.005}},
          {"benchmark_returns", {0.005, 0.015, -0.005, 0.0}}}},
    };
    auto result = dispatcher->Dispatch(call);
    REQUIRE(result.output.contains("alpha"));
    REQUIRE(result.output.contains("beta"));
}

TEST_CASE("Dispatcher screens stocks", "[agent]") {
    auto cache = std::make_shared<marketdata::InMemoryCache>();
    auto svc = std::make_shared<marketdata::Service>("synthetic", cache);
    svc->Register(std::make_shared<marketdata::SyntheticProvider>());
    auto dispatcher = MakeDispatcher(svc);

    agent::ToolCall call;
    call.name = agent::ToolScreenStocks;
    call.arguments = {
        {"tickers", {"AAPL", "MSFT", "TSLA"}},
        {"criteria", {{"pe_ratio_max", 30.0}}},
    };
    auto result = dispatcher->Dispatch(call);
    REQUIRE(result.output.contains("matches"));
    REQUIRE(result.output.contains("failed"));
}
