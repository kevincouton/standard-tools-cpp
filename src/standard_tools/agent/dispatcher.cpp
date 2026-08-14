#include "standard_tools/agent/dispatcher.hpp"

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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace standard_tools::agent {

namespace {

core::Date ParseDateParam(const std::string& name, const json& args) {
    if (!args.contains(name) || !args[name].is_string()) {
        throw core::InvalidCommandError{name + " date is required"};
    }
    return core::ParseDate(args[name].get<std::string>());
}

std::vector<core::OHLCV> FetchSeries(
    marketdata::Service* svc,
    const json& args,
    const std::string& ticker_field = "ticker") {
    auto ticker = core::Ticker(args.value(ticker_field, ""));
    auto start = ParseDateParam("start", args);
    auto end = ParseDateParam("end", args);
    auto range = core::DateRange(start, end);
    auto interval = core::ParseBarInterval(args.value("interval", "daily"));
    auto provider = args.value("provider", "");
    return svc->Fetch(ticker, interval, range, provider);
}

std::vector<double> ExtractCloses(const std::vector<core::OHLCV>& series) {
    std::vector<double> closes;
    closes.reserve(series.size());
    for (const auto& bar : series) {
        closes.push_back(bar.close);
    }
    return closes;
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
    config.periods_per_year = obj.value("periods_per_year", static_cast<int>(config.periods_per_year));
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

    if (operation == analysis::operation::kRegression) {
        if (body.contains("asset_returns")) {
            req.asset_returns = body["asset_returns"].get<std::vector<double>>();
        }
        if (body.contains("benchmark_returns")) {
            req.benchmark_returns = body["benchmark_returns"].get<std::vector<double>>();
        }
    } else if (operation == analysis::operation::kCointegration) {
        if (body.contains("a_closes")) req.a_closes = body["a_closes"].get<std::vector<double>>();
        if (body.contains("b_closes")) req.b_closes = body["b_closes"].get<std::vector<double>>();
    } else if (operation == analysis::operation::kHurst) {
        if (body.contains("prices")) req.prices = body["prices"].get<std::vector<double>>();
        if (body.contains("max_lag")) req.max_lag = body["max_lag"].get<int>();
    } else if (operation == analysis::operation::kPca) {
        if (body.contains("returns_matrix")) {
            req.returns_matrix = body["returns_matrix"].get<std::vector<std::vector<double>>>();
        }
        req.n_components = body.value("n_components", req.n_components);
    } else if (operation == analysis::operation::kCorrelation) {
        if (body.contains("returns_map")) {
            req.returns_map = body["returns_map"].get<std::map<std::string, std::vector<double>>>();
        }
    } else if (operation == analysis::operation::kOptions) {
        analysis::BlackScholesParams p;
        p.spot = body.value("spot", 0.0);
        p.strike = body.value("strike", 0.0);
        p.risk_free_rate = body.value("risk_free_rate", 0.0);
        p.volatility = body.value("volatility", 0.0);
        p.time_to_maturity = body.value("time_to_maturity", 0.0);
        auto type = body.value("option_type", std::string{"call"});
        p.option_type = (type == "put") ? analysis::OptionType::Put : analysis::OptionType::Call;
        req.black_scholes = p;
    }
    return req;
}

template <typename T>
T GetNumber(const json& j, const std::string& key, T default_value) {
    if (!j.contains(key) || !j[key].is_number()) {
        return default_value;
    }
    return j[key].get<T>();
}

portfolio::MeanVarianceRequest ParseMeanVarianceRequest(const json& body) {
    portfolio::MeanVarianceRequest req;
    req.returns = body.value("returns", std::vector<std::vector<double>>{});
    req.labels = body.value("labels", std::vector<std::string>{});
    req.risk_free_rate = body.value("risk_free_rate", req.risk_free_rate);
    req.objective = body.value("objective", req.objective);
    if (body.contains("target_return")) {
        req.target_return = body["target_return"].get<double>();
    }
    if (body.contains("target_volatility")) {
        req.target_volatility = body["target_volatility"].get<double>();
    }
    return req;
}

portfolio::RiskParityRequest ParseRiskParityRequest(const json& body) {
    portfolio::RiskParityRequest req;
    req.returns = body.value("returns", std::vector<std::vector<double>>{});
    req.labels = body.value("labels", std::vector<std::string>{});
    return req;
}

portfolio::BlackLittermanSimplifiedRequest ParseBlackLittermanRequest(const json& body) {
    portfolio::BlackLittermanSimplifiedRequest req;
    req.returns = body.value("returns", std::vector<std::vector<double>>{});
    req.labels = body.value("labels", std::vector<std::string>{});
    if (body.contains("market_caps")) {
        req.market_caps = body["market_caps"].get<std::map<std::string, double>>();
    }
    if (body.contains("views")) {
        req.views = body["views"].get<std::map<std::string, double>>();
    }
    req.tau = body.value("tau", req.tau);
    req.risk_aversion = body.value("risk_aversion", req.risk_aversion);
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

}  // namespace

Dispatcher::Dispatcher(
    std::shared_ptr<marketdata::Service> market_data,
    std::shared_ptr<indicators::IndicatorCalculator> indicators,
    std::shared_ptr<metrics::RiskReturnCalculator> metrics,
    std::shared_ptr<analysis::AnalysisCalculator> analysis,
    std::shared_ptr<screener::Screener> screener)
    : market_data_(std::move(market_data)),
      indicators_(std::move(indicators)),
      metrics_(std::move(metrics)),
      analysis_(std::move(analysis)),
      screener_(std::move(screener)) {}

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
    if (call.name == ToolCalculateIndicator) {
        return CalculateIndicator(call.arguments);
    }
    if (call.name == ToolCalculateReturnMetrics) {
        return CalculateReturnMetrics(call.arguments);
    }
    if (call.name == ToolCalculateRiskMetrics) {
        return CalculateRiskMetrics(call.arguments);
    }
    if (call.name == ToolRunAnalysis) {
        return RunAnalysis(call.arguments);
    }
    if (call.name == ToolRunBacktest) {
        return RunBacktest(call.arguments);
    }
    if (call.name == ToolOptimizePortfolio) {
        return OptimizePortfolio(call.arguments);
    }
    if (call.name == ToolRiskParityPortfolio) {
        return RiskParityPortfolio(call.arguments);
    }
    if (call.name == ToolBlackLittermanPortfolio) {
        return BlackLittermanPortfolio(call.arguments);
    }
    if (call.name == ToolScreenStocks) {
        return ScreenStocks(call.arguments);
    }
    throw core::InvalidCommandError{"unknown tool " + call.name};
}

ToolResult Dispatcher::FetchOhlcv(const json& args) {
    auto series = FetchSeries(market_data_.get(), args);
    return OkResult(json(series));
}

ToolResult Dispatcher::CalculateIndicator(const json& args) {
    auto indicator = args.value("indicator", std::string{});
    if (indicator.empty()) {
        throw core::InvalidCommandError{"indicator is required"};
    }
    auto series = FetchSeries(market_data_.get(), args);
    auto params = ParseParams(args.value("params", json::object()));
    auto result = indicators_->Calculate(indicator, series, params);
    return OkResult(json(result));
}

ToolResult Dispatcher::CalculateReturnMetrics(const json& args) {
    auto series = FetchSeries(market_data_.get(), args);
    auto closes = ExtractCloses(series);
    double risk_free_rate = args.value("risk_free_rate", metrics::kDefaultRiskFreeRate);
    auto result = metrics_->CalculateReturnMetrics(closes, risk_free_rate);
    return OkResult(json(result));
}

ToolResult Dispatcher::CalculateRiskMetrics(const json& args) {
    auto series = FetchSeries(market_data_.get(), args);
    auto closes = ExtractCloses(series);
    double risk_free_rate = args.value("risk_free_rate", metrics::kDefaultRiskFreeRate);
    auto result = metrics_->CalculateRiskMetrics(closes, risk_free_rate);
    return OkResult(json(result));
}

ToolResult Dispatcher::RunAnalysis(const json& args) {
    auto operation = args.value("operation", std::string{});
    if (operation.empty()) {
        throw core::InvalidCommandError{"operation is required"};
    }
    auto request = ParseAnalysisRequest(operation, args.value("request", json::object()));
    auto result = analysis_->Calculate(request);
    return OkResult(json(result));
}

ToolResult Dispatcher::RunBacktest(const json& args) {
    auto strategy = args.value("strategy", std::string{});
    if (strategy.empty()) {
        throw core::InvalidCommandError{"strategy is required"};
    }
    auto series = FetchSeries(market_data_.get(), args);
    auto params = ParseParams(args.value("params", json::object()));
    auto config = ParseBacktestConfig(args.value("config", json::object()));

    backtest::BacktestEngine engine(strategy, config);
    auto result = engine.Run(series, params);
    return OkResult(json(result));
}

ToolResult Dispatcher::OptimizePortfolio(const json& args) {
    auto request = ParseMeanVarianceRequest(args.value("request", json::object()));
    auto result = portfolio::MeanVariance(request);
    return OkResult(json(result));
}

ToolResult Dispatcher::RiskParityPortfolio(const json& args) {
    auto request = ParseRiskParityRequest(args.value("request", json::object()));
    auto result = portfolio::RiskParity(request);
    return OkResult(json(result));
}

ToolResult Dispatcher::BlackLittermanPortfolio(const json& args) {
    auto request = ParseBlackLittermanRequest(args.value("request", json::object()));
    auto result = portfolio::BlackLittermanSimplified(request);
    return OkResult(json(result));
}

ToolResult Dispatcher::ScreenStocks(const json& args) {
    std::vector<std::string> tickers;
    if (args.contains("tickers") && args["tickers"].is_array()) {
        tickers = args["tickers"].get<std::vector<std::string>>();
    }
    auto criteria = ParseScreenCriteria(args.value("criteria", json::object()));
    auto result = screener_->Screen(tickers, criteria);
    return OkResult(json(result));
}

}  // namespace standard_tools::agent
