#include "standard_tools/storage/postgres.hpp"

#include <pqxx/pqxx>

#include <chrono>
#include <stdexcept>
#include <thread>

namespace standard_tools::storage {

class PostgresPool::Impl {
public:
    explicit Impl(std::string conn_string) : conn_string_(std::move(conn_string)) {}

    pqxx::connection& Connect() {
        if (!conn_ || !conn_->is_open()) {
            conn_.emplace(conn_string_);
        }
        return *conn_;
    }

private:
    std::string conn_string_;
    std::optional<pqxx::connection> conn_;
};

PostgresPool::PostgresPool(std::string conn_string)
    : impl_(std::make_unique<Impl>(std::move(conn_string))) {}

PostgresPool::~PostgresPool() = default;

pqxx::connection& PostgresPool::Connect() {
    return impl_->Connect();
}

bool PostgresPool::Ping() {
    try {
        auto& conn = impl_->Connect();
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
