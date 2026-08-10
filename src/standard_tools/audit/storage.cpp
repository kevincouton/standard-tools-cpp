#include "standard_tools/audit/storage.hpp"

namespace standard_tools::audit {

void MemoryStorage::Append(const DecisionRecord& record) {
    std::lock_guard<std::mutex> lock(mu_);
    records_.push_back(record);
}

DecisionRecord MemoryStorage::Latest() {
    std::lock_guard<std::mutex> lock(mu_);
    if (records_.empty()) {
        throw NotFoundError{};
    }
    return records_.back();
}

DecisionRecord MemoryStorage::GetByRequestID(const std::string& request_id) {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto it = records_.rbegin(); it != records_.rend(); ++it) {
        if (it->request_id == request_id) {
            return *it;
        }
    }
    throw NotFoundError{};
}

std::vector<DecisionRecord> MemoryStorage::All() {
    std::lock_guard<std::mutex> lock(mu_);
    return records_;
}

}  // namespace standard_tools::audit
