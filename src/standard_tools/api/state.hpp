#pragma once

#include "standard_tools/agent/dispatcher.hpp"
#include "standard_tools/audit/writer.hpp"
#include "standard_tools/marketdata/service.hpp"

#include <memory>

namespace standard_tools::api {

struct AppState {
    std::shared_ptr<agent::Dispatcher> dispatcher;
    std::shared_ptr<marketdata::Service> market_data;
    std::shared_ptr<audit::Writer> audit_writer;
};

}  // namespace standard_tools::api
