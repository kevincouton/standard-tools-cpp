#include "standard_tools/agent/dispatcher.hpp"
#include "standard_tools/analysis/calculator.hpp"
#include "standard_tools/api/a2a.hpp"
#include "standard_tools/api/auth.hpp"
#include "standard_tools/api/mcp.hpp"
#include "standard_tools/api/rest.hpp"
#include "standard_tools/api/state.hpp"
#include "standard_tools/audit/storage.hpp"
#include "standard_tools/audit/writer.hpp"
#include "standard_tools/config/config.hpp"
#include "standard_tools/indicators/calculator.hpp"
#include "standard_tools/marketdata/service.hpp"
#include "standard_tools/marketdata/synthetic.hpp"
#include "standard_tools/metrics/risk_return.hpp"
#include "standard_tools/screener/hardcoded_provider.hpp"
#include "standard_tools/screener/service.hpp"

#ifdef STANDARD_TOOLS_ENABLE_GRPC
#include "standard_tools/api/grpc_server.hpp"
#endif

#ifdef STANDARD_TOOLS_ENABLE_POSTGRES
#include "standard_tools/audit/postgres.hpp"
#include "standard_tools/storage/migrate.hpp"
#include "standard_tools/storage/postgres.hpp"
#endif

#include <crow.h>

#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

using namespace standard_tools;

namespace {

std::atomic<bool> g_shutdown{false};

void SignalHandler(int) {
    g_shutdown.store(true);
}

audit::StoragePtr OpenStorage(const config::Config& cfg) {
    if (!cfg.database_url.empty()) {
#ifdef STANDARD_TOOLS_ENABLE_POSTGRES
        auto pool = storage::NewPool(cfg.database_url);
        storage::MigrateUp(*pool);
        return std::make_shared<audit::PostgresStorage>(pool);
#else
        throw std::runtime_error("PostgreSQL support is not compiled in");
#endif
    }
    return std::make_shared<audit::MemoryStorage>();
}

}  // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    config::Config cfg;
    try {
        cfg = config::Load();
    } catch (const std::exception& e) {
        std::cerr << "failed to load config: " << e.what() << "\n";
        return 1;
    }

    audit::StoragePtr audit_storage;
    try {
        audit_storage = OpenStorage(cfg);
    } catch (const std::exception& e) {
        std::cerr << "failed to setup storage: " << e.what() << "\n";
        return 1;
    }

    // Fail closed: refuse to start when auth is enabled without a key.
    if (cfg.auth_enabled && cfg.api_key.empty()) {
        std::cerr << "SQT_AUTH_ENABLED is true but SQT_API_KEY is not set; refusing to start\n";
        return 1;
    }
    api::ConfigureAuth(cfg.auth_enabled, cfg.api_key);

    auto cache = std::make_shared<marketdata::InMemoryCache>();
    auto market_svc = std::make_shared<marketdata::Service>("synthetic", cache);
    market_svc->Register(std::make_shared<marketdata::SyntheticProvider>());

    auto indicators = std::make_shared<indicators::IndicatorCalculator>();
    auto metrics_calc = std::make_shared<metrics::RiskReturnCalculator>();
    auto analysis_calc = std::make_shared<analysis::AnalysisCalculator>();
    auto screener = std::make_shared<screener::Screener>(
        std::make_shared<screener::HardcodedFundamentalProvider>());
    auto dispatcher = std::make_shared<agent::Dispatcher>(
        market_svc, indicators, metrics_calc, analysis_calc, screener);

    api::AppState state{
        .dispatcher = dispatcher,
        .market_data = market_svc,
        .audit_writer = std::make_shared<audit::Writer>(audit_storage),
        .indicators = indicators,
        .metrics = metrics_calc,
        .analysis = analysis_calc,
        .screener = screener,
    };

    api::App app;
    api::RegisterRoutes(app, state);
    api::RegisterA2ARoutes(app, state);
    api::RegisterMCPRoutes(app, state);
    app.port(cfg.http_port).multithreaded();

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    std::thread http_thread([&app]() { app.run(); });

#ifdef STANDARD_TOOLS_ENABLE_GRPC
    std::string grpc_addr = "0.0.0.0:" + std::to_string(cfg.grpc_port);
    auto grpc_server = api::StartGrpcHealthServer(grpc_addr);
    std::thread grpc_thread([&grpc_server]() {
        if (grpc_server) grpc_server->Wait();
    });
    std::cout << "starting server http=" << cfg.http_port << " grpc=" << cfg.grpc_port << "\n";
#else
    std::cout << "starting server http=" << cfg.http_port << " (gRPC disabled)\n";
#endif

    while (!g_shutdown.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    app.stop();
#ifdef STANDARD_TOOLS_ENABLE_GRPC
    grpc_server->Shutdown();
    grpc_thread.join();
#endif
    http_thread.join();
    return 0;
}
