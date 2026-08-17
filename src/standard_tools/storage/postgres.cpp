#include "standard_tools/storage/postgres.hpp"

#include <pqxx/pqxx>

#include <chrono>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace standard_tools::storage {

class PostgresPool::Impl {
public:
    explicit Impl(std::string conn_string) : conn_string_(std::move(conn_string)) {}

    const std::string& conn_string() const { return conn_string_; }

private:
    std::string conn_string_;
};

PostgresPool::PostgresPool(std::string conn_string)
    : impl_(std::make_unique<Impl>(std::move(conn_string))) {}

PostgresPool::~PostgresPool() = default;

pqxx::connection& PostgresPool::Connect() {
    // pqxx::connection is not thread-safe, so give each caller thread its own
    // connection. The number of open connections is bounded by the HTTP worker
    // thread count rather than unbounded concurrent requests.
    thread_local std::optional<pqxx::connection> conn;
    if (!conn || !conn->is_open()) {
        conn.emplace(impl_->conn_string());
    }
    return *conn;
}

bool PostgresPool::Ping() {
    try {
        auto& conn = Connect();
        pqxx::work txn(conn);
        txn.exec1("SELECT 1");
        txn.commit();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

PostgresPoolPtr NewPool(const std::string& database_url) {
    auto pool = std::make_shared<PostgresPool>(database_url);
    if (!pool->Ping()) {
        throw std::runtime_error("failed to ping database");
    }
    return pool;
}

}  // namespace standard_tools::storage
