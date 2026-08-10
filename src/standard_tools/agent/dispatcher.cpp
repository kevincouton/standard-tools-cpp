#include "standard_tools/agent/dispatcher.hpp"

#include "standard_tools/core/errors.hpp"
#include "standard_tools/core/json_serialization.hpp"
#include "standard_tools/core/value_objects.hpp"

namespace standard_tools::agent {

namespace {

core::Date ParseDateParam(const std::string& name, const json& args) {
    if (!args.contains(name) || !args[name].is_string()) {
        throw core::InvalidCommandError{name + " date is required"};
    }
    return core::ParseDate(args[name].get<std::string>());
}

}  // namespace

Dispatcher::Dispatcher(std::shared_ptr<marketdata::Service> market_data)
    : market_data_(std::move(market_data)) {}

ToolResult Dispatcher::Dispatch(const ToolCall& call) {
    if (!FindTool(call.name)) {
        throw core::InvalidCommandError{"unknown tool " + call.name};
    }
    if (call.name == ToolHealth) {
        return OkResult(json{{"status", "ok"}});
    }
    if (call.name == ToolListTools) {
        std::vector<std::string> names;
        for (const auto& tool : ListTools()) {
            names.push_back(tool.name);
        }
        return OkResult(json(names));
    }
    if (call.name == ToolFetchOhlcv) {
        return FetchOhlcv(call.arguments);
    }
    throw core::InvalidCommandError{"unknown tool " + call.name};
}

ToolResult Dispatcher::FetchOhlcv(const json& args) {
    auto ticker = core::Ticker(args.value("ticker", ""));
    auto start = ParseDateParam("start", args);
    auto end = ParseDateParam("end", args);
    auto range = core::DateRange(start, end);
    auto interval = core::ParseBarInterval(args.value("interval", "daily"));
    auto provider = args.value("provider", "");
    auto series = market_data_->Fetch(ticker, interval, range, provider);
    return OkResult(json(series));
}

}  // namespace standard_tools::agent
