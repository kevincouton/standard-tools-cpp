#include "standard_tools/portfolio/black_litterman.hpp"

#include "standard_tools/core/errors.hpp"

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

std::vector<std::vector<double>> MatCopy(const std::vector<std::vector<double>>& m) {
    return m;
}

std::vector<std::vector<double>> MatScale(const std::vector<std::vector<double>>& m,
                                          double s) {
    auto out = m;
    for (auto& row : out) {
        for (auto& v : row) {
            v *= s;
        }
    }
    return out;
}

std::vector<std::vector<double>> MatAdd(const std::vector<std::vector<double>>& a,
                                        const std::vector<std::vector<double>>& b) {
    const std::size_t n = a.size();
    auto out = MatAlloc(n);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            out[i][j] = a[i][j] + b[i][j];
        }
    }
    return out;
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

    auto aug = MatCopy(m);
    for (std::size_t i = 0; i < n; ++i) {
        aug[i].resize(2 * n, 0.0);
        aug[i][n + i] = 1.0;
    }

    for (std::size_t col = 0; col < n; ++col) {
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

        const double pivot_val = aug[col][col];
        for (std::size_t j = 0; j < 2 * n; ++j) {
            aug[col][j] /= pivot_val;
        }

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

void ValidateBlackLittermanRequest(const BlackLittermanRequest& req) {
    if (req.tau <= 0.0 || !IsFinite(req.tau)) {
        throw core::InvalidCommandError{"tau must be a positive finite number"};
    }
    if (req.risk_aversion <= 0.0 || !IsFinite(req.risk_aversion)) {
        throw core::InvalidCommandError{"risk_aversion must be a positive finite number"};
    }
    ValidateReturnMatrix(req.returns, req.labels);
    const std::size_t n = req.returns.size();
    if (req.market_caps.size() != n) {
        std::ostringstream oss;
        oss << "expected " << n << " market caps, got " << req.market_caps.size();
        throw core::InvalidCommandError{oss.str()};
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (req.market_caps[i] <= 0.0 || !IsFinite(req.market_caps[i])) {
            std::ostringstream oss;
            oss << "market cap " << i << " must be positive and finite";
            throw core::InvalidCommandError{oss.str()};
        }
    }
    if (req.p_matrix.empty()) {
        throw core::InvalidCommandError{"P matrix must contain at least one view"};
    }
    if (req.p_matrix.size() != req.q_vector.size()) {
        std::ostringstream oss;
        oss << "P matrix has " << req.p_matrix.size() << " rows but Q has "
            << req.q_vector.size() << " elements";
        throw core::InvalidCommandError{oss.str()};
    }
    for (std::size_t i = 0; i < req.p_matrix.size(); ++i) {
        if (req.p_matrix[i].size() != n) {
            std::ostringstream oss;
            oss << "P row " << i << " has length " << req.p_matrix[i].size()
                << " but there are " << n << " assets";
            throw core::InvalidCommandError{oss.str()};
        }
        bool all_zero = true;
        for (double v : req.p_matrix[i]) {
            if (v != 0.0) {
                all_zero = false;
                break;
            }
        }
        if (all_zero) {
            std::ostringstream oss;
            oss << "P row " << i << " is all zeros";
            throw core::InvalidCommandError{oss.str()};
        }
    }
}

}  // namespace

BlackLittermanResult BlackLitterman(const BlackLittermanRequest& req) {
    ValidateBlackLittermanRequest(req);

    const std::size_t n = req.returns.size();
    const std::size_t obs = req.returns[0].size();
    const std::size_t k = req.p_matrix.size();

    std::vector<double> means(n);
    for (std::size_t i = 0; i < n; ++i) {
        means[i] = Mean(req.returns[i]);
    }
    std::vector<std::vector<double>> centered(n, std::vector<double>(obs));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t t = 0; t < obs; ++t) {
            centered[i][t] = req.returns[i][t] - means[i];
        }
    }
    auto cov = SampleCovariance(centered);
    for (std::size_t i = 0; i < n; ++i) {
        cov[i][i] += kCovRidge;
    }

    const double cap_sum = Sum(req.market_caps);
    std::vector<double> w_mkt(n);
    for (std::size_t i = 0; i < n; ++i) {
        w_mkt[i] = req.market_caps[i] / cap_sum;
    }

    // Pi = delta * Sigma * w_mkt.
    const auto sigma_w_mkt = VecDotMat(cov, w_mkt);
    const auto pi = Scale(sigma_w_mkt, req.risk_aversion);

    // Omega = diag(P * (tau * Sigma) * P').
    const auto tau_sigma = MatScale(cov, req.tau);
    std::vector<double> omega_inv(k);
    for (std::size_t i = 0; i < k; ++i) {
        const auto p_sigma = VecDotMat(tau_sigma, req.p_matrix[i]);
        const double omega_i = VecDot(req.p_matrix[i], p_sigma);
        if (std::abs(omega_i) < kDegeneracyEps) {
            std::ostringstream oss;
            oss << "view " << i << " has zero confidence";
            throw core::DataQualityError{oss.str()};
        }
        omega_inv[i] = 1.0 / omega_i;
    }

    // P' Omega^{-1} P and P' Omega^{-1} Q.
    auto pt_op = MatAlloc(n);
    std::vector<double> pt_oq(n, 0.0);
    for (std::size_t i = 0; i < k; ++i) {
        const double scale = omega_inv[i];
        const double q = req.q_vector[i];
        for (std::size_t a = 0; a < n; ++a) {
            for (std::size_t b = 0; b < n; ++b) {
                pt_op[a][b] += scale * req.p_matrix[i][a] * req.p_matrix[i][b];
            }
            pt_oq[a] += scale * q * req.p_matrix[i][a];
        }
    }

    // (tau * Sigma)^{-1}.
    const auto tau_sigma_inv = MatInverse(tau_sigma);

    // M1 = (tau Sigma)^{-1} + P' Omega^{-1} P.
    const auto m1 = MatAdd(tau_sigma_inv, pt_op);
    const auto m1_inv = MatInverse(m1);

    // M2 = (tau Sigma)^{-1} Pi + P' Omega^{-1} Q.
    const auto m2_part = VecDotMat(tau_sigma_inv, pi);
    std::vector<double> m2(n);
    for (std::size_t i = 0; i < n; ++i) {
        m2[i] = m2_part[i] + pt_oq[i];
    }

    const auto mu_bl = VecDotMat(m1_inv, m2);
    const auto sigma_bl = MatAdd(cov, m1_inv);

    // Mean-variance optimal weights against posterior estimates.
    const auto a = MatScale(sigma_bl, req.risk_aversion);
    const auto a_inv = MatInverse(a);
    const auto w_raw = VecDotMat(a_inv, mu_bl);
    const double w_sum = Sum(w_raw);
    if (std::abs(w_sum) < kDegeneracyEps) {
        throw core::InternalError{"optimised weights sum to zero"};
    }
    const auto w = Scale(w_raw, 1.0 / w_sum);

    BlackLittermanResult result;
    for (std::size_t i = 0; i < n; ++i) {
        result.portfolio.weights[req.labels[i]] = w[i];
        result.expected_returns[req.labels[i]] = mu_bl[i];
    }

    result.covariance = sigma_bl;

    double expected_return = 0.0, volatility = 0.0, sharpe = 0.0;
    PortfolioMetrics(w, mu_bl, sigma_bl, 0.0, expected_return, volatility, sharpe);
    result.portfolio.expected_return = expected_return;
    result.portfolio.volatility = volatility;
    result.portfolio.sharpe_ratio = sharpe;
    result.portfolio.Validate();

    return result;
}

BlackLittermanResult BlackLittermanSimplified(
    const BlackLittermanSimplifiedRequest& req) {
    if (req.labels.empty()) {
        throw core::DataQualityError{"labels must not be empty"};
    }
    if (req.views.empty()) {
        throw core::InvalidCommandError{"at least one expert view is required"};
    }
    if (req.tau <= 0.0 || !IsFinite(req.tau)) {
        throw core::InvalidCommandError{"tau must be a positive finite number"};
    }
    if (req.risk_aversion <= 0.0 || !IsFinite(req.risk_aversion)) {
        throw core::InvalidCommandError{"risk_aversion must be a positive finite number"};
    }

    std::vector<double> ordered_caps;
    ordered_caps.reserve(req.labels.size());
    for (const auto& label : req.labels) {
        const auto it = req.market_caps.find(label);
        if (it == req.market_caps.end()) {
            throw core::InvalidCommandError{"missing market cap for asset " + label};
        }
        if (it->second <= 0.0 || !IsFinite(it->second)) {
            throw core::InvalidCommandError{"market cap for " + label +
                                          " must be positive and finite"};
        }
        ordered_caps.push_back(it->second);
    }

    std::vector<std::vector<double>> p_rows;
    std::vector<double> q;
    p_rows.reserve(req.views.size());
    q.reserve(req.views.size());
    for (const auto& [label, expected_return] : req.views) {
        const auto it = std::find(req.labels.begin(), req.labels.end(), label);
        if (it == req.labels.end()) {
            throw core::InvalidCommandError{"unknown view asset " + label};
        }
        const std::size_t idx = static_cast<std::size_t>(it - req.labels.begin());
        std::vector<double> row(req.labels.size(), 0.0);
        row[idx] = 1.0;
        p_rows.push_back(std::move(row));
        q.push_back(expected_return);
    }

    BlackLittermanRequest full_req;
    full_req.returns = req.returns;
    full_req.labels = req.labels;
    full_req.market_caps = std::move(ordered_caps);
    full_req.p_matrix = std::move(p_rows);
    full_req.q_vector = std::move(q);
    full_req.tau = req.tau;
    full_req.risk_aversion = req.risk_aversion;
    return BlackLitterman(full_req);
}

}  // namespace standard_tools::portfolio
