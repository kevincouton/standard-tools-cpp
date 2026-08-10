#pragma once

#include "standard_tools/audit/record.hpp"

#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace standard_tools::audit {

class NotFoundError : public std::runtime_error {
public:
    NotFoundError() : std::runtime_error("audit: no records found") {}
};

class Storage {
public:
    virtual ~Storage() = default;

    virtual void Append(const DecisionRecord& record) = 0;
    virtual DecisionRecord Latest() = 0;
    virtual DecisionRecord GetByRequestID(const std::string& request_id) = 0;
    virtual std::vector<DecisionRecord> All() = 0;
};

using StoragePtr = std::shared_ptr<Storage>;

class MemoryStorage : public Storage {
public:
    void Append(const DecisionRecord& record) override;
    DecisionRecord Latest() override;
    DecisionRecord GetByRequestID(const std::string& request_id) override;
    std::vector<DecisionRecord> All() override;

private:
    std::mutex mu_;
    std::vector<DecisionRecord> records_;
};

}  // namespace standard_tools::audit
