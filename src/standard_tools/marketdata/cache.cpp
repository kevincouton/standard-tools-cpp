#include "standard_tools/marketdata/cache.hpp"

namespace standard_tools::marketdata {

std::optional<std::vector<core::OHLCV>> InMemoryCache::Get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void InMemoryCache::Put(const std::string& key, const std::vector<core::OHLCV>& series) {
    std::lock_guard<std::mutex> lock(mu_);
    data_[key] = series;
}

}  // namespace standard_tools::marketdata
