#include "standard_tools/audit/verifier.hpp"

#include "standard_tools/audit/record.hpp"

#include <stdexcept>

namespace standard_tools::audit {

Verifier::Verifier(StoragePtr storage) : storage_(std::move(storage)) {}

void Verifier::VerifyChain() {
    auto records = storage_->All();
    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto& r = records[i];
        if (HashJson(r.input) != r.input_hash) {
            throw std::runtime_error("input hash mismatch at " + r.request_id);
        }
        if (HashJson(r.output) != r.output_hash) {
            throw std::runtime_error("output hash mismatch at " + r.request_id);
        }
        if (HashRecord(r) != r.record_hash) {
            throw std::runtime_error("record hash mismatch at " + r.request_id);
        }
        if (i > 0 && r.prev_record_hash != records[i - 1].record_hash) {
            throw std::runtime_error("chain mismatch at " + r.request_id);
        }
    }
}

}  // namespace standard_tools::audit
