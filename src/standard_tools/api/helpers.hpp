#pragma once

#include "standard_tools/agent/tool.hpp"
#include "standard_tools/api/state.hpp"
#include "standard_tools/audit/record.hpp"

#include <crow.h>
#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace standard_tools::api {

using json = nlohmann::json;

struct ToolCallRequest {
    std::string tool;
    std::string name;
    json arguments;
};

inline agent::ToolCall ToolCallFromRequest(const ToolCallRequest& req) {
    agent::ToolCall call;
    call.name = req.tool.empty() ? req.name : req.tool;
    call.arguments = req.arguments;
    return call;
}

inline ToolCallRequest DecodeToolCall(const crow::request& req) {
    auto body = json::parse(req.body, nullptr, false);
    if (body.is_discarded()) {
        throw std::runtime_error("invalid JSON body");
    }
    ToolCallRequest out;
    if (body.contains("tool")) out.tool = body["tool"].get<std::string>();
    if (body.contains("name")) out.name = body["name"].get<std::string>();
    if (body.contains("arguments") && !body["arguments"].is_null()) {
        out.arguments = body["arguments"];
    } else {
        out.arguments = json::object();
    }
    return out;
}

inline void RecordAudit(
    const AppState& state,
    const std::string& request_id,
    const std::string& tool_name,
    const json& input,
    const json& output,
    const std::optional<std::string>& error) {
    if (!state.audit_writer) return;

    audit::DecisionRecord rec;
    rec.request_id = request_id.empty() ? std::to_string(std::hash<std::string>{}(tool_name + input.dump())) : request_id;
    rec.tool_name = tool_name;
    rec.input = input;
    rec.output = output;
    rec.status = error ? "error" : "ok";
    if (error) rec.error = *error;
    state.audit_writer->Write(std::move(rec));
}

inline crow::response JsonResponse(int status, const json& body) {
    crow::response res;
    res.code = status;
    res.set_header("Content-Type", "application/json");
    res.body = body.dump();
    return res;
}

inline crow::response ErrorResponse(int status, const std::string& message) {
    json body = {
        {"error", message},
        {"code", status},
    };
    return JsonResponse(status, body);
}

inline int DomainErrorStatus(const std::exception& e) {
    using namespace standard_tools::core;
    if (dynamic_cast<const InvalidCommandError*>(&e)) return 400;
    if (dynamic_cast<const InvalidTickerError*>(&e)) return 400;
    if (dynamic_cast<const InvalidDateRangeError*>(&e)) return 400;
    if (dynamic_cast<const NotFoundError*>(&e)) return 404;
    if (dynamic_cast<const ProviderNotAvailableError*>(&e)) return 503;
    if (dynamic_cast<const DataQualityError*>(&e)) return 502;
    if (dynamic_cast<const InternalError*>(&e)) return 500;
    return 500;
}

}  // namespace standard_tools::api
