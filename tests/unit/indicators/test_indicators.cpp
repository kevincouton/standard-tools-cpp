#include "standard_tools/core/errors.hpp"
#include "standard_tools/core/value_objects.hpp"
#include "standard_tools/indicators/calculator.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <vector>

using Catch::Approx;
using namespace standard_tools;

namespace {

std::vector<core::OHLCV> MakeSeriesFromCloses(const std::vector<double>& closes) {
    std::vector<core::OHLCV> series;
    series.reserve(closes.size());
    for (std::size_t i = 0; i < closes.size(); ++i) {
        double close = closes[i];
        double open = close - 0.1;
        double high = close + 0.2;
        double low = close - 0.2;
        series.push_back(core::OHLCV{
            .date = core::MakeDate(2024, 1, 1 + static_cast<int>(i)),
            .open = open,
            .high = high,
            .low = low,
            .close = close,
            .volume = static_cast<std::int64_t>(1000 + i * 100)});
    }
    return series;
}

double ValueAt(const indicators::IndicatorResult& result, std::size_t idx) {
    REQUIRE(result.values[idx].value.has_value());
    return result.values[idx].value.value();
}

}  // namespace

TEST_CASE("IndicatorCalculator rejects unknown indicators", "[indicators]") {
    indicators::IndicatorCalculator calc;
    auto series = MakeSeriesFromCloses({1.0, 2.0, 3.0, 4.0, 5.0});
    REQUIRE_THROWS_AS(calc.Calculate("unknown", series, {}), core::InvalidCommandError);
}

TEST_CASE("IndicatorCalculator rejects invalid parameters", "[indicators]") {
    indicators::IndicatorCalculator calc;
    auto series = MakeSeriesFromCloses({1.0, 2.0, 3.0, 4.0, 5.0});
    REQUIRE_THROWS_AS(
        calc.Calculate("sma", series, {{"period", -3.0}}), core::InvalidCommandError);
    REQUIRE_THROWS_AS(
        calc.Calculate("sma", series, {{"period", 3.5}}), core::InvalidCommandError);
}

TEST_CASE("SMA produces known values", "[indicators]") {
    indicators::IndicatorCalculator calc;
    auto closes = std::vector<double>{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
    auto series = MakeSeriesFromCloses(closes);
    auto result = calc.Calculate("sma", series, {{"period", 3.0}});

    REQUIRE(result.name == "sma");
    REQUIRE(result.values.size() == series.size());
    REQUIRE(result.params.at("period") == 3.0);

    for (std::size_t i = 0; i < 2; ++i) {
        REQUIRE(!result.values[i].value.has_value());
    }

    std::vector<double> expected{0.0, 0.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0};
    for (std::size_t i = 2; i < series.size(); ++i) {
        REQUIRE(ValueAt(result, i) == Approx(expected[i]));
    }
}

TEST_CASE("SMA uses default period", "[indicators]") {
    indicators::IndicatorCalculator calc;
    std::vector<double> closes(25);
    for (std::size_t i = 0; i < closes.size(); ++i) {
        closes[i] = static_cast<double>(i + 1);
    }
    auto series = MakeSeriesFromCloses(closes);
    auto result = calc.Calculate("sma", series, {});

    REQUIRE(result.params.at("period") == 20.0);
    REQUIRE(result.values[19].value.has_value());
    REQUIRE(ValueAt(result, 19) == Approx(10.5));
}

TEST_CASE("EMA tracks upward trend", "[indicators]") {
    indicators::IndicatorCalculator calc;
    auto closes = std::vector<double>{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};
    auto series = MakeSeriesFromCloses(closes);
    auto result = calc.Calculate("ema", series, {{"period", 3.0}});

    REQUIRE(result.name == "ema");
    for (std::size_t i = 0; i < 2; ++i) {
        REQUIRE(!result.values[i].value.has_value());
    }

    double prev = ValueAt(result, 2);
    for (std::size_t i = 3; i < series.size(); ++i) {
        double actual = ValueAt(result, i);
        REQUIRE(actual >= prev);
        REQUIRE(actual >= 1.0);
        REQUIRE(actual <= 10.0);
        prev = actual;
    }
}

TEST_CASE("RSI is 100 for all-up series", "[indicators]") {
    indicators::IndicatorCalculator calc;
    std::vector<double> closes(30);
    for (std::size_t i = 0; i < closes.size(); ++i) {
        closes[i] = static_cast<double>(i + 1);
    }
    auto series = MakeSeriesFromCloses(closes);
    auto result = calc.Calculate("rsi", series, {{"period", 14.0}});

    REQUIRE(result.name == "rsi");
    for (std::size_t i = 0; i < 14; ++i) {
        REQUIRE(!result.values[i].value.has_value());
    }

    REQUIRE(ValueAt(result, result.values.size() - 1) == Approx(100.0));
}

TEST_CASE("RSI stays within [0, 100]", "[indicators]") {
    indicators::IndicatorCalculator calc;
    std::vector<double> closes(50);
    for (std::size_t i = 0; i < closes.size(); ++i) {
        closes[i] = static_cast<double>(i % 10 + 1);
    }
    auto series = MakeSeriesFromCloses(closes);
    auto result = calc.Calculate("rsi", series, {{"period", 14.0}});

    for (const auto& v : result.values) {
        if (v.value.has_value()) {
            REQUIRE(v.value.value() >= 0.0);
            REQUIRE(v.value.value() <= 100.0);
        }
    }
}

TEST_CASE("MACD produces signal and histogram series", "[indicators]") {
    indicators::IndicatorCalculator calc;
    std::vector<double> closes(60);
    for (std::size_t i = 0; i < closes.size(); ++i) {
        closes[i] = static_cast<double>(i + 1);
    }
    auto series = MakeSeriesFromCloses(closes);
    auto result = calc.Calculate("macd", series, {});

    REQUIRE(result.name == "macd");
    REQUIRE(result.values.size() == series.size());
    REQUIRE(result.extra_series.count("signal") == 1);
    REQUIRE(result.extra_series.count("histogram") == 1);
    REQUIRE(result.extra_series.at("signal").size() == series.size());
    REQUIRE(result.extra_series.at("histogram").size() == series.size());
}

TEST_CASE("Bollinger Bands upper >= middle >= lower", "[indicators]") {
    indicators::IndicatorCalculator calc;
    std::vector<double> closes(30);
    for (std::size_t i = 0; i < closes.size(); ++i) {
        closes[i] = static_cast<double>(i + 1);
    }
    auto series = MakeSeriesFromCloses(closes);
    auto result = calc.Calculate("bollinger_bands", series, {});

    REQUIRE(result.name == "bollinger_bands");
    REQUIRE(result.extra_series.count("upper") == 1);
    REQUIRE(result.extra_series.count("lower") == 1);

    const auto& upper = result.extra_series.at("upper");
    const auto& lower = result.extra_series.at("lower");

    for (std::size_t i = 19; i < series.size(); ++i) {
        double middle = ValueAt(result, i);
        REQUIRE(upper[i].value.has_value());
        REQUIRE(lower[i].value.has_value());
        REQUIRE(upper[i].value.value() >= middle);
        REQUIRE(lower[i].value.value() <= middle);
    }
}

TEST_CASE("ATR is positive", "[indicators]") {
    indicators::IndicatorCalculator calc;
    auto series = MakeSeriesFromCloses(
        {10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0, 19.0, 20.0,
         21.0, 22.0, 23.0, 24.0, 25.0});
    auto result = calc.Calculate("atr", series, {{"period", 5.0}});

    REQUIRE(result.name == "atr");
    for (std::size_t i = 0; i < 5; ++i) {
        REQUIRE(!result.values[i].value.has_value());
    }
    for (std::size_t i = 5; i < series.size(); ++i) {
        REQUIRE(ValueAt(result, i) > 0.0);
    }
}

TEST_CASE("OBV starts at first volume", "[indicators]") {
    indicators::IndicatorCalculator calc;
    auto series = MakeSeriesFromCloses({10.0, 11.0, 10.0, 12.0, 11.0});
    auto result = calc.Calculate("obv", series, {});

    REQUIRE(result.name == "obv");
    for (const auto& v : result.values) {
        REQUIRE(v.value.has_value());
    }
    REQUIRE(ValueAt(result, 0) == Approx(static_cast<double>(series[0].volume)));
}

TEST_CASE("VWAP is positive", "[indicators]") {
    indicators::IndicatorCalculator calc;
    auto series = MakeSeriesFromCloses({10.0, 11.0, 12.0, 13.0, 14.0});
    auto result = calc.Calculate("vwap", series, {});

    REQUIRE(result.name == "vwap");
    for (const auto& v : result.values) {
        REQUIRE(v.value.has_value());
        REQUIRE(v.value.value() > 0.0);
    }
}
