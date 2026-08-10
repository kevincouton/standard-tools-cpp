#include "standard_tools/audit/postgres.hpp"
#include "standard_tools/audit/verifier.hpp"
#include "standard_tools/audit/writer.hpp"
#include "standard_tools/storage/migrate.hpp"
#include "standard_tools/storage/postgres.hpp"

#include <catch2/catch_test_macros.hpp>
#include <pqxx/pqxx>

#include <cstdlib>

using namespace standard_tools;

TEST_CASE("PostgresStorage round-trips and verifies chain", "[integration]") {
    const char* url = std::getenv("SQT_DATABASE_URL");
    if (!url) {
        SKIP("SQT_DATABASE_URL not set");
    }

    auto pool = storage::NewPool(url);
    storage::MigrateUp(*pool);
    auto pg_storage = std::make_shared<audit::PostgresStorage>(pool);

    // Clear table for idempotent test run.
    {
        auto& conn = pool->Connect();
        pqxx::work txn(conn);
        txn.exec("TRUNCATE audit_records RESTART IDENTITY CASCADE");
        txn.commit();
    }

    audit::Writer writer(pg_storage);

    audit::DecisionRecord r1;
    r1.request_id = "int-req-1";
    r1.tool_name = "health";
    r1.input = nullptr;
    r1.output = {{"status", "ok"}};
    r1.status = "ok";
    writer.Write(r1);

    audit::DecisionRecord r2;
    r2.request_id = "int-req-2";
    r2.tool_name = "list_tools";
    r2.input = nullptr;
    r2.output = audit::json::array();
    r2.status = "ok";
    writer.Write(r2);

    auto latest = pg_storage->Latest();
    REQUIRE(latest.request_id == "int-req-2");

    auto found = pg_storage->GetByRequestID("int-req-1");
    REQUIRE(found.tool_name == "health");

    audit::Verifier verifier(pg_storage);
    REQUIRE_NOTHROW(verifier.VerifyChain());

    storage::MigrateDown(*pool);
}
