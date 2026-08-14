#include "standard_tools/metrics/risk_return.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <sstream>
#include <vector>

namespace standard_tools::metrics {

namespace {

constexpr double kVaRQuantile = 0.05;

bool IsFinite(double v) { return std::isfinite(v); }

double NaN() { return std::numeric_limits<double>::quiet_NaN(); }

std::vector<double> ComputeReturns(const std::vector<double>& prices) {
    std::vector<double> returns;
    returns.reserve(prices.size() - 1);
    for (std::size_t i = 1; i < prices.size(); ++i) {
        returns.push_back(prices[i] / prices[i - 1] - 1.0);
    }
    return returns;
}

double CumulativeReturn(const std::vector<double>& returns) {
    double product = 1.0;
    for (double r : returns) {
        product *= 1.0 + r;
    }
    return product - 1.0;
}

double Mean(const std::vector<double>& values) {
    if (values.empty()) {
        return NaN();
    }
    double sum = 0.0;
    for (double v : values) {
        sum += v;
    }
    return sum / static_cast<double>(values.size());
}

double AnnualizedVolatility(const std::vector<double>& returns) {
    if (returns.size() < 2) {
        return 0.0;
    }
    const double mean = Mean(returns);
    double sum_sq = 0.0;
    for (double r : returns) {
        const double d = r - mean;
        sum_sq += d * d;
    }
    const double variance = sum_sq / static_cast<double>(returns.size());
    return std::sqrt(variance) * std::sqrt(kTradingDaysPerYear);
}

double SharpeRatio(double cagr, double volatility, double risk_free_rate) {
    if (volatility <= 0.0 || !IsFinite(volatility)) {
        return NaN();
    }
    return (cagr - risk_free_rate) / volatility;
}

double SortinoRatio(const std::vector<double>& returns, double cagr, double risk_free_rate) {
    if (returns.empty()) {
        return NaN();
    }
    const double periodic_rf = risk_free_rate / kTradingDaysPerYear;
    double downside_sum = 0.0;
    for (double r : returns) {
        const double d = r - periodic_rf;
        if (d < 0.0) {
            downside_sum += d * d;
        }
    }
    const double downside_deviation =
        std::sqrt(downside_sum / static_cast<double>(returns.size())) * std::sqrt(kTradingDaysPerYear);
    if (downside_deviation <= 0.0 || !IsFinite(downside_deviation)) {
        return NaN();
    }
    return (cagr - risk_free_rate) / downside_deviation;
}

double MaxDrawdown(const std::vector<double>& returns) {
    double equity = 1.0;
    double peak = 1.0;
    double max_dd = 0.0;
    for (double r : returns) {
        equity *= 1.0 + r;
        if (equity > peak) {
            peak = equity;
        }
        const double drawdown = (peak - equity) / peak;
        if (drawdown > max_dd) {
            max_dd = drawdown;
        }
    }
    return -max_dd;
}

double HistoricalVaR(const std::vector<double>& returns, double quantile) {
    if (returns.empty()) {
        return NaN();
    }
    std::vector<double> sorted(returns);
    std::sort(sorted.begin(), sorted.end());
    const auto index = static_cast<std::size_t>(
        std::lround(quantile * static_cast<double>(sorted.size() - 1)));
    const std::size_t clamped_index = std::min(index, sorted.size() - 1);
    return sorted[clamped_index];
}

double HistoricalCVaR(const std::vector<double>& returns, double quantile) {
    const double var_value = HistoricalVaR(returns, quantile);
    if (!IsFinite(var_value)) {
        return NaN();
    }
    double sum = 0.0;
    std::size_t count = 0;
    for (double r : returns) {
        if (r <= var_value) {
            sum += r;
            ++count;
        }
    }
    if (count == 0) {
        return NaN();
    }
    return sum / static_cast<double>(count);
}

double CalmarRatio(double cagr, double max_drawdown) {
    const double dd = std::abs(max_drawdown);
    if (dd <= 0.0) {
        return NaN();
    }
    return cagr / dd;
}

void ValidatePrices(const std::vector<double>& prices) {
    if (prices.size() < 2) {
        std::ostringstream oss;
        oss << "at least two close prices required, got " << prices.size();
        throw core::InsufficientDataError{oss.str()};
    }
    for (std::size_t i = 0; i < prices.size(); ++i) {
        const double p = prices[i];
        if (p <= 0.0 || !IsFinite(p)) {
            std::ostringstream oss;
            oss << "price at index " << i << " must be positive and finite (got " << p << ")";
            throw core::InvalidPricesError{oss.str()};
        }
    }
}

}  // namespace

ReturnMetrics RiskReturnCalculator::CalculateReturnMetrics(const std::vector<double>& series,
                                                           double risk_free_rate) const {
    ValidatePrices(series);
    const auto returns = ComputeReturns(series);
    const double cumulative = CumulativeReturn(returns);
    const double periods = static_cast<double>(returns.size());
    const double cagr = std::pow(1.0 + cumulative, kTradingDaysPerYear / periods) - 1.0;
    const double vol = AnnualizedVolatility(returns);
    return ReturnMetrics{
        .cumulative_return = cumulative,
        .cagr = cagr,
        .annualized_volatility = vol,
    };
}

RiskMetrics RiskReturnCalculator::CalculateRiskMetrics(const std::vector<double>& series,
                                                       double risk_free_rate) const {
    ValidatePrices(series);
    const auto returns = ComputeReturns(series);
    const double cumulative = CumulativeReturn(returns);
    const double periods = static_cast<double>(returns.size());
    const double cagr = std::pow(1.0 + cumulative, kTradingDaysPerYear / periods) - 1.0;
    const double vol = AnnualizedVolatility(returns);
    const double max_dd = MaxDrawdown(returns);
    return RiskMetrics{
        .sharpe_ratio = SharpeRatio(cagr, vol, risk_free_rate),
        .sortino_ratio = SortinoRatio(returns, cagr, risk_free_rate),
        .max_drawdown = max_dd,
        .calmar_ratio = CalmarRatio(cagr, max_dd),
        .var_95 = HistoricalVaR(returns, kVaRQuantile),
        .cvar_95 = HistoricalCVaR(returns, kVaRQuantile),
        .volatility = vol,
    };
}

}  // namespace standard_tools::metrics
