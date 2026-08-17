#include "standard_tools/api/rest.hpp"

#include "standard_tools/agent/tool.hpp"
#include "standard_tools/api/helpers.hpp"
#include "standard_tools/backtest/engine.hpp"
#include "standard_tools/backtest/monte_carlo.hpp"
#include "standard_tools/backtest/walk_forward.hpp"
#include "standard_tools/core/errors.hpp"
#include "standard_tools/core/json_serialization.hpp"
#include "standard_tools/core/value_objects.hpp"
#include "standard_tools/portfolio/black_litterman.hpp"
#include "standard_tools/portfolio/optimizer.hpp"
#include "standard_tools/portfolio/risk_parity.hpp"
#include "standard_tools/screener/criteria.hpp"

#include <crow.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace standard_tools::api {

namespace {

core::Date ParseQueryDate(const crow::request& req, const std::string& name) {
    auto value = req.url_params.get(name.c_str());
    if (!value) {
        throw core::InvalidCommandError{name + " date is required"};
    }
    return core::ParseDate(value);
}

std::string ParseQueryString(
    const crow::request& req, const std::string& name, const std::string& default_value = "") {
    auto value = req.url_params.get(name.c_str());
    if (!value) {
        return default_value;
    }
    return std::string(value);
}

std::vector<core::OHLCV> FetchSeriesFromQuery(
    const AppState& state,
    const crow::request& req,
    const std::string& ticker) {
    auto start = ParseQueryDate(req, "start");
    auto end = ParseQueryDate(req, "end");
    auto range = core::DateRange(start, end);
    auto interval = core::ParseBarInterval(ParseQueryString(req, "interval", "daily"));
    auto provider = ParseQueryString(req, "provider");
    return state.market_data->Fetch(core::Ticker(ticker), interval, range, provider);
}

std::vector<double> ExtractCloses(const std::vector<core::OHLCV>& series) {
    std::vector<double> closes;
    closes.reserve(series.size());
    for (const auto& bar : series) {
        closes.push_back(bar.close);
    }
    return closes;
}

std::unordered_map<std::string, double> ParseIndicatorParams(const crow::request& req) {
    std::unordered_map<std::string, double> params;
    const char* keys[] = {"period", "fast", "slow", "signal", "std_dev"};
    for (const auto* key : keys) {
        auto value = req.url_params.get(key);
        if (value) {
            try {
                params[key] = std::stod(value);
            } catch (...) {
                throw core::InvalidCommandError{
                    std::string("invalid numeric value for ") + key};
            }
        }
    }
    return params;
}

std::unordered_map<std::string, double> ParseParams(const json& obj) {
    std::unordered_map<std::string, double> params;
    if (!obj.is_object()) {
        return params;
    }
    for (auto& [key, value] : obj.items()) {
        if (value.is_number()) {
            params[key] = value.get<double>();
        }
    }
    return params;
}

backtest::BacktestConfig ParseBacktestConfig(const json& obj) {
    backtest::BacktestConfig config;
    if (!obj.is_object()) {
        return config;
    }
    config.initial_capital = obj.value("initial_capital", config.initial_capital);
    config.commission_rate = obj.value("commission_rate", config.commission_rate);
    config.periods_per_year = obj.value("periods_per_year", config.periods_per_year);
    config.risk_free_rate = obj.value("risk_free_rate", config.risk_free_rate);
    return config;
}

backtest::OptimizationMetric ParseOptimizationMetric(const std::string& name) {
    if (name == "sharpe") return backtest::OptimizationMetric::Sharpe;
    if (name == "win_rate") return backtest::OptimizationMetric::WinRate;
    return backtest::OptimizationMetric::TotalReturn;
}

analysis::Request ParseAnalysisRequest(const std::string& operation, const json& body) {
    analysis::Request req;
    req.operation = operation;
    const json& inputs = body.contains("request") && body["request"].is_object()
                             ? body["request"]
                             : body;

    if (operation == analysis::operation::kRegression) {
        if (inputs.contains("asset_returns")) {
            req.asset_returns = inputs["asset_returns"].get<std::vector<double>>();
        }
        if (inputs.contains("benchmark_returns")) {
            req.benchmark_returns = inputs["benchmark_returns"].get<std::vector<double>>();
        }
    } else if (operation == analysis::operation::kCointegration) {
        if (inputs.contains("a_closes")) req.a_closes = inputs["a_closes"].get<std::vector<double>>();
        if (inputs.contains("b_closes")) req.b_closes = inputs["b_closes"].get<std::vector<double>>();
    } else if (operation == analysis::operation::kHurst) {
        if (inputs.contains("prices")) req.prices = inputs["prices"].get<std::vector<double>>();
        if (inputs.contains("max_lag")) req.max_lag = inputs["max_lag"].get<int>();
    } else if (operation == analysis::operation::kPca) {
        if (inputs.contains("returns_matrix")) {
            req.returns_matrix = inputs["returns_matrix"].get<std::vector<std::vector<double>>>();
        }
        req.n_components = inputs.value("n_components", req.n_components);
    } else if (operation == analysis::operation::kCorrelation) {
        if (inputs.contains("returns_map")) {
            req.returns_map = inputs["returns_map"].get<std::map<std::string, std::vector<double>>>();
        }
    } else if (operation == analysis::operation::kOptions) {
        analysis::BlackScholesParams p;
        p.spot = inputs.value("spot", 0.0);
        p.strike = inputs.value("strike", 0.0);
        p.risk_free_rate = inputs.value("risk_free_rate", 0.0);
        p.volatility = inputs.value("volatility", 0.0);
        p.time_to_maturity = inputs.value("time_to_maturity", 0.0);
        auto type = inputs.value("option_type", std::string{"call"});
        p.option_type = (type == "put") ? analysis::OptionType::Put : analysis::OptionType::Call;
        req.black_scholes = p;
    }
    return req;
}

portfolio::MeanVarianceRequest ParseMeanVarianceRequest(const json& body) {
    const json& inputs = body.contains("request") && body["request"].is_object()
                             ? body["request"]
                             : body;
    portfolio::MeanVarianceRequest req;
    req.returns = inputs.value("returns", std::vector<std::vector<double>>{});
    req.labels = inputs.value("labels", std::vector<std::string>{});
    req.risk_free_rate = inputs.value("risk_free_rate", req.risk_free_rate);
    req.objective = inputs.value("objective", req.objective);
    if (inputs.contains("target_return")) {
        req.target_return = inputs["target_return"].get<double>();
    }
    if (inputs.contains("target_volatility")) {
        req.target_volatility = inputs["target_volatility"].get<double>();
    }
    return req;
}

portfolio::RiskParityRequest ParseRiskParityRequest(const json& body) {
    const json& inputs = body.contains("request") && body["request"].is_object()
                             ? body["request"]
                             : body;
    portfolio::RiskParityRequest req;
    req.returns = inputs.value("returns", std::vector<std::vector<double>>{});
    req.labels = inputs.value("labels", std::vector<std::string>{});
    return req;
}

portfolio::BlackLittermanSimplifiedRequest ParseBlackLittermanRequest(const json& body) {
    const json& inputs = body.contains("request") && body["request"].is_object()
                             ? body["request"]
                             : body;
    portfolio::BlackLittermanSimplifiedRequest req;
    req.returns = inputs.value("returns", std::vector<std::vector<double>>{});
    req.labels = inputs.value("labels", std::vector<std::string>{});
    if (inputs.contains("market_caps")) {
        req.market_caps = inputs["market_caps"].get<std::map<std::string, double>>();
    }
    if (inputs.contains("views")) {
        req.views = inputs["views"].get<std::map<std::string, double>>();
    }
    req.tau = inputs.value("tau", req.tau);
    req.risk_aversion = inputs.value("risk_aversion", req.risk_aversion);
    return req;
}

screener::ScreenCriteria ParseScreenCriteria(const json& obj) {
    screener::ScreenCriteria criteria;
    if (!obj.is_object()) {
        return criteria;
    }
    auto opt = [](const json& j, const std::string& key) -> std::optional<double> {
        if (!j.contains(key) || !j[key].is_number()) {
            return std::nullopt;
        }
        return j[key].get<double>();
    };
    criteria.pe_ratio_max = opt(obj, "pe_ratio_max");
    criteria.pe_ratio_min = opt(obj, "pe_ratio_min");
    criteria.pb_ratio_max = opt(obj, "pb_ratio_max");
    criteria.pb_ratio_min = opt(obj, "pb_ratio_min");
    criteria.market_cap_max = opt(obj, "market_cap_max");
    criteria.market_cap_min = opt(obj, "market_cap_min");
    criteria.dividend_yield_max = opt(obj, "dividend_yield_max");
    criteria.dividend_yield_min = opt(obj, "dividend_yield_min");
    criteria.eps_growth_max = opt(obj, "eps_growth_max");
    criteria.eps_growth_min = opt(obj, "eps_growth_min");
    criteria.debt_to_equity_max = opt(obj, "debt_to_equity_max");
    criteria.debt_to_equity_min = opt(obj, "debt_to_equity_min");
    criteria.roe_max = opt(obj, "roe_max");
    criteria.roe_min = opt(obj, "roe_min");
    return criteria;
}

std::optional<std::uint64_t> ParseOptionalSeed(const json& body) {
    if (!body.contains("seed") || body["seed"].is_null()) {
        return std::nullopt;
    }
    return body["seed"].get<std::uint64_t>();
}

}  // namespace

App& RegisterRoutes(App& app, AppState& state) {
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

    CROW_ROUTE(app, "/api/v1/indicators/<string>").methods(crow::HTTPMethod::GET)
    ([&state](const crow::request& req, crow::response& res, const std::string& indicator) {
        try {
            auto ticker = ParseQueryString(req, "ticker");
            if (ticker.empty()) {
                res = ErrorResponse(400, "ticker is required");
                res.end();
                return;
            }
            auto series = FetchSeriesFromQuery(state, req, ticker);
            auto params = ParseIndicatorParams(req);
            auto result = state.indicators->Calculate(indicator, series, params);
            res.set_header("Content-Type", "application/json");
            res.body = json(result).dump();
        } catch (const std::exception& e) {
            res = ErrorResponse(DomainErrorStatus(e), e.what());
        }
        res.end();
    });

    CROW_ROUTE(app, "/api/v1/metrics/risk").methods(crow::HTTPMethod::GET)
    ([&state](const crow::request& req, crow::response& res) {
        try {
            auto ticker = ParseQueryString(req, "ticker");
            if (ticker.empty()) {
                res = ErrorResponse(400, "ticker is required");
                res.end();
                return;
            }
            auto series = FetchSeriesFromQuery(state, req, ticker);
            auto closes = ExtractCloses(series);
            double risk_free_rate = 0.02;
            auto rf = req.url_params.get("risk_free_rate");
            if (rf) {
                try {
                    risk_free_rate = std::stod(rf);
                } catch (...) {
                    throw core::InvalidCommandError{"invalid risk_free_rate"};
                }
            }
            auto result = state.metrics->CalculateRiskMetrics(closes, risk_free_rate);
            res.set_header("Content-Type", "application/json");
            res.body = json(result).dump();
        } catch (const std::exception& e) {
            res = ErrorResponse(DomainErrorStatus(e), e.what());
        }
        res.end();
    });

    CROW_ROUTE(app, "/api/v1/metrics/return").methods(crow::HTTPMethod::GET)
    ([&state](const crow::request& req, crow::response& res) {
        try {
            auto ticker = ParseQueryString(req, "ticker");
            if (ticker.empty()) {
                res = ErrorResponse(400, "ticker is required");
                res.end();
                return;
            }
            auto series = FetchSeriesFromQuery(state, req, ticker);
            auto closes = ExtractCloses(series);
            double risk_free_rate = 0.02;
            auto rf = req.url_params.get("risk_free_rate");
            if (rf) {
                try {
                    risk_free_rate = std::stod(rf);
                } catch (...) {
                    throw core::InvalidCommandError{"invalid risk_free_rate"};
                }
            }
            auto result = state.metrics->CalculateReturnMetrics(closes, risk_free_rate);
            res.set_header("Content-Type", "application/json");
            res.body = json(result).dump();
        } catch (const std::exception& e) {
            res = ErrorResponse(DomainErrorStatus(e), e.what());
        }
        res.end();
    });

    CROW_ROUTE(app, "/api/v1/analysis/<string>").methods(crow::HTTPMethod::POST)
    ([&state](const crow::request& req, crow::response& res, const std::string& operation) {
        try {
            auto body = json::parse(req.body, nullptr, false);
            if (body.is_discarded()) {
                throw core::InvalidCommandError{"invalid JSON body"};
            }
            auto request = ParseAnalysisRequest(operation, body);
            auto result = state.analysis->Calculate(request);
            res.set_header("Content-Type", "application/json");
            res.body = json(result).dump();
        } catch (const std::exception& e) {
            res = ErrorResponse(DomainErrorStatus(e), e.what());
        }
        res.end();
    });

    CROW_ROUTE(app, "/api/v1/backtest/<string>").methods(crow::HTTPMethod::POST)
    ([&state](const crow::request& req, crow::response& res, const std::string& strategy) {
        try {
            auto body = json::parse(req.body, nullptr, false);
            if (body.is_discarded()) {
                throw core::InvalidCommandError{"invalid JSON body"};
            }
            auto ticker = body.value("ticker", std::string{});
            if (ticker.empty()) {
                res = ErrorResponse(400, "ticker is required");
                res.end();
                return;
            }
            auto start = core::ParseDate(body.value("start", std::string{}));
            auto end = core::ParseDate(body.value("end", std::string{}));
            auto range = core::DateRange(start, end);
            auto interval = core::ParseBarInterval(body.value("interval", "daily"));
            auto provider = body.value("provider", std::string{});
            auto series = state.market_data->Fetch(core::Ticker(ticker), interval, range, provider);
            auto params = ParseParams(body.value("params", json::object()));
            auto config = ParseBacktestConfig(body.value("config", json::object()));
            backtest::BacktestEngine engine(strategy, config);
            auto result = engine.Run(series, params);
            res.set_header("Content-Type", "application/json");
            res.body = json(result).dump();
        } catch (const std::exception& e) {
            res = ErrorResponse(DomainErrorStatus(e), e.what());
        }
        res.end();
    });

    CROW_ROUTE(app, "/api/v1/backtest/walk-forward").methods(crow::HTTPMethod::POST)
    ([&state](const crow::request& req, crow::response& res) {
        try {
            auto body = json::parse(req.body, nullptr, false);
            if (body.is_discarded()) {
                throw core::InvalidCommandError{"invalid JSON body"};
            }
            auto ticker = body.value("ticker", std::string{});
            if (ticker.empty()) {
                res = ErrorResponse(400, "ticker is required");
                res.end();
                return;
            }
            auto start = core::ParseDate(body.value("start", std::string{}));
            auto end = core::ParseDate(body.value("end", std::string{}));
            auto range = core::DateRange(start, end);
            auto interval = core::ParseBarInterval(body.value("interval", "daily"));
            auto provider = body.value("provider", std::string{});
            auto series = state.market_data->Fetch(core::Ticker(ticker), interval, range, provider);

            backtest::WalkForwardRequest wfr;
            wfr.strategy = body.value("strategy", std::string{});
            wfr.series = std::move(series);
            if (body.contains("param_grid") && body["param_grid"].is_object()) {
                for (auto& [key, value] : body["param_grid"].items()) {
                    if (value.is_array()) {
                        wfr.param_grid[key] = value.get<std::vector<double>>();
                    }
                }
            }
            wfr.train_size = body.value("train_size", static_cast<std::size_t>(0));
            wfr.test_size = body.value("test_size", static_cast<std::size_t>(0));
            wfr.metric = ParseOptimizationMetric(body.value("metric", "total_return"));
            wfr.config = ParseBacktestConfig(body.value("config", json::object()));

            backtest::WalkForwardOptimizer optimizer(std::move(wfr));
            auto result = optimizer.Run();
            res.set_header("Content-Type", "application/json");
            res.body = json(result).dump();
        } catch (const std::exception& e) {
            res = ErrorResponse(DomainErrorStatus(e), e.what());
        }
        res.end();
    });

    CROW_ROUTE(app, "/api/v1/backtest/monte-carlo").methods(crow::HTTPMethod::POST)
    ([&state](const crow::request& req, crow::response& res) {
        try {
            auto body = json::parse(req.body, nullptr, false);
            if (body.is_discarded()) {
                throw core::InvalidCommandError{"invalid JSON body"};
            }
            auto ticker = body.value("ticker", std::string{});
            if (ticker.empty()) {
                res = ErrorResponse(400, "ticker is required");
                res.end();
                return;
            }
            auto start = core::ParseDate(body.value("start", std::string{}));
            auto end = core::ParseDate(body.value("end", std::string{}));
            auto range = core::DateRange(start, end);
            auto interval = core::ParseBarInterval(body.value("interval", "daily"));
            auto provider = body.value("provider", std::string{});
            auto series = state.market_data->Fetch(core::Ticker(ticker), interval, range, provider);
            auto strategy = body.value("strategy", std::string{"buy_and_hold"});
            auto params = ParseParams(body.value("params", json::object()));
            auto config = ParseBacktestConfig(body.value("config", json::object()));
            backtest::BacktestEngine engine(strategy, config);
            auto bt_result = engine.Run(series, params);

            int simulations = body.value("simulations", 100);
            auto seed = ParseOptionalSeed(body);
            backtest::MonteCarloSimulator simulator(simulations, seed);
            auto mc_result = simulator.FromTrades(bt_result.trades, config.initial_capital);
            res.set_header("Content-Type", "application/json");
            res.body = json(mc_result).dump();
        } catch (const std::exception& e) {
            res = ErrorResponse(DomainErrorStatus(e), e.what());
        }
        res.end();
    });

    CROW_ROUTE(app, "/api/v1/portfolio/optimize").methods(crow::HTTPMethod::POST)
    ([&state](const crow::request& req, crow::response& res) {
        (void)state;
        try {
            auto body = json::parse(req.body, nullptr, false);
            if (body.is_discarded()) {
                throw core::InvalidCommandError{"invalid JSON body"};
            }
            auto request = ParseMeanVarianceRequest(body);
            auto result = portfolio::MeanVariance(request);
            res.set_header("Content-Type", "application/json");
            res.body = json(result).dump();
        } catch (const std::exception& e) {
            res = ErrorResponse(DomainErrorStatus(e), e.what());
        }
        res.end();
    });

    CROW_ROUTE(app, "/api/v1/portfolio/risk-parity").methods(crow::HTTPMethod::POST)
    ([&state](const crow::request& req, crow::response& res) {
        (void)state;
        try {
            auto body = json::parse(req.body, nullptr, false);
            if (body.is_discarded()) {
                throw core::InvalidCommandError{"invalid JSON body"};
            }
            auto request = ParseRiskParityRequest(body);
            auto result = portfolio::RiskParity(request);
            res.set_header("Content-Type", "application/json");
            res.body = json(result).dump();
        } catch (const std::exception& e) {
            res = ErrorResponse(DomainErrorStatus(e), e.what());
        }
        res.end();
    });

    CROW_ROUTE(app, "/api/v1/portfolio/black-litterman").methods(crow::HTTPMethod::POST)
    ([&state](const crow::request& req, crow::response& res) {
        (void)state;
        try {
            auto body = json::parse(req.body, nullptr, false);
            if (body.is_discarded()) {
                throw core::InvalidCommandError{"invalid JSON body"};
            }
            auto request = ParseBlackLittermanRequest(body);
            auto result = portfolio::BlackLittermanSimplified(request);
            res.set_header("Content-Type", "application/json");
            res.body = json(result).dump();
        } catch (const std::exception& e) {
            res = ErrorResponse(DomainErrorStatus(e), e.what());
        }
        res.end();
    });

    CROW_ROUTE(app, "/api/v1/screen").methods(crow::HTTPMethod::POST)
    ([&state](const crow::request& req, crow::response& res) {
        try {
            auto body = json::parse(req.body, nullptr, false);
            if (body.is_discarded()) {
                throw core::InvalidCommandError{"invalid JSON body"};
            }
            std::vector<std::string> tickers;
            if (body.contains("tickers") && body["tickers"].is_array()) {
                tickers = body["tickers"].get<std::vector<std::string>>();
            }
            auto criteria = ParseScreenCriteria(body.value("criteria", json::object()));
            auto result = state.screener->Screen(tickers, criteria);
            res.set_header("Content-Type", "application/json");
            res.body = json(result).dump();
        } catch (const std::exception& e) {
            res = ErrorResponse(DomainErrorStatus(e), e.what());
        }
        res.end();
    });

    return app;
}

}  // namespace standard_tools::api
