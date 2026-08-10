#include "standard_tools/audit/storage.hpp"
#include "standard_tools/audit/verifier.hpp"
#include "standard_tools/audit/replay.hpp"
#include "standard_tools/config/config.hpp"

#ifdef STANDARD_TOOLS_ENABLE_POSTGRES
#include "standard_tools/audit/postgres.hpp"
#include "standard_tools/storage/migrate.hpp"
#include "standard_tools/storage/postgres.hpp"
#endif

#include <CLI/App.hpp>
#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>

using namespace standard_tools;
using json = nlohmann::json;

namespace {

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
    CLI::App app{"Standard Quant Tools CLI (C++)"};
    app.require_subcommand(1);

    auto verify_cmd = app.add_subcommand("verify", "Verify the integrity of the audit chain");
    auto report_cmd = app.add_subcommand("report", "Print an audit record as JSON");
    std::string report_id;
    report_cmd->add_option("request-id", report_id, "Request ID to report")->required();

    auto replay_cmd = app.add_subcommand("replay", "Replay an audit record by request ID");
    std::string replay_id;
    replay_cmd->add_option("request-id", replay_id, "Request ID to replay")->required();

    auto keygen_cmd = app.add_subcommand("keygen", "Generate a random 256-bit key");
    auto anchor_cmd = app.add_subcommand("anchor", "Print the latest audit record hash (chain tip)");

    CLI11_PARSE(app, argc, argv);

    config::Config cfg;
    try {
        cfg = config::Load();
    } catch (const std::exception& e) {
        std::cerr << "failed to load config: " << e.what() << "\n";
        return 1;
    }

    try {
        if (*verify_cmd) {
            auto storage = OpenStorage(cfg);
            audit::Verifier verifier(storage);
            verifier.VerifyChain();
            std::cout << "audit chain OK\n";
        } else if (*report_cmd) {
            auto storage = OpenStorage(cfg);
            auto record = storage->GetByRequestID(report_id);
            std::cout << json(record).dump(2) << "\n";
        } else if (*replay_cmd) {
            auto storage = OpenStorage(cfg);
            auto record = audit::Replay(storage, replay_id);
            std::cout << json(record).dump(2) << "\n";
        } else if (*keygen_cmd) {
            std::random_device rd;
            std::uniform_int_distribution<int> dist(0, 255);
            std::ostringstream oss;
            for (int i = 0; i < 32; ++i) {
                oss << std::hex << std::setw(2) << std::setfill('0') << dist(rd);
            }
            std::cout << oss.str() << "\n";
        } else if (*anchor_cmd) {
            auto storage = OpenStorage(cfg);
            try {
                auto latest = storage->Latest();
                std::cout << latest.record_hash << "\n";
            } catch (const audit::NotFoundError&) {
                std::cout << "no audit records\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
