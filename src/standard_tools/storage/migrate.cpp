#include "standard_tools/storage/migrate.hpp"

#include <pqxx/pqxx>

#include <stdexcept>

namespace standard_tools::storage {

std::vector<Migration> Migrations() {
    return {
        Migration{
            .name = "000001_create_audit_table",
            .up_sql = R"(
CREATE TABLE IF NOT EXISTS audit_records (
    id BIGSERIAL PRIMARY KEY,
    request_id TEXT NOT NULL UNIQUE,
    recorded_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    tool_name TEXT NOT NULL,
    input_hash TEXT NOT NULL,
    output_hash TEXT NOT NULL,
    status TEXT NOT NULL,
    error TEXT,
    git_commit_sha TEXT,
    package_version TEXT,
    random_seed BIGINT,
    prev_record_hash TEXT,
    record_hash TEXT NOT NULL,
    input_json TEXT NOT NULL,
    output_json TEXT NOT NULL,
    raw JSONB NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_audit_recorded_at ON audit_records(recorded_at);
CREATE INDEX IF NOT EXISTS idx_audit_tool_name ON audit_records(tool_name);
)",
            .down_sql = "DROP TABLE IF EXISTS audit_records;",
        },
    };
}

namespace {

void EnsureMigrationsTable(PostgresPool& pool) {
    auto& conn = pool.Connect();
    pqxx::work txn(conn);
    txn.exec(R"(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            version TEXT PRIMARY KEY,
            applied_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
        )
    )");
    txn.commit();
}

std::vector<std::string> AppliedVersions(PostgresPool& pool) {
    auto& conn = pool.Connect();
    pqxx::work txn(conn);
    auto result = txn.exec("SELECT version FROM schema_migrations ORDER BY version");
    txn.commit();
    std::vector<std::string> versions;
    for (const auto& row : result) {
        versions.push_back(row[0].as<std::string>());
    }
    return versions;
}

void MarkApplied(PostgresPool& pool, const std::string& version) {
    auto& conn = pool.Connect();
    pqxx::work txn(conn);
    txn.exec_params("INSERT INTO schema_migrations (version) VALUES ($1) ON CONFLICT DO NOTHING", version);
    txn.commit();
}

void MarkRolledBack(PostgresPool& pool, const std::string& version) {
    auto& conn = pool.Connect();
    pqxx::work txn(conn);
    txn.exec_params("DELETE FROM schema_migrations WHERE version = $1", version);
    txn.commit();
}

}  // namespace

void MigrateUp(PostgresPool& pool) {
    EnsureMigrationsTable(pool);
    auto applied = AppliedVersions(pool);
    size_t applied_count = 0;
    for (const auto& migration : Migrations()) {
        if (std::find(applied.begin(), applied.end(), migration.name) != applied.end()) {
            continue;
        }
        auto& conn = pool.Connect();
        pqxx::work txn(conn);
        txn.exec(migration.up_sql);
        txn.commit();
        MarkApplied(pool, migration.name);
        ++applied_count;
    }
    if (applied_count == 0) {
        // No change is fine.
    }
}

void MigrateDown(PostgresPool& pool) {
    EnsureMigrationsTable(pool);
    auto migrations = Migrations();
    auto applied = AppliedVersions(pool);
    for (auto it = migrations.rbegin(); it != migrations.rend(); ++it) {
        if (std::find(applied.begin(), applied.end(), it->name) == applied.end()) {
            continue;
        }
        auto& conn = pool.Connect();
        pqxx::work txn(conn);
        txn.exec(it->down_sql);
        txn.commit();
        MarkRolledBack(pool, it->name);
    }
}

}  // namespace standard_tools::storage
