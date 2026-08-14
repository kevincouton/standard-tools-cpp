#pragma once

#include "standard_tools/backtest/result.hpp"
#include "standard_tools/backtest/signal.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace standard_tools::backtest {

/// Strategy generates a signal for every bar in a price series.
class Strategy {
public:
    virtual ~Strategy() = default;

    /// Returns the canonical strategy identifier.
    virtual std::string Name() const = 0;

    /// Produces one SignalResult per OHLCV bar using the supplied parameters.
    virtual std::vector<SignalResult> Signals(
        const std::vector<core::OHLCV>& series,
        const std::unordered_map<std::string, double>& params) const = 0;
};

/// Returns the named built-in strategy, or nullptr if the name is unknown.
std::unique_ptr<Strategy> BuiltinStrategy(const std::string& name);

}  // namespace standard_tools::backtest
