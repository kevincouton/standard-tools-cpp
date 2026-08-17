#include "standard_tools/screener/service.hpp"

#include "standard_tools/core/errors.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace standard_tools::screener {

namespace {

constexpr std::size_t kMaxScreenTickers = 500;

}  // namespace

Screener::Screener(FundamentalProviderPtr provider) : provider_(std::move(provider)) {}

ScreenResult Screener::Screen(const std::vector<std::string>& tickers,
                              const ScreenCriteria& criteria) const {
    if (tickers.size() > kMaxScreenTickers) {
        std::ostringstream oss;
        oss << "ticker count " << tickers.size() << " exceeds maximum of "
            << kMaxScreenTickers;
        throw core::InvalidCommandError{oss.str()};
    }
    ScreenResult result;
    for (const auto& ticker : tickers) {
        try {
            auto data = provider_->Fetch(ticker);
            if (criteria.Apply(data)) {
                result.matches.push_back(std::move(data));
            } else {
                result.failed.push_back(ticker);
            }
        } catch (const core::NotFoundError&) {
            result.failed.push_back(ticker);
        }
    }
    return result;
}

}  // namespace standard_tools::screener
