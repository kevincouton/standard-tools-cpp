#pragma once

#include "standard_tools/storage/postgres.hpp"

#include <string>
#include <vector>

namespace standard_tools::storage {

struct Migration {
    std::string name;
    std::string up_sql;
    std::string down_sql;
};

std::vector<Migration> Migrations();

void MigrateUp(PostgresPool& pool);
void MigrateDown(PostgresPool& pool);

}  // namespace standard_tools::storage
