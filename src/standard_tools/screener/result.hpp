#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace standard_tools::screener {

using json = nlohmann::json;

/// A snapshot of fundamental metrics for a single security.
struct FundamentalData {
    std::string ticker;
    double market_cap = 0.0;
    double pe_ratio = 0.0;
    double pb_ratio = 0.0;
    double dividend_yield = 0.0;
    double eps_growth = 0.0;
    double debt_to_equity = 0.0;
    double roe = 0.0;
};

/// The outcome of a screening run.
///
/// Matches are tickers that satisfied every configured criterion. Failed are
/// tickers that could not be fetched or did not satisfy the criteria.
struct ScreenResult {
    std::vector<FundamentalData> matches;
    std::vector<std::string> failed;
};

inline void to_json(json& j, const FundamentalData& d) {
    j = json::object();
    j["ticker"] = d.ticker;
    j["market_cap"] = d.market_cap;
    j["pe_ratio"] = d.pe_ratio;
    j["pb_ratio"] = d.pb_ratio;
    j["dividend_yield"] = d.dividend_yield;
    j["eps_growth"] = d.eps_growth;
    j["debt_to_equity"] = d.debt_to_equity;
    j["roe"] = d.roe;
}

inline void to_json(json& j, const ScreenResult& r) {
    j = json::object();
    j["matches"] = r.matches;
    j["failed"] = r.failed;
}

}  // namespace standard_tools::screener
