#pragma once

#include "standard_tools/core/value_objects.hpp"

#include <date/date.h>
#include <nlohmann/json.hpp>

namespace standard_tools::core {

using json = nlohmann::json;

inline void to_json(json& j, const Date& d) {
    j = FormatDate(d);
}

inline void from_json(const json& j, Date& d) {
    d = ParseDate(j.get<std::string>());
}

inline void to_json(json& j, const OHLCV& o) {
    j = json{
        {"date", FormatDate(o.date)},
        {"open", o.open},
        {"high", o.high},
        {"low", o.low},
        {"close", o.close},
        {"volume", o.volume},
    };
}

inline void from_json(const json& j, OHLCV& o) {
    o.date = ParseDate(j.at("date").get<std::string>());
    o.open = j.value("open", 0.0);
    o.high = j.value("high", 0.0);
    o.low = j.value("low", 0.0);
    o.close = j.value("close", 0.0);
    o.volume = j.value("volume", static_cast<std::int64_t>(0));
}

inline void to_json(json& j, const TickerInfo& t) {
    j = json{
        {"symbol", t.symbol},
        {"name", t.name},
        {"sector", t.sector},
        {"industry", t.industry},
        {"employees", t.employees},
        {"city", t.city},
        {"country", t.country},
        {"website", t.website},
    };
}

inline void from_json(const json& j, TickerInfo& t) {
    j.at("symbol").get_to(t.symbol);
    j.at("name").get_to(t.name);
    j.at("sector").get_to(t.sector);
    j.at("industry").get_to(t.industry);
    j.at("employees").get_to(t.employees);
    j.at("city").get_to(t.city);
    j.at("country").get_to(t.country);
    j.at("website").get_to(t.website);
}

inline void to_json(json& j, const FinancialRatios& f) {
    j = json{
        {"symbol", f.symbol},
        {"forward_pe", f.forward_pe},
        {"trailing_pe", f.trailing_pe},
        {"price_to_book", f.price_to_book},
        {"debt_to_equity", f.debt_to_equity},
        {"roe", f.roe},
        {"profit_margins", f.profit_margins},
        {"dividend_yield", f.dividend_yield},
        {"market_cap", f.market_cap},
    };
}

inline void from_json(const json& j, FinancialRatios& f) {
    j.at("symbol").get_to(f.symbol);
    j.at("forward_pe").get_to(f.forward_pe);
    j.at("trailing_pe").get_to(f.trailing_pe);
    j.at("price_to_book").get_to(f.price_to_book);
    j.at("debt_to_equity").get_to(f.debt_to_equity);
    j.at("roe").get_to(f.roe);
    j.at("profit_margins").get_to(f.profit_margins);
    j.at("dividend_yield").get_to(f.dividend_yield);
    j.at("market_cap").get_to(f.market_cap);
}

inline void to_json(json& j, const DataSetMetadata& m) {
    j = json{
        {"provider", m.provider},
        {"adjusted", m.adjusted},
        {"survivorship_free", m.survivorship_free},
        {"point_in_time", m.point_in_time},
        {"frequency", m.frequency},
        {"timezone", m.timezone},
        {"retrieved_at", FormatDate(m.retrieved_at)},
    };
}

inline void from_json(const json& j, DataSetMetadata& m) {
    j.at("provider").get_to(m.provider);
    j.at("adjusted").get_to(m.adjusted);
    j.at("survivorship_free").get_to(m.survivorship_free);
    j.at("point_in_time").get_to(m.point_in_time);
    j.at("frequency").get_to(m.frequency);
    j.at("timezone").get_to(m.timezone);
    m.retrieved_at = ParseDate(j.at("retrieved_at").get<std::string>());
}

}  // namespace standard_tools::core
