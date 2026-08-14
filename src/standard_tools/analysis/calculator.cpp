#include "standard_tools/analysis/calculator.hpp"

#include "standard_tools/core/errors.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>

namespace standard_tools::analysis {

namespace {

using core::DataQualityError;
using core::InvalidCommandError;

void ValidateFinite(const std::vector<double>& values, const std::string& context) {
    for (double v : values) {
        if (std::isnan(v) || std::isinf(v)) {
            std::ostringstream msg;
            msg << "input contains non-finite (NaN or infinite) values";
            if (!context.empty()) {
                msg << " in " << context;
            }
            throw DataQualityError{msg.str()};
        }
    }
}

void ValidateFinite(const std::vector<std::vector<double>>& matrix, const std::string& context) {
    for (std::size_t i = 0; i < matrix.size(); ++i) {
        ValidateFinite(matrix[i], context + " series " + std::to_string(i));
    }
}

double Mean(const std::vector<double>& values) {
    if (values.empty()) {
        return 0.0;
    }
    const double sum = std::accumulate(values.begin(), values.end(), 0.0);
    return sum / static_cast<double>(values.size());
}

double Slope(const std::vector<double>& xs, const std::vector<double>& ys) {
    if (xs.size() != ys.size() || xs.empty()) {
        return 0.0;
    }
    const double mean_x = Mean(xs);
    const double mean_y = Mean(ys);
    double num = 0.0;
    double den = 0.0;
    for (std::size_t i = 0; i < xs.size(); ++i) {
        const double dx = xs[i] - mean_x;
        num += dx * (ys[i] - mean_y);
        den += dx * dx;
    }
    if (den == 0.0) {
        return 0.0;
    }
    return num / den;
}

double NormalCdf(double x) {
    if (std::isinf(x)) {
        return x > 0.0 ? 1.0 : 0.0;
    }
    constexpr double b1 = 0.319381530;
    constexpr double b2 = -0.356563782;
    constexpr double b3 = 1.781477937;
    constexpr double b4 = -1.821255978;
    constexpr double b5 = 1.330274429;
    constexpr double p = 0.2316419;
    constexpr double c = 0.3989422804014327;  // 1 / sqrt(2*pi)

    if (x >= 0.0) {
        const double t = 1.0 / (1.0 + p * x);
        const double poly = t * (t * (t * (t * (t * b5 + b4) + b3) + b2) + b1);
        return 1.0 - c * std::exp(-x * x / 2.0) * poly;
    }
    return 1.0 - NormalCdf(-x);
}

double NormalPdf(double x) {
    constexpr double kPi = 3.14159265358979323846;
    return std::exp(-x * x / 2.0) / std::sqrt(2.0 * kPi);
}

double AR1Coefficient(const std::vector<double>& values) {
    if (values.size() < 2) {
        return 0.0;
    }
    double num = 0.0;
    double den = 0.0;
    for (std::size_t i = 1; i < values.size(); ++i) {
        num += values[i] * values[i - 1];
        den += values[i - 1] * values[i - 1];
    }
    if (den == 0.0) {
        return 0.0;
    }
    return num / den;
}

double AdfStatistic(const std::vector<double>& values) {
    if (values.size() < 3) {
        return 0.0;
    }
    double num = 0.0;
    double den = 0.0;
    for (std::size_t i = 1; i < values.size(); ++i) {
        const double y_prev = values[i - 1];
        num += (values[i] - y_prev) * y_prev;
        den += y_prev * y_prev;
    }
    if (den == 0.0) {
        return 0.0;
    }
    const double gamma = num / den;

    double ss_res = 0.0;
    for (std::size_t i = 1; i < values.size(); ++i) {
        const double y_prev = values[i - 1];
        const double dy = values[i] - y_prev;
        const double eps = dy - gamma * y_prev;
        ss_res += eps * eps;
    }
    const double df = static_cast<double>(values.size() - 2);
    const double mse = ss_res / df;
    const double se = std::sqrt(mse / den);
    if (se == 0.0) {
        return 0.0;
    }
    return gamma / se;
}

double StandardScore(double value, const std::vector<double>& values) {
    const double m = Mean(values);
    double sum = 0.0;
    for (double v : values) {
        const double d = v - m;
        sum += d * d;
    }
    const double std = std::sqrt(sum / static_cast<double>(values.size()));
    if (std == 0.0) {
        return 0.0;
    }
    return (value - m) / std;
}

double RescaledRange(const std::vector<double>& series, int lag) {
    const int chunks = static_cast<int>(series.size()) / lag;
    if (chunks == 0) {
        return 0.0;
    }

    double total = 0.0;
    int count = 0;
    for (int c = 0; c < chunks; ++c) {
        const std::size_t start = static_cast<std::size_t>(c * lag);
        const std::size_t end = start + static_cast<std::size_t>(lag);
        const std::vector<double> chunk(series.begin() + start, series.begin() + end);

        const double chunk_mean = Mean(chunk);
        double cumulative = 0.0;
        double max_dev = std::numeric_limits<double>::lowest();
        double min_dev = std::numeric_limits<double>::max();
        for (double r : chunk) {
            cumulative += r - chunk_mean;
            if (cumulative > max_dev) {
                max_dev = cumulative;
            }
            if (cumulative < min_dev) {
                min_dev = cumulative;
            }
        }
        const double range_val = max_dev - min_dev;
        double variance = 0.0;
        for (double r : chunk) {
            const double d = r - chunk_mean;
            variance += d * d;
        }
        variance /= static_cast<double>(chunk.size());
        const double std = std::sqrt(variance);
        if (std > 0.0) {
            total += range_val / std;
            ++count;
        }
    }

    if (count == 0) {
        return 0.0;
    }
    return total / static_cast<double>(count);
}

std::string InterpretHurst(double exponent) {
    if (exponent < 0.4) {
        return "mean-reverting";
    }
    if (exponent < 0.55) {
        return "approximately random walk";
    }
    return "persistent/trending";
}

}  // namespace

Result AnalysisCalculator::Calculate(const Request& request) const {
    if (request.operation == operation::kRegression) {
        return Regression(request);
    }
    if (request.operation == operation::kCointegration) {
        return Cointegration(request);
    }
    if (request.operation == operation::kHurst) {
        return Hurst(request);
    }
    if (request.operation == operation::kPca) {
        return Pca(request);
    }
    if (request.operation == operation::kCorrelation) {
        return Correlation(request);
    }
    if (request.operation == operation::kOptions) {
        return Options(request);
    }
    throw InvalidCommandError{"unknown analysis operation " + request.operation};
}

RegressionResult AnalysisCalculator::Regression(const Request& request) const {
    const auto& asset_returns = request.asset_returns;
    const auto& benchmark_returns = request.benchmark_returns;
    ValidateFinite(asset_returns, "asset returns");
    ValidateFinite(benchmark_returns, "benchmark returns");
    if (asset_returns.size() != benchmark_returns.size()) {
        throw DataQualityError{
            "asset and benchmark returns must have the same length (got " +
            std::to_string(asset_returns.size()) + " and " +
            std::to_string(benchmark_returns.size()) + ")"};
    }
    const auto n = asset_returns.size();
    if (n < 2) {
        throw DataQualityError{"regression requires at least two observations"};
    }

    const double mean_x = Mean(benchmark_returns);
    const double mean_y = Mean(asset_returns);

    double ss_xx = 0.0;
    double ss_xy = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double dx = benchmark_returns[i] - mean_x;
        ss_xx += dx * dx;
        ss_xy += dx * (asset_returns[i] - mean_y);
    }

    if (ss_xx == 0.0) {
        throw DataQualityError{"benchmark returns have zero variance"};
    }

    const double beta = ss_xy / ss_xx;
    const double alpha = mean_y - beta * mean_x;

    RegressionResult result;
    result.alpha = alpha;
    result.beta = beta;
    result.residuals.resize(n);
    double ss_res = 0.0;
    double ss_tot = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double fitted = alpha + beta * benchmark_returns[i];
        const double resid = asset_returns[i] - fitted;
        result.residuals[i] = resid;
        ss_res += resid * resid;
        ss_tot += (asset_returns[i] - mean_y) * (asset_returns[i] - mean_y);
    }

    result.r_squared = 0.0;
    if (ss_tot != 0.0) {
        result.r_squared = 1.0 - ss_res / ss_tot;
    }

    return result;
}

CointegrationResult AnalysisCalculator::Cointegration(const Request& request) const {
    const auto& a_closes = request.a_closes;
    const auto& b_closes = request.b_closes;
    ValidateFinite(a_closes, "a closes");
    ValidateFinite(b_closes, "b closes");
    if (a_closes.size() != b_closes.size()) {
        throw DataQualityError{
            "price series must have the same length (got " +
            std::to_string(a_closes.size()) + " and " +
            std::to_string(b_closes.size()) + ")"};
    }
    const auto n = a_closes.size();
    if (n < 10) {
        throw DataQualityError{"cointegration test requires at least ten observations"};
    }

    const double mean_a = Mean(a_closes);
    const double mean_b = Mean(b_closes);

    double ss_bb = 0.0;
    double ss_ab = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double db = b_closes[i] - mean_b;
        ss_bb += db * db;
        ss_ab += (a_closes[i] - mean_a) * db;
    }

    if (ss_bb == 0.0) {
        throw DataQualityError{"benchmark series has zero variance"};
    }

    const double hedge_ratio = ss_ab / ss_bb;
    const double alpha = mean_a - hedge_ratio * mean_b;

    std::vector<double> residuals(n);
    for (std::size_t i = 0; i < n; ++i) {
        residuals[i] = a_closes[i] - (alpha + hedge_ratio * b_closes[i]);
    }

    const double phi = AR1Coefficient(residuals);
    double half_life = std::numeric_limits<double>::infinity();
    if (phi > 0.0 && phi < 1.0) {
        half_life = -std::log(2.0) / std::log(phi);
    }

    CointegrationResult result;
    result.hedge_ratio = hedge_ratio;
    result.adf_statistic = AdfStatistic(residuals);
    result.half_life = half_life;
    result.current_z_score = StandardScore(residuals.back(), residuals);
    return result;
}

HurstResult AnalysisCalculator::Hurst(const Request& request) const {
    const auto& prices = request.prices;
    ValidateFinite(prices, "prices");
    if (prices.size() < 50) {
        throw DataQualityError{"Hurst estimation requires at least 50 price observations"};
    }
    for (double p : prices) {
        if (p <= 0.0) {
            throw DataQualityError{"prices must be positive to compute log-returns"};
        }
    }

    std::vector<double> returns(prices.size() - 1);
    for (std::size_t i = 1; i < prices.size(); ++i) {
        returns[i - 1] = std::log(prices[i] / prices[i - 1]);
    }
    const auto n = returns.size();

    int max_l = static_cast<int>(n / 4);
    if (max_l < 8) {
        max_l = 8;
    }
    if (request.max_lag.has_value()) {
        int m = request.max_lag.value();
        if (m < 8) {
            m = 8;
        }
        if (m > static_cast<int>(n / 4)) {
            m = static_cast<int>(n / 4);
        }
        max_l = m;
    }

    if (max_l > static_cast<int>(n) - 1) {
        max_l = static_cast<int>(n) - 1;
    }
    constexpr int min_lag = 8;
    if (max_l <= min_lag) {
        throw DataQualityError{"series is too short for meaningful R/S analysis"};
    }

    int step = (max_l - min_lag) / 50;
    if (step < 1) {
        step = 1;
    }

    std::vector<double> log_lags;
    std::vector<double> log_rs;
    for (int lag = min_lag; lag <= max_l; lag += step) {
        const double rs = RescaledRange(returns, lag);
        if (rs > 0.0) {
            log_lags.push_back(std::log(static_cast<double>(lag)));
            log_rs.push_back(std::log(rs));
        }
    }

    if (log_lags.size() < 2) {
        throw DataQualityError{"could not compute rescaled range for any lag"};
    }

    double exponent = Slope(log_lags, log_rs);
    if (exponent < 0.0) {
        exponent = 0.0;
    }
    if (exponent > 1.0) {
        exponent = 1.0;
    }

    HurstResult result;
    result.exponent = exponent;
    result.interpretation = InterpretHurst(exponent);
    return result;
}

PCAResult AnalysisCalculator::Pca(const Request& request) const {
    const auto& returns_matrix = request.returns_matrix;
    const int n_components = request.n_components;
    if (returns_matrix.empty()) {
        throw DataQualityError{"returns matrix must contain at least one series"};
    }
    if (n_components <= 0 || static_cast<std::size_t>(n_components) > returns_matrix.size()) {
        std::ostringstream msg;
        msg << "n_components must be in [1, " << returns_matrix.size() << "], got " << n_components;
        throw DataQualityError{msg.str()};
    }

    const auto n_obs = returns_matrix[0].size();
    if (n_obs < 2) {
        throw DataQualityError{"each return series must contain at least two observations"};
    }
    ValidateFinite(returns_matrix, "returns matrix");
    for (std::size_t i = 0; i < returns_matrix.size(); ++i) {
        if (returns_matrix[i].size() != n_obs) {
            std::ostringstream msg;
            msg << "series " << i << " has length " << returns_matrix[i].size()
                << " but series 0 has length " << n_obs;
            throw DataQualityError{msg.str()};
        }
    }

    const auto n_vars = returns_matrix.size();
    std::vector<std::vector<double>> centered(n_vars);
    std::vector<double> variances(n_vars);
    for (std::size_t i = 0; i < n_vars; ++i) {
        const double m = Mean(returns_matrix[i]);
        centered[i].resize(n_obs);
        double sum = 0.0;
        for (std::size_t t = 0; t < n_obs; ++t) {
            const double d = returns_matrix[i][t] - m;
            centered[i][t] = d;
            sum += d * d;
        }
        variances[i] = sum / static_cast<double>(n_obs - 1);
    }

    std::vector<std::size_t> indices(n_vars);
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](std::size_t a, std::size_t b) {
        return variances[a] > variances[b];
    });

    double total_variance = 0.0;
    for (double v : variances) {
        total_variance += v;
    }

    PCAResult result;
    result.labels.resize(n_vars);
    result.explained_variance_ratio.resize(static_cast<std::size_t>(n_components));
    result.loadings.resize(static_cast<std::size_t>(n_components));
    result.factor_returns.resize(static_cast<std::size_t>(n_components));

    for (std::size_t i = 0; i < n_vars; ++i) {
        result.labels[i] = "PC" + std::to_string(i + 1);
    }

    for (int k = 0; k < n_components; ++k) {
        const std::size_t idx = indices[static_cast<std::size_t>(k)];
        if (total_variance > 0.0) {
            result.explained_variance_ratio[static_cast<std::size_t>(k)] = variances[idx] / total_variance;
        }
        std::vector<double> loading(n_vars, 0.0);
        loading[idx] = 1.0;
        result.loadings[static_cast<std::size_t>(k)] = std::move(loading);
        result.factor_returns[static_cast<std::size_t>(k)] = centered[idx];
    }

    return result;
}

CorrelationResult AnalysisCalculator::Correlation(const Request& request) const {
    const auto& returns_map = request.returns_map;
    if (returns_map.empty()) {
        throw DataQualityError{"returns map must contain at least one series"};
    }

    std::vector<std::string> labels;
    labels.reserve(returns_map.size());
    for (const auto& [label, _] : returns_map) {
        labels.push_back(label);
    }
    std::sort(labels.begin(), labels.end());

    const auto n = returns_map.at(labels[0]).size();
    if (n < 2) {
        throw DataQualityError{"each return series must contain at least two observations"};
    }
    for (const auto& label : labels) {
        ValidateFinite(returns_map.at(label), "series " + label);
        if (returns_map.at(label).size() != n) {
            throw DataQualityError{
                "series " + label + " has length " +
                std::to_string(returns_map.at(label).size()) + " but expected " +
                std::to_string(n)};
        }
    }

    const auto k = labels.size();
    std::vector<double> means(k);
    for (std::size_t i = 0; i < k; ++i) {
        means[i] = Mean(returns_map.at(labels[i]));
    }

    std::vector<std::vector<double>> cov(k, std::vector<double>(k, 0.0));
    std::vector<double> variances(k, 0.0);

    for (std::size_t i = 0; i < k; ++i) {
        for (std::size_t j = i; j < k; ++j) {
            double acc = 0.0;
            for (std::size_t t = 0; t < n; ++t) {
                acc += (returns_map.at(labels[i])[t] - means[i]) *
                       (returns_map.at(labels[j])[t] - means[j]);
            }
            const double value = acc / static_cast<double>(n - 1);
            cov[i][j] = value;
            if (i == j) {
                variances[i] = value;
            } else {
                cov[j][i] = value;
            }
        }
    }

    CorrelationResult result;
    result.labels = labels;
    result.matrix.resize(k, std::vector<double>(k, 0.0));
    for (std::size_t i = 0; i < k; ++i) {
        for (std::size_t j = 0; j < k; ++j) {
            const double denom = std::sqrt(variances[i] * variances[j]);
            if (denom == 0.0) {
                throw DataQualityError{
                    "series " + labels[i] + " and/or " + labels[j] + " have zero variance"};
            }
            result.matrix[i][j] = cov[i][j] / denom;
        }
    }

    double sum = 0.0;
    double min_val = std::numeric_limits<double>::infinity();
    double max_val = -std::numeric_limits<double>::infinity();
    std::size_t count = 0;
    for (std::size_t i = 0; i < k; ++i) {
        for (std::size_t j = i + 1; j < k; ++j) {
            const double v = result.matrix[i][j];
            sum += v;
            ++count;
            if (v < min_val) {
                min_val = v;
            }
            if (v > max_val) {
                max_val = v;
            }
        }
    }

    result.average = 0.0;
    if (count > 0) {
        result.average = sum / static_cast<double>(count);
    }
    result.min = min_val;
    result.max = max_val;

    return result;
}

OptionPricingResult AnalysisCalculator::Options(const Request& request) const {
    if (!request.black_scholes.has_value()) {
        throw DataQualityError{"BlackScholes params are required for operation options"};
    }
    const auto& p = request.black_scholes.value();

    const std::vector<double> inputs = {
        p.spot, p.strike, p.risk_free_rate, p.volatility, p.time_to_maturity};
    ValidateFinite(inputs, "Black-Scholes inputs");

    if (p.spot < 0.0) {
        throw DataQualityError{"spot must be a non-negative finite number"};
    }
    if (p.strike < 0.0) {
        throw DataQualityError{"strike must be a non-negative finite number"};
    }
    if (p.risk_free_rate < 0.0) {
        throw DataQualityError{"risk_free_rate must be a non-negative finite number"};
    }
    if (p.volatility < 0.0) {
        throw DataQualityError{"volatility must be a non-negative finite number"};
    }
    if (p.time_to_maturity < 0.0) {
        throw DataQualityError{"time_to_maturity must be a non-negative finite number"};
    }
    if (p.time_to_maturity == 0.0) {
        throw DataQualityError{"time_to_maturity must be positive"};
    }
    if (p.volatility == 0.0) {
        throw DataQualityError{"volatility must be positive"};
    }

    const double sqrt_t = std::sqrt(p.time_to_maturity);
    const double d1 =
        (std::log(p.spot / p.strike) +
         (p.risk_free_rate + 0.5 * p.volatility * p.volatility) * p.time_to_maturity) /
        (p.volatility * sqrt_t);
    const double d2 = d1 - p.volatility * sqrt_t;

    const double nd1 = NormalCdf(d1);
    const double nd2 = NormalCdf(d2);
    const double n_neg_d1 = NormalCdf(-d1);
    const double n_neg_d2 = NormalCdf(-d2);
    const double pdf_d1 = NormalPdf(d1);

    const double discount = std::exp(-p.risk_free_rate * p.time_to_maturity);

    OptionPricingResult result;
    switch (p.option_type) {
        case OptionType::Call:
            result.price = p.spot * nd1 - p.strike * discount * nd2;
            result.delta = nd1;
            result.theta =
                -p.spot * pdf_d1 * p.volatility / (2.0 * sqrt_t) -
                p.risk_free_rate * p.strike * discount * nd2;
            result.rho = p.strike * p.time_to_maturity * discount * nd2;
            break;
        case OptionType::Put:
            result.price = p.strike * discount * n_neg_d2 - p.spot * n_neg_d1;
            result.delta = nd1 - 1.0;
            result.theta =
                -p.spot * pdf_d1 * p.volatility / (2.0 * sqrt_t) +
                p.risk_free_rate * p.strike * discount * n_neg_d2;
            result.rho = -p.strike * p.time_to_maturity * discount * n_neg_d2;
            break;
    }

    result.gamma = pdf_d1 / (p.spot * p.volatility * sqrt_t);
    result.vega = p.spot * pdf_d1 * sqrt_t;

    return result;
}

}  // namespace standard_tools::analysis
