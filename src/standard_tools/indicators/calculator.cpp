#include "standard_tools/indicators/calculator.hpp"

#include "standard_tools/core/errors.hpp"

#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace standard_tools::indicators {

namespace {

std::size_t ParamAsSize(
    const std::unordered_map<std::string, double>& params,
    const std::string& key,
    std::size_t default_value) {
    auto it = params.find(key);
    if (it == params.end()) {
        return default_value;
    }
    double value = it->second;
    if (value <= 0.0 || std::floor(value) != value) {
        throw core::InvalidCommandError{
            "invalid value for " + key + ": " + std::to_string(value)};
    }
    return static_cast<std::size_t>(value);
}

double Mean(const std::vector<double>& values) {
    double sum = 0.0;
    for (double v : values) {
        sum += v;
    }
    return sum / static_cast<double>(values.size());
}

double SampleStdDev(const std::vector<double>& values) {
    if (values.size() < 2) {
        return 0.0;
    }
    double mean = Mean(values);
    double variance = 0.0;
    for (double v : values) {
        double diff = v - mean;
        variance += diff * diff;
    }
    variance /= static_cast<double>(values.size() - 1);
    return std::sqrt(variance);
}

double TrueRange(const core::OHLCV& bar, const core::OHLCV& prev) {
    double high_low = bar.high - bar.low;
    double high_close = std::abs(bar.high - prev.close);
    double low_close = std::abs(bar.low - prev.close);
    return std::max(high_low, std::max(high_close, low_close));
}

std::vector<core::Date> DatesFromSeries(const std::vector<core::OHLCV>& series) {
    std::vector<core::Date> dates;
    dates.reserve(series.size());
    for (const auto& bar : series) {
        dates.push_back(bar.date);
    }
    return dates;
}

std::vector<IndicatorValue> NoneValues(const std::vector<core::Date>& dates) {
    std::vector<IndicatorValue> values;
    values.reserve(dates.size());
    for (const auto& d : dates) {
        values.push_back(IndicatorValue{.date = d, .value = std::nullopt});
    }
    return values;
}

std::vector<IndicatorValue> EmaValues(
    const std::vector<double>& closes,
    const std::vector<core::Date>& dates,
    std::size_t period) {
    std::vector<IndicatorValue> values;
    values.reserve(closes.size());

    if (period == 0 || closes.size() < period) {
        return NoneValues(dates);
    }

    double multiplier = 2.0 / static_cast<double>(period + 1);
    double ema = 0.0;

    for (std::size_t i = 0; i < closes.size(); ++i) {
        if (i + 1 < period) {
            values.push_back(IndicatorValue{.date = dates[i], .value = std::nullopt});
        } else if (i + 1 == period) {
            ema = Mean(std::vector<double>(closes.begin(), closes.begin() + i + 1));
            values.push_back(IndicatorValue{.date = dates[i], .value = ema});
        } else {
            ema = (closes[i] - ema) * multiplier + ema;
            values.push_back(IndicatorValue{.date = dates[i], .value = ema});
        }
    }

    return values;
}

std::vector<IndicatorValue> EmaOfOptions(
    const std::vector<IndicatorValue>& series,
    std::size_t period) {
    std::vector<IndicatorValue> result;
    result.reserve(series.size());

    if (period == 0) {
        for (const auto& iv : series) {
            result.push_back(IndicatorValue{.date = iv.date, .value = std::nullopt});
        }
        return result;
    }

    double multiplier = 2.0 / static_cast<double>(period + 1);
    std::optional<double> ema;
    std::size_t seen = 0;
    std::vector<double> seed_values;
    seed_values.reserve(period);

    for (const auto& iv : series) {
        if (iv.value.has_value() && !ema.has_value()) {
            seen++;
            seed_values.push_back(iv.value.value());
            if (seen == period) {
                double seed = Mean(seed_values);
                ema = seed;
                result.push_back(IndicatorValue{.date = iv.date, .value = seed});
            } else {
                result.push_back(IndicatorValue{.date = iv.date, .value = std::nullopt});
            }
        } else if (iv.value.has_value() && ema.has_value()) {
            double next = (iv.value.value() - ema.value()) * multiplier + ema.value();
            ema = next;
            result.push_back(IndicatorValue{.date = iv.date, .value = next});
        } else {
            result.push_back(IndicatorValue{.date = iv.date, .value = std::nullopt});
        }
    }

    return result;
}

double RsiValue(double avg_gain, double avg_loss) {
    if (avg_loss == 0.0) {
        return 100.0;
    }
    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

IndicatorResult CalculateSMA(
    const std::vector<core::OHLCV>& series,
    const std::unordered_map<std::string, double>& params) {
    std::size_t period = ParamAsSize(params, "period", 20);
    std::vector<IndicatorValue> values;
    values.reserve(series.size());

    if (period == 0 || series.size() < period) {
        for (const auto& bar : series) {
            values.push_back(IndicatorValue{.date = bar.date, .value = std::nullopt});
        }
        return IndicatorResult{
            .name = "sma",
            .params = {{"period", static_cast<double>(period)}},
            .values = std::move(values),
            .extra_series = {}};
    }

    for (std::size_t i = 0; i < series.size(); ++i) {
        if (i + 1 < period) {
            values.push_back(IndicatorValue{.date = series[i].date, .value = std::nullopt});
        } else {
            std::vector<double> window;
            window.reserve(period);
            for (std::size_t j = i + 1 - period; j <= i; ++j) {
                window.push_back(series[j].close);
            }
            values.push_back(
                IndicatorValue{.date = series[i].date, .value = Mean(window)});
        }
    }

    return IndicatorResult{
        .name = "sma",
        .params = {{"period", static_cast<double>(period)}},
        .values = std::move(values),
        .extra_series = {}};
}

IndicatorResult CalculateEMA(
    const std::vector<core::OHLCV>& series,
    const std::unordered_map<std::string, double>& params) {
    std::size_t period = ParamAsSize(params, "period", 20);
    std::vector<IndicatorValue> values;
    values.reserve(series.size());

    if (period == 0 || series.size() < period) {
        for (const auto& bar : series) {
            values.push_back(IndicatorValue{.date = bar.date, .value = std::nullopt});
        }
        return IndicatorResult{
            .name = "ema",
            .params = {{"period", static_cast<double>(period)}},
            .values = std::move(values),
            .extra_series = {}};
    }

    double multiplier = 2.0 / static_cast<double>(period + 1);
    double ema = 0.0;

    for (std::size_t i = 0; i < series.size(); ++i) {
        if (i + 1 < period) {
            values.push_back(IndicatorValue{.date = series[i].date, .value = std::nullopt});
        } else if (i + 1 == period) {
            std::vector<double> window;
            window.reserve(period);
            for (std::size_t j = 0; j <= i; ++j) {
                window.push_back(series[j].close);
            }
            ema = Mean(window);
            values.push_back(IndicatorValue{.date = series[i].date, .value = ema});
        } else {
            ema = (series[i].close - ema) * multiplier + ema;
            values.push_back(IndicatorValue{.date = series[i].date, .value = ema});
        }
    }

    return IndicatorResult{
        .name = "ema",
        .params = {{"period", static_cast<double>(period)}},
        .values = std::move(values),
        .extra_series = {}};
}

IndicatorResult CalculateRSI(
    const std::vector<core::OHLCV>& series,
    const std::unordered_map<std::string, double>& params) {
    std::size_t period = ParamAsSize(params, "period", 14);
    std::vector<IndicatorValue> values;
    values.reserve(series.size());

    if (period == 0 || series.size() < period + 1) {
        for (const auto& bar : series) {
            values.push_back(IndicatorValue{.date = bar.date, .value = std::nullopt});
        }
        return IndicatorResult{
            .name = "rsi",
            .params = {{"period", static_cast<double>(period)}},
            .values = std::move(values),
            .extra_series = {}};
    }

    std::vector<double> gains;
    std::vector<double> losses;
    gains.reserve(period);
    losses.reserve(period);

    for (std::size_t i = 0; i < period; ++i) {
        double diff = series[i + 1].close - series[i].close;
        if (diff >= 0.0) {
            gains.push_back(diff);
            losses.push_back(0.0);
        } else {
            gains.push_back(0.0);
            losses.push_back(-diff);
        }
    }

    double avg_gain = Mean(gains);
    double avg_loss = Mean(losses);

    for (std::size_t i = 0; i < period; ++i) {
        values.push_back(IndicatorValue{.date = series[i].date, .value = std::nullopt});
    }
    values.push_back(
        IndicatorValue{.date = series[period].date, .value = RsiValue(avg_gain, avg_loss)});

    double smoothing = static_cast<double>(period);
    double smoothing_minus_one = static_cast<double>(period - 1);

    for (std::size_t i = period; i + 1 < series.size(); ++i) {
        double diff = series[i + 1].close - series[i].close;
        double gain = diff >= 0.0 ? diff : 0.0;
        double loss = diff >= 0.0 ? 0.0 : -diff;

        avg_gain = (avg_gain * smoothing_minus_one + gain) / smoothing;
        avg_loss = (avg_loss * smoothing_minus_one + loss) / smoothing;

        values.push_back(IndicatorValue{
            .date = series[i + 1].date, .value = RsiValue(avg_gain, avg_loss)});
    }

    return IndicatorResult{
        .name = "rsi",
        .params = {{"period", static_cast<double>(period)}},
        .values = std::move(values),
        .extra_series = {}};
}

IndicatorResult CalculateMACD(
    const std::vector<core::OHLCV>& series,
    const std::unordered_map<std::string, double>& params) {
    std::size_t fast = ParamAsSize(params, "fast", 12);
    std::size_t slow = ParamAsSize(params, "slow", 26);
    std::size_t signal = ParamAsSize(params, "signal", 9);

    auto dates = DatesFromSeries(series);
    std::vector<double> closes;
    closes.reserve(series.size());
    for (const auto& bar : series) {
        closes.push_back(bar.close);
    }

    std::vector<IndicatorValue> values;
    values.reserve(series.size());
    std::vector<IndicatorValue> signal_series;
    signal_series.reserve(series.size());
    std::vector<IndicatorValue> histogram_series;
    histogram_series.reserve(series.size());

    if (fast == 0 || slow == 0 || signal == 0 || series.size() < slow) {
        for (const auto& d : dates) {
            values.push_back(IndicatorValue{.date = d, .value = std::nullopt});
            signal_series.push_back(IndicatorValue{.date = d, .value = std::nullopt});
            histogram_series.push_back(IndicatorValue{.date = d, .value = std::nullopt});
        }
        return IndicatorResult{
            .name = "macd",
            .params = {
                {"fast", static_cast<double>(fast)},
                {"slow", static_cast<double>(slow)},
                {"signal", static_cast<double>(signal)}},
            .values = std::move(values),
            .extra_series = {
                {"signal", std::move(signal_series)},
                {"histogram", std::move(histogram_series)}}};
    }

    auto fast_ema = EmaValues(closes, dates, fast);
    auto slow_ema = EmaValues(closes, dates, slow);

    std::vector<IndicatorValue> macd_line;
    macd_line.reserve(series.size());
    for (std::size_t i = 0; i < series.size(); ++i) {
        IndicatorValue iv{.date = dates[i], .value = std::nullopt};
        if (fast_ema[i].value.has_value() && slow_ema[i].value.has_value()) {
            iv.value = fast_ema[i].value.value() - slow_ema[i].value.value();
        }
        macd_line.push_back(iv);
    }

    auto signal_ema = EmaOfOptions(macd_line, signal);

    for (std::size_t i = 0; i < series.size(); ++i) {
        auto macd = macd_line[i].value;
        auto sig = signal_ema[i].value;
        std::optional<double> histogram;
        if (macd.has_value() && sig.has_value()) {
            histogram = macd.value() - sig.value();
        }
        values.push_back(IndicatorValue{.date = dates[i], .value = macd});
        signal_series.push_back(IndicatorValue{.date = dates[i], .value = sig});
        histogram_series.push_back(IndicatorValue{.date = dates[i], .value = histogram});
    }

    return IndicatorResult{
        .name = "macd",
        .params = {
            {"fast", static_cast<double>(fast)},
            {"slow", static_cast<double>(slow)},
            {"signal", static_cast<double>(signal)}},
        .values = std::move(values),
        .extra_series = {
            {"signal", std::move(signal_series)},
            {"histogram", std::move(histogram_series)}}};
}

IndicatorResult CalculateBollinger(
    const std::vector<core::OHLCV>& series,
    const std::unordered_map<std::string, double>& params) {
    std::size_t period = ParamAsSize(params, "period", 20);
    std::size_t std_dev_count = ParamAsSize(params, "std_dev", 2);

    std::vector<IndicatorValue> values;
    values.reserve(series.size());
    std::vector<IndicatorValue> upper;
    upper.reserve(series.size());
    std::vector<IndicatorValue> lower;
    lower.reserve(series.size());

    if (period == 0 || series.size() < period) {
        for (const auto& bar : series) {
            values.push_back(IndicatorValue{.date = bar.date, .value = std::nullopt});
            upper.push_back(IndicatorValue{.date = bar.date, .value = std::nullopt});
            lower.push_back(IndicatorValue{.date = bar.date, .value = std::nullopt});
        }
        return IndicatorResult{
            .name = "bollinger_bands",
            .params = {
                {"period", static_cast<double>(period)},
                {"std_dev", static_cast<double>(std_dev_count)}},
            .values = std::move(values),
            .extra_series = {
                {"upper", std::move(upper)},
                {"lower", std::move(lower)}}};
    }

    for (std::size_t i = 0; i < series.size(); ++i) {
        if (i + 1 < period) {
            values.push_back(IndicatorValue{.date = series[i].date, .value = std::nullopt});
            upper.push_back(IndicatorValue{.date = series[i].date, .value = std::nullopt});
            lower.push_back(IndicatorValue{.date = series[i].date, .value = std::nullopt});
        } else {
            std::vector<double> window;
            window.reserve(period);
            for (std::size_t j = i + 1 - period; j <= i; ++j) {
                window.push_back(series[j].close);
            }
            double middle = Mean(window);
            double band_width = static_cast<double>(std_dev_count) * SampleStdDev(window);

            values.push_back(IndicatorValue{.date = series[i].date, .value = middle});
            upper.push_back(
                IndicatorValue{.date = series[i].date, .value = middle + band_width});
            lower.push_back(
                IndicatorValue{.date = series[i].date, .value = middle - band_width});
        }
    }

    return IndicatorResult{
        .name = "bollinger_bands",
        .params = {
            {"period", static_cast<double>(period)},
            {"std_dev", static_cast<double>(std_dev_count)}},
        .values = std::move(values),
        .extra_series = {
            {"upper", std::move(upper)},
            {"lower", std::move(lower)}}};
}

IndicatorResult CalculateATR(
    const std::vector<core::OHLCV>& series,
    const std::unordered_map<std::string, double>& params) {
    std::size_t period = ParamAsSize(params, "period", 14);
    std::vector<IndicatorValue> values;
    values.reserve(series.size());

    if (period == 0 || series.size() < period + 1) {
        for (const auto& bar : series) {
            values.push_back(IndicatorValue{.date = bar.date, .value = std::nullopt});
        }
        return IndicatorResult{
            .name = "atr",
            .params = {{"period", static_cast<double>(period)}},
            .values = std::move(values),
            .extra_series = {}};
    }

    std::vector<double> initial_trs;
    initial_trs.reserve(period);
    for (std::size_t i = 0; i < period; ++i) {
        initial_trs.push_back(TrueRange(series[i + 1], series[i]));
    }

    double atr = Mean(initial_trs);
    double smoothing = static_cast<double>(period);
    double smoothing_minus_one = static_cast<double>(period - 1);

    for (std::size_t i = 0; i < period; ++i) {
        values.push_back(IndicatorValue{.date = series[i].date, .value = std::nullopt});
    }
    values.push_back(IndicatorValue{.date = series[period].date, .value = atr});

    for (std::size_t i = period; i + 1 < series.size(); ++i) {
        double tr = TrueRange(series[i + 1], series[i]);
        atr = (atr * smoothing_minus_one + tr) / smoothing;
        values.push_back(IndicatorValue{.date = series[i + 1].date, .value = atr});
    }

    return IndicatorResult{
        .name = "atr",
        .params = {{"period", static_cast<double>(period)}},
        .values = std::move(values),
        .extra_series = {}};
}

IndicatorResult CalculateOBV(
    const std::vector<core::OHLCV>& series,
    const std::unordered_map<std::string, double>& params) {
    std::vector<IndicatorValue> values;
    values.reserve(series.size());

    if (series.empty()) {
        return IndicatorResult{
            .name = "obv",
            .params = params,
            .values = std::move(values),
            .extra_series = {}};
    }

    double obv = static_cast<double>(series[0].volume);
    values.push_back(IndicatorValue{.date = series[0].date, .value = obv});

    for (std::size_t i = 0; i + 1 < series.size(); ++i) {
        double current_close = series[i + 1].close;
        double previous_close = series[i].close;
        double volume = static_cast<double>(series[i + 1].volume);

        if (current_close > previous_close) {
            obv += volume;
        } else if (current_close < previous_close) {
            obv -= volume;
        }

        values.push_back(IndicatorValue{.date = series[i + 1].date, .value = obv});
    }

    return IndicatorResult{
        .name = "obv",
        .params = params,
        .values = std::move(values),
        .extra_series = {}};
}

IndicatorResult CalculateVWAP(
    const std::vector<core::OHLCV>& series,
    const std::unordered_map<std::string, double>& params) {
    std::vector<IndicatorValue> values;
    values.reserve(series.size());

    double cumulative_tp_volume = 0.0;
    double cumulative_volume = 0.0;

    for (const auto& bar : series) {
        double typical_price = (bar.high + bar.low + bar.close) / 3.0;
        double volume = static_cast<double>(bar.volume);

        cumulative_tp_volume += typical_price * volume;
        cumulative_volume += volume;

        std::optional<double> vwap;
        if (cumulative_volume != 0.0) {
            vwap = cumulative_tp_volume / cumulative_volume;
        }
        values.push_back(IndicatorValue{.date = bar.date, .value = vwap});
    }

    return IndicatorResult{
        .name = "vwap",
        .params = params,
        .values = std::move(values),
        .extra_series = {}};
}

}  // namespace

IndicatorResult IndicatorCalculator::Calculate(
    const std::string& name,
    const std::vector<core::OHLCV>& series,
    const std::unordered_map<std::string, double>& params) const {
    if (name == "sma") {
        return CalculateSMA(series, params);
    }
    if (name == "ema") {
        return CalculateEMA(series, params);
    }
    if (name == "rsi") {
        return CalculateRSI(series, params);
    }
    if (name == "macd") {
        return CalculateMACD(series, params);
    }
    if (name == "bollinger_bands") {
        return CalculateBollinger(series, params);
    }
    if (name == "atr") {
        return CalculateATR(series, params);
    }
    if (name == "obv") {
        return CalculateOBV(series, params);
    }
    if (name == "vwap") {
        return CalculateVWAP(series, params);
    }

    throw core::InvalidCommandError{"unknown indicator: " + name};
}

}  // namespace standard_tools::indicators
