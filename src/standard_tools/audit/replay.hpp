#pragma once

#include "standard_tools/audit/record.hpp"
#include "standard_tools/audit/storage.hpp"

namespace standard_tools::audit {

DecisionRecord Replay(StoragePtr storage, const std::string& request_id);

}  // namespace standard_tools::audit
