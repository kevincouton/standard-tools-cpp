#include "standard_tools/audit/postgres.hpp"

#include <date/date.h>
#include <pqxx/pqxx>

#include <sstream>

namespace standard_tools::audit {

namespace {

std::string FormatTimestamp(std::chrono::system_clock::time_point tp) {
    return date::format("%F %T%z", tp);
}

std::chrono::system_clock::time_point ParseTimestamp(const std::string& s) {
    std::istringstream in(s);
    std::chrono::system_clock::time_point tp;
    in >> date::parse("%F %T%z", tp);
    if (in.fail()) {
        throw std::runtime_error("invalid timestamp: " + s);
    }
    return tp;
}

template <typename Row>
DecisionRecord RowToRecord(const Row& row) {
    DecisionRecord r;
    r.request_id = row["request_id"].template as<std::string>();
    r.recorded_at = ParseTimestamp(row["recorded_at"].template as<std::string>());
    r.tool_name = row["tool_name"].template as<std::string>();
    r.input_hash = row["input_hash"].template as<std::string>();
    r.output_hash = row["output_hash"].template as<std::string>();
    r.status = row["status"].template as<std::string>();
    if (!row["error"].is_null()) r.error = row["error"].template as<std::string>();
    if (!row["git_commit_sha"].is_null()) r.git_commit_sha = row["git_commit_sha"].template as<std::string>();
    if (!row["package_version"].is_null()) r.package_version = row["package_version"].template as<std::string>();
    if (!row["random_seed"].is_null()) r.random_seed = row["random_seed"].template as<std::int64_t>();
    if (!row["prev_record_hash"].is_null()) r.prev_record_hash = row["prev_record_hash"].template as<std::string>();
    r.record_hash = row["record_hash"].template as<std::string>();
    r.input = json::parse(row["input_json"].template as<std::string>());
    r.output = json::parse(row["output_json"].template as<std::string>());
    return r;
}

}  // namespace

PostgresStorage::PostgresStorage(storage::PostgresPoolPtr pool) : pool_(std::move(pool)) {}

void PostgresStorage::Append(const DecisionRecord& record) {
    auto& conn = pool_->Connect();
    pqxx::work txn(conn);
    txn.exec_params(
        R"(
            INSERT INTO audit_records (
                request_id, recorded_at, tool_name, input_hash, output_hash,
                status, error, git_commit_sha, package_version, random_seed,
                prev_record_hash, record_hash, input_json, output_json, raw
            ) VALUES (
                $1, $2, $3, $4, $5, $6,
                NULLIF($7, ''),
                NULLIF($8, ''),
                NULLIF($9, ''),
                $10,
                NULLIF($11, ''),
                $12, $13, $14, $15
            )
        )",
        record.request_id,
        FormatTimestamp(record.recorded_at),
        record.tool_name,
        record.input_hash,
        record.output_hash,
        record.status,
        record.error,
        record.git_commit_sha,
        record.package_version,
        record.random_seed,
        record.prev_record_hash,
        record.record_hash,
        record.input.dump(),
        record.output.dump(),
        json{{"input", record.input}, {"output", record.output}}.dump());
    txn.commit();
}

DecisionRecord PostgresStorage::Latest() {
    auto& conn = pool_->Connect();
    pqxx::work txn(conn);
    auto result = txn.exec("SELECT * FROM audit_records ORDER BY id DESC LIMIT 1");
    txn.commit();
    if (result.empty()) {
        throw NotFoundError{};
    }
    return RowToRecord(result[0]);
}

DecisionRecord PostgresStorage::GetByRequestID(const std::string& request_id) {
    auto& conn = pool_->Connect();
    pqxx::work txn(conn);
    auto result = txn.exec_params("SELECT * FROM audit_records WHERE request_id = $1", request_id);
    txn.commit();
    if (result.empty()) {
        throw NotFoundError{};
    }
    return RowToRecord(result[0]);
}

std::vector<DecisionRecord> PostgresStorage::All() {
    auto& conn = pool_->Connect();
    pqxx::work txn(conn);
    auto result = txn.exec("SELECT * FROM audit_records ORDER BY id ASC");
    txn.commit();
    std::vector<DecisionRecord> records;
    for (const auto& row : result) {
        records.push_back(RowToRecord(row));
    }
    return records;
}

}  // namespace standard_tools::audit
