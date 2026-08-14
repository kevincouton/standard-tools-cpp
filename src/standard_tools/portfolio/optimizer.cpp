#include "standard_tools/portfolio/optimizer.hpp"

#include "standard_tools/core/errors.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace standard_tools::portfolio {

namespace {

constexpr double kCovRidge = 1e-8;
constexpr double kDegeneracyEps = 1e-12;
constexpr double kInversePivotEps = 1e-15;

bool IsFinite(double v) { return std::isfinite(v); }

double NaN() { return std::numeric_limits<double>::quiet_NaN(); }

void ValidateReturnMatrix(const std::vector<std::vector<double>>& returns,
                          const std::vector<std::string>& labels) {
    if (returns.empty()) {
        throw core::DataQualityError{"returns matrix must contain at least one series"};
    }
    if (labels.size() != returns.size()) {
        std::ostringstream oss;
        oss << "expected " << returns.size() << " labels, got " << labels.size();
        throw core::InvalidCommandError{oss.str()};
    }
    std::unordered_set<std::string> seen;
    for (const auto& label : labels) {
        if (!seen.insert(label).second) {
            throw core::InvalidCommandError{"duplicate label " + label};
        }
    }
    const std::size_t obs = returns[0].size();
    if (obs < 2) {
        throw core::DataQualityError{"each return series must contain at least two observations"};
    }
    for (std::size_t i = 0; i < returns.size(); ++i) {
        if (returns[i].size() != obs) {
            std::ostringstream oss;
            oss << "series " << i << " has length " << returns[i].size()
                << " but series 0 has length " << obs;
            throw core::DataQualityError{oss.str()};
        }
        for (std::size_t j = 0; j < obs; ++j) {
            if (!IsFinite(returns[i][j])) {
                std::ostringstream oss;
                oss << "series " << i << " contains non-finite value at index " << j;
                throw core::DataQualityError{oss.str()};
            }
        }
    }
}

std::vector<std::vector<double>> MatAlloc(std::size_t n) {
    return std::vector<std::vector<double>>(n, std::vector<double>(n, 0.0));
}

std::vector<double> VecDotMat(const std::vector<std::vector<double>>& a,
                              const std::vector<double>& x) {
    const std::size_t m = a.size();
    std::vector<double> out(m, 0.0);
    for (std::size_t i = 0; i < m; ++i) {
        double sum = 0.0;
        for (std::size_t j = 0; j < x.size(); ++j) {
            sum += a[i][j] * x[j];
        }
        out[i] = sum;
    }
    return out;
}

double VecDot(const std::vector<double>& a, const std::vector<double>& b) {
    double sum = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

double QuadraticForm(const std::vector<double>& x,
                     const std::vector<std::vector<double>>& a) {
    const std::size_t n = x.size();
    double result = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            result += x[i] * a[i][j] * x[j];
        }
    }
    return result;
}

std::vector<std::vector<double>> MatInverse(const std::vector<std::vector<double>>& m) {
    const std::size_t n = m.size();
    if (n == 0) {
        throw core::InvalidCommandError{"cannot invert empty matrix"};
    }
    for (const auto& row : m) {
        if (row.size() != n) {
            throw core::InvalidCommandError{"matrix must be square"};
        }
    }

    // Build augmented matrix [m | I].
    auto aug = m;
    for (std::size_t i = 0; i < n; ++i) {
        aug[i].resize(2 * n, 0.0);
        aug[i][n + i] = 1.0;
    }

    for (std::size_t col = 0; col < n; ++col) {
        // Partial pivot.
        std::size_t pivot = col;
        double max_val = std::abs(aug[col][col]);
        for (std::size_t row = col + 1; row < n; ++row) {
            const double v = std::abs(aug[row][col]);
            if (v > max_val) {
                max_val = v;
                pivot = row;
            }
        }
        if (max_val < kInversePivotEps) {
            throw core::InternalError{"matrix is singular or near-singular"};
        }
        if (pivot != col) {
            std::swap(aug[col], aug[pivot]);
        }

        // Normalize pivot row.
        const double pivot_val = aug[col][col];
        for (std::size_t j = 0; j < 2 * n; ++j) {
            aug[col][j] /= pivot_val;
        }

        // Eliminate column in all other rows.
        for (std::size_t row = 0; row < n; ++row) {
            if (row == col) {
                continue;
            }
            const double factor = aug[row][col];
            if (factor == 0.0) {
                continue;
            }
            for (std::size_t j = 0; j < 2 * n; ++j) {
                aug[row][j] -= factor * aug[col][j];
            }
        }
    }

    std::vector<std::vector<double>> inv = MatAlloc(n);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            inv[i][j] = aug[i][n + j];
        }
    }
    return inv;
}

std::vector<std::vector<double>> SampleCovariance(
    const std::vector<std::vector<double>>& centered) {
    const std::size_t n = centered.size();
    const std::size_t obs = centered[0].size();
    auto cov = MatAlloc(n);
    const double scale = 1.0 / (static_cast<double>(obs) - 1.0);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i; j < n; ++j) {
            double sum = 0.0;
            for (std::size_t t = 0; t < obs; ++t) {
                sum += centered[i][t] * centered[j][t];
            }
            cov[i][j] = sum * scale;
            cov[j][i] = cov[i][j];
        }
    }
    return cov;
}

double Mean(const std::vector<double>& values) {
    double sum = 0.0;
    for (double v : values) {
        sum += v;
    }
    return sum / static_cast<double>(values.size());
}

double Sum(const std::vector<double>& values) {
    double s = 0.0;
    for (double v : values) {
        s += v;
    }
    return s;
}

std::vector<double> Scale(const std::vector<double>& values, double factor) {
    std::vector<double> out(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) {
        out[i] = values[i] * factor;
    }
    return out;
}

std::vector<double> Blend(const std::vector<double>& a,
                          const std::vector<double>& b,
                          double alpha) {
    std::vector<double> out(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        out[i] = alpha * a[i] + (1.0 - alpha) * b[i];
    }
    return out;
}

double Clamp(double v, double min, double max) {
    if (v < min) {
        return min;
    }
    if (v > max) {
        return max;
    }
    return v;
}

std::vector<double> ComputeMeans(const std::vector<std::vector<double>>& returns) {
    std::vector<double> means;
    means.reserve(returns.size());
    for (const auto& series : returns) {
        means.push_back(Mean(series));
    }
    return means;
}

std::vector<std::vector<double>> CenterReturns(
    const std::vector<std::vector<double>>& returns,
    const std::vector<double>& means) {
    const std::size_t n = returns.size();
    const std::size_t obs = returns[0].size();
    std::vector<std::vector<double>> centered(n, std::vector<double>(obs));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t t = 0; t < obs; ++t) {
            centered[i][t] = returns[i][t] - means[i];
        }
    }
    return centered;
}

void PortfolioMetrics(const std::vector<double>& weights,
                      const std::vector<double>& means,
                      const std::vector<std::vector<double>>& cov,
                      double rf,
                      double& expected_return,
                      double& volatility,
                      double& sharpe) {
    expected_return = VecDot(weights, means);
    const double variance = QuadraticForm(weights, cov);
    volatility = std::sqrt(std::max(0.0, variance));
    if (volatility > 0.0) {
        sharpe = (expected_return - rf) / volatility;
    } else {
        sharpe = std::numeric_limits<double>::lowest();
    }
}

double SolveQuadraticForBlend(double a, double b, double c) {
    const double discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0 || std::abs(a) < kDegeneracyEps) {
        if (std::abs(b) < kDegeneracyEps) {
            return 0.5;
        }
        return Clamp(-c / b, 0.0, 1.0);
    }
    const double sqrt_d = std::sqrt(discriminant);
    const double alpha1 = (-b + sqrt_d) / (2.0 * a);
    const double alpha2 = (-b - sqrt_d) / (2.0 * a);
    return Clamp(std::max(alpha1, alpha2), 0.0, 1.0);
}

std::vector<double> TargetVolatilityBlend(const std::vector<double>& w_ms,
                                          const std::vector<double>& w_mv,
                                          const std::vector<double>& means,
                                          const std::vector<std::vector<double>>& cov,
                                          double rf,
                                          double target_vol,
                                          double ret_mv,
                                          double vol_mv,
                                          double ret_ms,
                                          double vol_ms) {
    double target = std::max(vol_mv, std::min(vol_ms, target_vol));
    if (target <= vol_mv || std::abs(vol_ms - vol_mv) < kDegeneracyEps) {
        return w_mv;
    }
    if (target >= vol_ms) {
        return w_ms;
    }

    const double var_ms = vol_ms * vol_ms;
    const double var_mv = vol_mv * vol_mv;
    const auto sigma_wmv = VecDotMat(cov, w_mv);
    const double cov_ms_mv = VecDot(w_ms, sigma_wmv);
    const double target_var = target * target;
    const double a = var_ms + var_mv - 2.0 * cov_ms_mv;
    const double b = 2.0 * cov_ms_mv - 2.0 * var_mv;
    const double c = var_mv - target_var;

    const double alpha = SolveQuadraticForBlend(a, b, c);
    return Blend(w_ms, w_mv, alpha);
}

PortfolioResult BuildResult(const std::vector<double>& weights,
                            const std::vector<std::string>& labels,
                            double expected_return,
                            double volatility,
                            double sharpe) {
    PortfolioResult result;
    for (std::size_t i = 0; i < labels.size(); ++i) {
        result.weights[labels[i]] = weights[i];
    }
    result.expected_return = expected_return;
    result.volatility = volatility;
    result.sharpe_ratio = sharpe;
    result.Validate();
    return result;
}

}  // namespace

PortfolioResult MeanVariance(const MeanVarianceRequest& request) {
    if (!IsFinite(request.risk_free_rate)) {
        throw core::InvalidCommandError{"risk_free_rate must be finite"};
    }
    ValidateReturnMatrix(request.returns, request.labels);

    if (request.objective == kObjectiveTargetReturn) {
        if (!request.target_return.has_value() || !IsFinite(*request.target_return)) {
            throw core::InvalidCommandError{"target_return must be a finite number"};
        }
    } else if (request.objective == kObjectiveTargetVolatility) {
        if (!request.target_volatility.has_value() || !IsFinite(*request.target_volatility) ||
            *request.target_volatility < 0.0) {
            throw core::InvalidCommandError{
                "target_volatility must be a finite non-negative number"};
        }
    } else if (request.objective != kObjectiveMaxSharpe &&
               request.objective != kObjectiveMinVolatility) {
        throw core::InvalidCommandError{"unknown objective " + request.objective};
    }

    const std::size_t n = request.returns.size();

    const auto means = ComputeMeans(request.returns);
    const auto centered = CenterReturns(request.returns, means);
    auto cov = SampleCovariance(centered);
    for (std::size_t i = 0; i < n; ++i) {
        cov[i][i] += kCovRidge;
    }

    const auto inv_cov = MatInverse(cov);

    // Global minimum-variance portfolio: w = Sigma^{-1} 1 / (1' Sigma^{-1} 1).
    std::vector<double> ones(n, 1.0);
    const auto inv1 = VecDotMat(inv_cov, ones);
    const double denom_mv = Sum(inv1);
    if (std::abs(denom_mv) < kDegeneracyEps) {
        throw core::InternalError{"minimum-variance portfolio is degenerate"};
    }
    const auto w_mv = Scale(inv1, 1.0 / denom_mv);

    // Maximum-Sharpe portfolio: w ∝ Sigma^{-1} (mu - rf * 1).
    std::vector<double> excess(n);
    for (std::size_t i = 0; i < n; ++i) {
        excess[i] = means[i] - request.risk_free_rate;
    }
    const auto k = VecDotMat(inv_cov, excess);
    const double sum_k = Sum(k);
    std::vector<double> w_ms;
    if (std::abs(sum_k) < kDegeneracyEps) {
        w_ms = w_mv;
    } else {
        w_ms = Scale(k, 1.0 / sum_k);
    }

    double ret_mv = 0.0, vol_mv = 0.0, sharpe_mv = 0.0;
    double ret_ms = 0.0, vol_ms = 0.0, sharpe_ms = 0.0;
    PortfolioMetrics(w_mv, means, cov, request.risk_free_rate, ret_mv, vol_mv, sharpe_mv);
    PortfolioMetrics(w_ms, means, cov, request.risk_free_rate, ret_ms, vol_ms, sharpe_ms);

    std::vector<double> weights;
    if (request.objective == kObjectiveMaxSharpe) {
        weights = w_ms;
    } else if (request.objective == kObjectiveMinVolatility) {
        weights = w_mv;
    } else if (request.objective == kObjectiveTargetReturn) {
        double alpha = 0.0;
        if (std::abs(ret_ms - ret_mv) >= kDegeneracyEps) {
            alpha = (*request.target_return - ret_mv) / (ret_ms - ret_mv);
        }
        alpha = Clamp(alpha, 0.0, 1.0);
        weights = Blend(w_ms, w_mv, alpha);
    } else {  // target_volatility
        weights = TargetVolatilityBlend(w_ms, w_mv, means, cov, request.risk_free_rate,
                                        *request.target_volatility, ret_mv, vol_mv, ret_ms,
                                        vol_ms);
    }

    double expected_return = 0.0, volatility = 0.0, sharpe = 0.0;
    PortfolioMetrics(weights, means, cov, request.risk_free_rate, expected_return, volatility,
                     sharpe);

    return BuildResult(weights, request.labels, expected_return, volatility, sharpe);
}

}  // namespace standard_tools::portfolio
