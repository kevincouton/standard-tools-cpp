#include "standard_tools/agent/dispatcher.hpp"
#include "standard_tools/api/a2a.hpp"
#include "standard_tools/api/mcp.hpp"
#include "standard_tools/api/rest.hpp"
#include "standard_tools/api/state.hpp"
#include "standard_tools/audit/storage.hpp"
#include "standard_tools/audit/writer.hpp"
#include "standard_tools/marketdata/service.hpp"
#include "standard_tools/marketdata/synthetic.hpp"

#include <catch2/catch_test_macros.hpp>
#include <crow.h>
#include <curl/curl.h>

#include <atomic>
#include <csignal>
#include <string>
#include <thread>

namespace {

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::pair<long, std::string> HttpGet(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    CURLcode res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) {
        return {-1, body};
    }
    return {code, body};
}

std::pair<long, std::string> HttpPost(const std::string& url, const std::string& json_body) {
    CURL* curl = curl_easy_init();
    std::string body;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    CURLcode res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    if (res != CURLE_OK) {
        return {-1, body};
    }
    return {code, body};
}

struct ServerFixture {
    using AppState = standard_tools::api::AppState;

    int port = 0;
    std::shared_ptr<crow::App<>> app;
    std::shared_ptr<AppState> state;
    std::thread thread;
    std::atomic<bool> shutdown{false};

    ServerFixture() {
        using namespace standard_tools;

        auto cache = std::make_shared<marketdata::InMemoryCache>();
        auto market_svc = std::make_shared<marketdata::Service>("synthetic", cache);
        market_svc->Register(std::make_shared<marketdata::SyntheticProvider>());

        state = std::make_shared<AppState>(AppState{
            .dispatcher = std::make_shared<agent::Dispatcher>(market_svc),
            .market_data = market_svc,
            .audit_writer = std::make_shared<audit::Writer>(std::make_shared<audit::MemoryStorage>()),
        });

        app = std::make_shared<crow::App<>>();
        api::RegisterRoutes(*app, *state);
        api::RegisterA2ARoutes(*app, *state);
        api::RegisterMCPRoutes(*app, *state);
        app->port(0).multithreaded();

        thread = std::thread([this]() {
            app->run();
        });

        // Wait for the server to start and discover the bound port.
        for (int i = 0; i < 100 && port == 0; ++i) {
            port = app->port();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        REQUIRE(port != 0);
    }

    ~ServerFixture() {
        shutdown.store(true);
        app->stop();
        thread.join();
    }
};

}  // namespace

TEST_CASE("Server health and agent endpoints", "[e2e]") {
    ServerFixture fixture;
    std::string base = "http://127.0.0.1:" + std::to_string(fixture.port);

    auto [health_code, health_body] = HttpGet(base + "/health");
    REQUIRE(health_code == 200);
    REQUIRE(health_body.find("ok") != std::string::npos);

    auto [tools_code, tools_body] = HttpGet(base + "/api/v1/agent/tools");
    REQUIRE(tools_code == 200);
    REQUIRE(tools_body.find("fetch_ohlcv") != std::string::npos);

    auto [dispatch_code, dispatch_body] = HttpPost(
        base + "/api/v1/agent/dispatch",
        R"({"tool":"health","arguments":{}})");
    REQUIRE(dispatch_code == 200);
    REQUIRE(dispatch_body.find("ok") != std::string::npos);

    auto [market_code, market_body] = HttpGet(
        base + "/api/v1/market-data/AAPL?start=2024-01-01&end=2024-01-03&interval=daily");
    REQUIRE(market_code == 200);
    REQUIRE(market_body.find("open") != std::string::npos);

    auto [a2a_code, a2a_body] = HttpGet(base + "/a2a/agent.json");
    REQUIRE(a2a_code == 200);
    REQUIRE(a2a_body.find("standard-tools-cpp") != std::string::npos);

    auto [mcp_code, mcp_body] = HttpGet(base + "/mcp/capabilities");
    REQUIRE(mcp_code == 200);
    REQUIRE(mcp_body.find("protocolVersion") != std::string::npos);
}
