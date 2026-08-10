#include "standard_tools/audit/writer.hpp"

#include "standard_tools/audit/record.hpp"

#include <utility>

namespace standard_tools::audit {

namespace {

std::pair<json, std::string> CanonicalizeAndHash(const json& value) {
    json canonical = value.is_null() ? json(nullptr) : json(value);
    return {canonical, HashJson(canonical)};
}

}  // namespace

Writer::Writer(StoragePtr storage) : storage_(std::move(storage)) {}

void Writer::Write(DecisionRecord record) {
    std::lock_guard<std::mutex> lock(mu_);

    if (record.recorded_at == std::chrono::system_clock::time_point{}) {
        record.recorded_at = Now();
    }

    try {
        auto latest = storage_->Latest();
        if (!latest.record_hash.empty()) {
            record.prev_record_hash = latest.record_hash;
        }
    } catch (const NotFoundError&) {
        // First record.
    }

    auto [input_json, input_hash] = CanonicalizeAndHash(record.input);
    record.input = std::move(input_json);
    record.input_hash = std::move(input_hash);

    auto [output_json, output_hash] = CanonicalizeAndHash(record.output);
    record.output = std::move(output_json);
    record.output_hash = std::move(output_hash);

    record.record_hash = HashRecord(record);
    storage_->Append(record);
}

}  // namespace standard_tools::audit
