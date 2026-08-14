#pragma once

#include "standard_tools/indicators/indicator.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace standard_tools::indicators {

/// Unified entry point for calculating technical indicators.
class IndicatorCalculator {
public:
    /// Calculate the named indicator over the supplied OHLCV series using the
    /// provided parameters.
    ///
    /// Supported indicators:
    ///   - "sma"  — simple moving average of close prices (period, default 20)
    ///   - "ema"  — exponential moving average of close prices (period, default 20)
    ///   - "rsi"  — relative strength index (period, default 14)
    ///   - "macd" — MACD line (fast 12, slow 26, signal 9)
    ///   - "bollinger_bands" — middle SMA band (period 20, std_dev 2)
    ///   - "atr"  — average true range (period, default 14)
    ///   - "obv"  — on-balance volume
    ///   - "vwap" — volume-weighted average price
    ///
    /// Unknown indicator names and invalid parameter values raise
    /// core::InvalidCommandError.
    IndicatorResult Calculate(
        const std::string& name,
        const std::vector<core::OHLCV>& series,
        const std::unordered_map<std::string, double>& params) const;
};

}  // namespace standard_tools::indicators
