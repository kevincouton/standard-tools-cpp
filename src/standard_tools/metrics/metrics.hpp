#pragma once

#include <nlohmann/json.hpp>

namespace standard_tools::metrics {

using json = nlohmann::json;

/// Return metrics computed from a price series.
struct ReturnMetrics {
    /// Compounded total return over the observed period (geometric product).
    double cumulative_return = 0.0;

    /// Compound annual growth rate, annualized using 252 trading days.
    double cagr = 0.0;

    /// Annualized population standard deviation of simple returns.
    double annualized_volatility = 0.0;
};

/// Risk metrics computed from a price series.
struct RiskMetrics {
    /// Annualized Sharpe ratio: (CAGR - risk-free rate) / annualized volatility.
    double sharpe_ratio = 0.0;

    /// Annualized Sortino ratio using downside deviation against the periodic
    /// risk-free rate.
    double sortino_ratio = 0.0;

    /// Maximum peak-to-trough drawdown as a negative fraction of equity.
    double max_drawdown = 0.0;

    /// Calmar ratio: CAGR divided by the absolute maximum drawdown.
    double calmar_ratio = 0.0;

    /// Historical value at risk at the 5th percentile (nearest-rank method).
    double var_95 = 0.0;

    /// Historical conditional value at risk (expected shortfall) at the 5th
    /// percentile: mean of returns less than or equal to VaR95.
    double cvar_95 = 0.0;

    /// Annualized volatility, identical to ReturnMetrics::annualized_volatility.
    double volatility = 0.0;
};

inline void to_json(json& j, const ReturnMetrics& m) {
    j = json::object();
    j["cumulative_return"] = m.cumulative_return;
    j["cagr"] = m.cagr;
    j["annualized_volatility"] = m.annualized_volatility;
}

inline void to_json(json& j, const RiskMetrics& m) {
    j = json::object();
    j["sharpe_ratio"] = m.sharpe_ratio;
    j["sortino_ratio"] = m.sortino_ratio;
    j["max_drawdown"] = m.max_drawdown;
    j["calmar_ratio"] = m.calmar_ratio;
    j["var_95"] = m.var_95;
    j["cvar_95"] = m.cvar_95;
    j["volatility"] = m.volatility;
}

}  // namespace standard_tools::metrics
