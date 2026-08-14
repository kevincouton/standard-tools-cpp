#pragma once

#include "standard_tools/core/value_objects.hpp"

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace standard_tools::indicators {

using json = nlohmann::json;

/// A single date-aligned indicator value.
///
/// A value of `std::nullopt` means the indicator is not yet available for that
/// date (warming period).
struct IndicatorValue {
    core::Date date;
    std::optional<double> value;
};

/// The outcome of a single indicator calculation.
struct IndicatorResult {
    /// Name of the indicator that produced this result.
    std::string name;

    /// Parameters used to configure the indicator.
    std::unordered_map<std::string, double> params;

    /// Date-aligned indicator values. Value is `std::nullopt` during the
    /// warming period.
    std::vector<IndicatorValue> values;

    /// Additional named series produced by the indicator (e.g. MACD signal
    /// and histogram, or Bollinger upper and lower bands).
    std::unordered_map<std::string, std::vector<IndicatorValue>> extra_series;
};

inline void to_json(json& j, const IndicatorValue& iv) {
    j = json::object();
    j["date"] = core::FormatDate(iv.date);
    j["value"] = iv.value.has_value() ? json(*iv.value) : json(nullptr);
}

inline void to_json(json& j, const IndicatorResult& r) {
    j = json::object();
    j["name"] = r.name;
    j["params"] = r.params;
    j["values"] = r.values;
    j["extra_series"] = r.extra_series;
}

}  // namespace standard_tools::indicators
