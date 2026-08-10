#pragma once

#include <memory>
#include <string>

namespace pqxx {
class connection;
}

namespace standard_tools::storage {

class PostgresPool {
public:
    explicit PostgresPool(std::string conn_string);
    ~PostgresPool();

    PostgresPool(const PostgresPool&) = delete;
    PostgresPool& operator=(const PostgresPool&) = delete;

    pqxx::connection& Connect();
    bool Ping();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

using PostgresPoolPtr = std::shared_ptr<PostgresPool>;

PostgresPoolPtr NewPool(const std::string& database_url);

}  // namespace standard_tools::storage
