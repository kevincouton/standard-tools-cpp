#include "standard_tools/api/mcp.hpp"

#include "standard_tools/agent/tool.hpp"
#include "standard_tools/api/helpers.hpp"

namespace standard_tools::api {

namespace {

constexpr const char* kAppName = "standard-tools-cpp";
constexpr const char* kAppVersion = "0.1.0";
constexpr const char* kMcpProtocolVersion = "2024-11-05";

}  // namespace

crow::App<>& RegisterMCPRoutes(crow::App<>& app, AppState& state) {
    CROW_ROUTE(app, "/mcp/capabilities").methods(crow::HTTPMethod::GET)
    ([](const crow::request&, crow::response& res) {
        json body = {
            {"protocolVersion", kMcpProtocolVersion},
            {"capabilities", {{"tools", json::object()}}},
            {"serverInfo", {{"name", kAppName}, {"version", kAppVersion}}},
        };
        res.set_header("Content-Type", "application/json");
        res.body = body.dump();
        res.end();
    });

    CROW_ROUTE(app, "/mcp/tools/list").methods(crow::HTTPMethod::POST)
    ([](const crow::request&, crow::response& res) {
        json body = {{"tools", agent::ListTools()}};
        res.set_header("Content-Type", "application/json");
        res.body = body.dump();
        res.end();
    });

    CROW_ROUTE(app, "/mcp/tools/call").methods(crow::HTTPMethod::POST)
    ([&state](const crow::request& req, crow::response& res) {
        try {
            auto tcr = DecodeToolCall(req);
            std::string name = tcr.name.empty() ? tcr.tool : tcr.name;
            if (name.empty()) {
                res = ErrorResponse(400, "name is required");
                res.end();
                return;
            }
            auto result = state.dispatcher->Dispatch(ToolCallFromRequest(tcr));
            RecordAudit(state, "", name, tcr.arguments, result.output, std::nullopt);
            json content = json::array();
            content.push_back({{"type", "text"}, {"text", result.output.dump()}});
            json body = {{"content", content}};
            res = JsonResponse(200, body);
        } catch (const std::exception& e) {
            json content = json::array();
            content.push_back({{"type", "text"}, {"text", std::string("error: ") + e.what()}});
            json body = {{"content", content}};
            res = JsonResponse(200, body);
        }
        res.end();
    });

    return app;
}

}  // namespace standard_tools::api
