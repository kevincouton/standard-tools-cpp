#include "standard_tools/audit/replay.hpp"

#include "standard_tools/core/errors.hpp"

namespace standard_tools::audit {

DecisionRecord Replay(StoragePtr storage, const std::string& request_id) {
    if (request_id.empty()) {
        throw core::InvalidCommandError{"request_id is required"};
    }
    return storage->GetByRequestID(request_id);
}

}  // namespace standard_tools::audit
