#pragma once

#include "standard_tools/agent/tool.hpp"
#include "standard_tools/marketdata/service.hpp"

#include <memory>

namespace standard_tools::agent {

class Dispatcher {
public:
    explicit Dispatcher(std::shared_ptr<marketdata::Service> market_data);

    ToolResult Dispatch(const ToolCall& call);

private:
    ToolResult FetchOhlcv(const json& args);

    std::shared_ptr<marketdata::Service> market_data_;
};

}  // namespace standard_tools::agent
