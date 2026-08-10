#include "standard_tools/agent/dispatcher.hpp"
#include "standard_tools/marketdata/service.hpp"
#include "standard_tools/marketdata/synthetic.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace standard_tools;

TEST_CASE("Dispatcher returns health", "[agent]") {
    auto cache = std::make_shared<marketdata::InMemoryCache>();
    auto svc = std::make_shared<marketdata::Service>("synthetic", cache);
    svc->Register(std::make_shared<marketdata::SyntheticProvider>());
    agent::Dispatcher dispatcher(svc);

    auto result = dispatcher.Dispatch({agent::ToolHealth, nullptr});
    REQUIRE(result.output["status"] == "ok");
}

TEST_CASE("Dispatcher lists tools", "[agent]") {
    auto cache = std::make_shared<marketdata::InMemoryCache>();
    auto svc = std::make_shared<marketdata::Service>("synthetic", cache);
    svc->Register(std::make_shared<marketdata::SyntheticProvider>());
    agent::Dispatcher dispatcher(svc);

    auto result = dispatcher.Dispatch({agent::ToolListTools, nullptr});
    REQUIRE(result.output.is_array());
    REQUIRE(!result.output.empty());
}

TEST_CASE("Dispatcher fetches OHLCV", "[agent]") {
    auto cache = std::make_shared<marketdata::InMemoryCache>();
    auto svc = std::make_shared<marketdata::Service>("synthetic", cache);
    svc->Register(std::make_shared<marketdata::SyntheticProvider>());
    agent::Dispatcher dispatcher(svc);

    agent::ToolCall call;
    call.name = agent::ToolFetchOhlcv;
    call.arguments = {
        {"ticker", "AAPL"},
        {"start", "2024-01-01"},
        {"end", "2024-01-03"},
    };
    auto result = dispatcher.Dispatch(call);
    REQUIRE(result.output.is_array());
    REQUIRE(result.output.size() == 3);
}

TEST_CASE("Dispatcher rejects unknown tools", "[agent]") {
    auto cache = std::make_shared<marketdata::InMemoryCache>();
    auto svc = std::make_shared<marketdata::Service>("synthetic", cache);
    agent::Dispatcher dispatcher(svc);
    REQUIRE_THROWS(dispatcher.Dispatch({"unknown", nullptr}));
}
