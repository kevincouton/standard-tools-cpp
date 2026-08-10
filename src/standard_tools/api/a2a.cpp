#include "standard_tools/api/a2a.hpp"

#include "standard_tools/agent/tool.hpp"
#include "standard_tools/api/helpers.hpp"

namespace standard_tools::api {

namespace {

constexpr const char* kAppName = "standard-tools-cpp";
constexpr const char* kAppVersion = "0.1.0";
constexpr const char* kAppDescription = "Quantitative finance toolkit agent";

std::string RequestScheme(const crow::request& req) {
    auto forwarded = req.get_header_value("X-Forwarded-Proto");
    if (!forwarded.empty()) return forwarded;
    return "http";
}

}  // namespace

crow::App<>& RegisterA2ARoutes(crow::App<>& app, AppState& state) {
    CROW_ROUTE(app, "/a2a/agent.json").methods(crow::HTTPMethod::GET)
    ([](const crow::request& req, crow::response& res) {
        json card = {
            {"name", kAppName},
            {"description", kAppDescription},
            {"version", kAppVersion},
            {"url", RequestScheme(req) + "://" + req.get_header_value("Host") + "/a2a"},
            {"capabilities", {{"streaming", false}, {"pushNotifications", false}}},
            {"skills", json::array()},
        };
        res.set_header("Content-Type", "application/json");
        res.body = card.dump();
        res.end();
    });

    CROW_ROUTE(app, "/a2a/tasks").methods(crow::HTTPMethod::POST)
    ([&state](const crow::request& req, crow::response& res) {
        ToolCallRequest tcr;
        std::string id = "task-" + std::to_string(std::hash<std::string>{}(req.body));
        try {
            tcr = DecodeToolCall(req);
            if (tcr.tool.empty() && tcr.name.empty()) {
                res = ErrorResponse(400, "tool or name is required");
                res.end();
                return;
            }
            auto result = state.dispatcher->Dispatch(ToolCallFromRequest(tcr));
            RecordAudit(state, id, tcr.tool.empty() ? tcr.name : tcr.tool, tcr.arguments, result.output, std::nullopt);
            json body = {
                {"id", id},
                {"status", "completed"},
                {"result", {{"output", result.output}, {"error", nullptr}}},
            };
            res = JsonResponse(200, body);
        } catch (const std::exception& e) {
            RecordAudit(state, id, tcr.tool.empty() ? tcr.name : tcr.tool, tcr.arguments, json(nullptr), e.what());
            json body = {
                {"id", id},
                {"status", "failed"},
                {"result", {{"output", nullptr}, {"error", e.what()}}},
            };
            res = JsonResponse(200, body);
        }
        res.end();
    });

    return app;
}

}  // namespace standard_tools::api
