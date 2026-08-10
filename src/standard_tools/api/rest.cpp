#include "standard_tools/api/rest.hpp"

#include "standard_tools/agent/tool.hpp"
#include "standard_tools/api/helpers.hpp"
#include "standard_tools/core/errors.hpp"
#include "standard_tools/core/json_serialization.hpp"
#include "standard_tools/core/value_objects.hpp"

#include <crow.h>

#include <vector>

namespace standard_tools::api {

crow::App<>& RegisterRoutes(crow::App<>& app, AppState& state) {
    CROW_ROUTE(app, "/health")
    ([](const crow::request&, crow::response& res) {
        res.set_header("Content-Type", "application/json");
        res.body = json{{"status", "ok"}}.dump();
        res.end();
    });

    CROW_ROUTE(app, "/api/v1/agent/tools").methods(crow::HTTPMethod::GET)
    ([](const crow::request&, crow::response& res) {
        res.set_header("Content-Type", "application/json");
        res.body = json(agent::ListTools()).dump();
        res.end();
    });

    CROW_ROUTE(app, "/api/v1/agent/dispatch").methods(crow::HTTPMethod::POST)
    ([&state](const crow::request& req, crow::response& res) {
        try {
            auto tcr = DecodeToolCall(req);
            if (tcr.tool.empty() && tcr.name.empty()) {
                res = ErrorResponse(400, "tool or name is required");
                res.end();
                return;
            }
            auto result = state.dispatcher->Dispatch(ToolCallFromRequest(tcr));
            RecordAudit(state, "", tcr.tool.empty() ? tcr.name : tcr.tool, tcr.arguments, result.output, std::nullopt);
            res = JsonResponse(200, json{{"output", result.output}});
        } catch (const std::exception& e) {
            res = ErrorResponse(DomainErrorStatus(e), e.what());
        }
        res.end();
    });

    CROW_ROUTE(app, "/api/v1/market-data/<string>").methods(crow::HTTPMethod::GET)
    ([&state](const crow::request& req, crow::response& res, const std::string& ticker_str) {
        try {
            auto ticker = core::Ticker(ticker_str);
            auto& query = req.url_params;
            auto start = core::ParseDate(query.get("start") ? query.get("start") : "");
            auto end = core::ParseDate(query.get("end") ? query.get("end") : "");
            auto range = core::DateRange(start, end);
            auto interval = core::ParseBarInterval(query.get("interval") ? query.get("interval") : "daily");
            auto provider = std::string(query.get("provider") ? query.get("provider") : "");
            auto series = state.market_data->Fetch(ticker, interval, range, provider);
            res.set_header("Content-Type", "application/json");
            res.body = json(series).dump();
        } catch (const std::exception& e) {
            res = ErrorResponse(DomainErrorStatus(e), e.what());
        }
        res.end();
    });

    return app;
}

}  // namespace standard_tools::api
