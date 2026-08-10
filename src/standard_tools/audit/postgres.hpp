#pragma once

#include "standard_tools/audit/storage.hpp"
#include "standard_tools/storage/postgres.hpp"

namespace standard_tools::audit {

class PostgresStorage : public Storage {
public:
    explicit PostgresStorage(storage::PostgresPoolPtr pool);

    void Append(const DecisionRecord& record) override;
    DecisionRecord Latest() override;
    DecisionRecord GetByRequestID(const std::string& request_id) override;
    std::vector<DecisionRecord> All() override;

private:
    storage::PostgresPoolPtr pool_;
};

}  // namespace standard_tools::audit
