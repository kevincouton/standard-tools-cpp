#include "standard_tools/backtest/strategy.hpp"

#include "standard_tools/core/errors.hpp"
#include "standard_tools/indicators/calculator.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <vector>

namespace standard_tools::backtest {

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

double ParamOrDefault(
    const std::unordered_map<std::string, double>& params,
    const std::string& key,
    double default_value) {
    auto it = params.find(key);
    if (it == params.end()) {
        return default_value;
    }
    return it->second;
}

std::vector<SignalResult> CrossoverSignals(
    const std::vector<core::OHLCV>& series,
    const std::vector<indicators::IndicatorValue>& fast,
    const std::vector<indicators::IndicatorValue>& slow) {
    std::vector<SignalResult> signals;
    signals.reserve(series.size());
    for (std::size_t i = 0; i < series.size(); ++i) {
        Signal s = Signal::Hold;
        if (i < fast.size() && i < slow.size() && fast[i].value.has_value() &&
            slow[i].value.has_value()) {
            double f = fast[i].value.value();
            double sl = slow[i].value.value();
            if (i == 0) {
                if (f > sl) {
                    s = Signal::Buy;
                } else if (f < sl) {
                    s = Signal::Sell;
                }
            } else if (i - 1 < fast.size() && i - 1 < slow.size() &&
                       fast[i - 1].value.has_value() && slow[i - 1].value.has_value()) {
                double pf = fast[i - 1].value.value();
                double ps = slow[i - 1].value.value();
                if (f > sl && !(pf > ps)) {
                    s = Signal::Buy;
                } else if (f < sl && !(pf < ps)) {
                    s = Signal::Sell;
                }
            }
        }
        signals.push_back(SignalResult{.date = series[i].date, .signal = s});
    }
    return signals;
}

class BuyAndHold : public Strategy {
public:
    std::string Name() const override { return "buy_and_hold"; }

    std::vector<SignalResult> Signals(
        const std::vector<core::OHLCV>& series,
        const std::unordered_map<std::string, double>& /*params*/) const override {
        std::vector<SignalResult> signals;
        signals.reserve(series.size());
        for (std::size_t i = 0; i < series.size(); ++i) {
            Signal s = (i == 0) ? Signal::Buy : Signal::Hold;
            signals.push_back(SignalResult{.date = series[i].date, .signal = s});
        }
        return signals;
    }
};

class SmaCrossover : public Strategy {
public:
    std::string Name() const override { return "sma_crossover"; }

    std::vector<SignalResult> Signals(
        const std::vector<core::OHLCV>& series,
        const std::unordered_map<std::string, double>& params) const override {
        indicators::IndicatorCalculator calc;

        std::unordered_map<std::string, double> fast_params = params;
        fast_params["period"] = ParamOrDefault(params, "fast", 10.0);
        std::unordered_map<std::string, double> slow_params = params;
        slow_params["period"] = ParamOrDefault(params, "slow", 30.0);

        auto fast_result = calc.Calculate("sma", series, fast_params);
        auto slow_result = calc.Calculate("sma", series, slow_params);

        return CrossoverSignals(series, fast_result.values, slow_result.values);
    }
};

class RsiThreshold : public Strategy {
public:
    std::string Name() const override { return "rsi_threshold"; }

    std::vector<SignalResult> Signals(
        const std::vector<core::OHLCV>& series,
        const std::unordered_map<std::string, double>& params) const override {
        indicators::IndicatorCalculator calc;
        auto rsi_result = calc.Calculate("rsi", series, params);

        double oversold = ParamOrDefault(params, "oversold", 30.0);
        double overbought = ParamOrDefault(params, "overbought", 70.0);

        std::vector<SignalResult> signals;
        signals.reserve(series.size());
        for (std::size_t i = 0; i < series.size(); ++i) {
            Signal s = Signal::Hold;
            if (i > 0 && rsi_result.values[i].value.has_value()) {
                double v = rsi_result.values[i].value.value();
                if (v < oversold) {
                    s = Signal::Buy;
                } else if (v > overbought) {
                    s = Signal::Sell;
                }
            }
            signals.push_back(SignalResult{.date = series[i].date, .signal = s});
        }
        return signals;
    }
};

class BollingerReversion : public Strategy {
public:
    std::string Name() const override { return "bollinger_bands_reversion"; }

    std::vector<SignalResult> Signals(
        const std::vector<core::OHLCV>& series,
        const std::unordered_map<std::string, double>& params) const override {
        indicators::IndicatorCalculator calc;
        auto bb_result = calc.Calculate("bollinger_bands", series, params);

        auto upper_it = bb_result.extra_series.find("upper");
        auto lower_it = bb_result.extra_series.find("lower");

        std::vector<SignalResult> signals;
        signals.reserve(series.size());
        for (std::size_t i = 0; i < series.size(); ++i) {
            Signal s = Signal::Hold;
            if (upper_it != bb_result.extra_series.end() &&
                lower_it != bb_result.extra_series.end() && i < upper_it->second.size() &&
                i < lower_it->second.size()) {
                const auto& upper_iv = upper_it->second[i];
                const auto& lower_iv = lower_it->second[i];
                if (upper_iv.value.has_value() && series[i].close > upper_iv.value.value()) {
                    s = Signal::Sell;
                } else if (lower_iv.value.has_value() &&
                           series[i].close < lower_iv.value.value()) {
                    s = Signal::Buy;
                }
            }
            signals.push_back(SignalResult{.date = series[i].date, .signal = s});
        }
        return signals;
    }
};

class MacdCrossover : public Strategy {
public:
    std::string Name() const override { return "macd_crossover"; }

    std::vector<SignalResult> Signals(
        const std::vector<core::OHLCV>& series,
        const std::unordered_map<std::string, double>& params) const override {
        indicators::IndicatorCalculator calc;
        auto macd_result = calc.Calculate("macd", series, params);

        auto signal_it = macd_result.extra_series.find("signal");
        if (signal_it == macd_result.extra_series.end()) {
            return std::vector<SignalResult>(
                series.size(), SignalResult{.date = core::Date{}, .signal = Signal::Hold});
        }

        return CrossoverSignals(series, macd_result.values, signal_it->second);
    }
};

}  // namespace

std::unique_ptr<Strategy> BuiltinStrategy(const std::string& name) {
    if (name == "buy_and_hold") {
        return std::make_unique<BuyAndHold>();
    }
    if (name == "sma_crossover") {
        return std::make_unique<SmaCrossover>();
    }
    if (name == "rsi_threshold") {
        return std::make_unique<RsiThreshold>();
    }
    if (name == "bollinger_bands_reversion") {
        return std::make_unique<BollingerReversion>();
    }
    if (name == "macd_crossover") {
        return std::make_unique<MacdCrossover>();
    }
    return nullptr;
}

}  // namespace standard_tools::backtest
