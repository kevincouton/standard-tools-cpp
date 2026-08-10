#include "standard_tools/audit/record.hpp"
#include "standard_tools/audit/storage.hpp"
#include "standard_tools/audit/verifier.hpp"
#include "standard_tools/audit/writer.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace standard_tools::audit;

TEST_CASE("MemoryStorage stores and retrieves records", "[audit]") {
    MemoryStorage storage;
    DecisionRecord r;
    r.request_id = "req-1";
    r.tool_name = "health";
    r.input = nullptr;
    r.output = {{"status", "ok"}};
    r.status = "ok";
    storage.Append(r);

    auto latest = storage.Latest();
    REQUIRE(latest.request_id == "req-1");

    auto found = storage.GetByRequestID("req-1");
    REQUIRE(found.tool_name == "health");
}

TEST_CASE("Writer hashes and chains records", "[audit]") {
    auto storage = std::make_shared<MemoryStorage>();
    Writer writer(storage);

    DecisionRecord r1;
    r1.request_id = "req-1";
    r1.tool_name = "health";
    r1.input = nullptr;
    r1.output = {{"status", "ok"}};
    r1.status = "ok";
    writer.Write(r1);

    DecisionRecord r2;
    r2.request_id = "req-2";
    r2.tool_name = "list_tools";
    r2.input = nullptr;
    r2.output = json::array();
    r2.status = "ok";
    writer.Write(r2);

    auto records = storage->All();
    REQUIRE(records.size() == 2);
    REQUIRE(!records[0].record_hash.empty());
    REQUIRE(!records[1].record_hash.empty());
    REQUIRE(records[1].prev_record_hash == records[0].record_hash);

    Verifier verifier(storage);
    REQUIRE_NOTHROW(verifier.VerifyChain());
}

TEST_CASE("Verifier detects tampering", "[audit]") {
    auto storage = std::make_shared<MemoryStorage>();
    Writer writer(storage);

    DecisionRecord r;
    r.request_id = "req-1";
    r.tool_name = "health";
    r.input = nullptr;
    r.output = {{"status", "ok"}};
    r.status = "ok";
    writer.Write(r);

    auto records = storage->All();
    records[0].output["status"] = "bad";
    storage = std::make_shared<MemoryStorage>();
    storage->Append(records[0]);

    Verifier verifier(storage);
    REQUIRE_THROWS(verifier.VerifyChain());
}
