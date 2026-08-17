#include "standard_tools/portfolio/risk_parity.hpp"

#include "standard_tools/core/errors.hpp"

#include <cmath>
#include <cstddef>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace standard_tools::portfolio {

namespace {

constexpr double kDegeneracyEps = 1e-12;

bool IsFinite(double v) { return std::isfinite(v); }

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

std::vector<std::vector<double>> MatAlloc(std::size_t n) {
    return std::vector<std::vector<double>>(n, std::vector<double>(n, 0.0));
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

}  // namespace

PortfolioResult RiskParity(const RiskParityRequest& request) {
    ValidateReturnMatrix(request.returns, request.labels);

    const std::size_t n = request.returns.size();
    const std::size_t obs = request.returns[0].size();

    std::vector<double> means(n);
    for (std::size_t i = 0; i < n; ++i) {
        means[i] = Mean(request.returns[i]);
    }
    std::vector<std::vector<double>> centered(n, std::vector<double>(obs));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t t = 0; t < obs; ++t) {
            centered[i][t] = request.returns[i][t] - means[i];
        }
    }
    const auto cov = SampleCovariance(centered);

    double total_variance = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        total_variance += cov[i][i];
    }
    if (total_variance < kDegeneracyEps) {
        throw core::DataQualityError{
            "all assets have zero volatility; cannot compute risk-parity weights"};
    }

    // Equal-budget risk parity via a damped fixed-point iteration on risk
    // contributions (same scheme as the Kotlin port's RiskParityOptimizer,
    // with damping to avoid oscillation on highly correlated inputs):
    //   rc_i = w_i * (cov * w)_i,  target_i = sum(rc) / n
    //   w_i <- w_i * target_i / rc_i, then renormalize.
    std::vector<double> weights(n, 1.0 / static_cast<double>(n));
    constexpr int kMaxIterations = 1000;
    constexpr double kConvergenceTol = 1e-10;
    constexpr double kDamping = 0.5;
    for (int iter = 0; iter < kMaxIterations; ++iter) {
        std::vector<double> rc(n, 0.0);
        for (std::size_t i = 0; i < n; ++i) {
            double mrc = 0.0;
            for (std::size_t j = 0; j < n; ++j) {
                mrc += cov[i][j] * weights[j];
            }
            rc[i] = weights[i] * mrc;
        }
        const double total_rc = Sum(rc);
        if (total_rc < kDegeneracyEps) {
            break;
        }
        const double target = total_rc / static_cast<double>(n);

        bool converged = true;
        for (double r : rc) {
            if (std::abs(r - target) >= kConvergenceTol) {
                converged = false;
                break;
            }
        }
        if (converged) {
            break;
        }

        std::vector<double> next(n);
        for (std::size_t i = 0; i < n; ++i) {
            next[i] = weights[i] + kDamping *
                                      (weights[i] * target / std::max(rc[i], kDegeneracyEps) -
                                       weights[i]);
        }
        const double next_total = Sum(next);
        if (next_total < kDegeneracyEps) {
            break;
        }
        for (std::size_t i = 0; i < n; ++i) {
            weights[i] = next[i] / next_total;
        }
    }

    double expected_return = 0.0, volatility = 0.0, sharpe = 0.0;
    PortfolioMetrics(weights, means, cov, 0.0, expected_return, volatility, sharpe);

    PortfolioResult result;
    for (std::size_t i = 0; i < n; ++i) {
        result.weights[request.labels[i]] = weights[i];
    }
    result.expected_return = expected_return;
    result.volatility = volatility;
    result.sharpe_ratio = sharpe;
    result.Validate();
    return result;
}

}  // namespace standard_tools::portfolio
