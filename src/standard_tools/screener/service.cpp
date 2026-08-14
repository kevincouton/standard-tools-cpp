#include "standard_tools/screener/service.hpp"

#include "standard_tools/core/errors.hpp"

namespace standard_tools::screener {

Screener::Screener(FundamentalProviderPtr provider) : provider_(std::move(provider)) {}

ScreenResult Screener::Screen(const std::vector<std::string>& tickers,
                              const ScreenCriteria& criteria) const {
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
