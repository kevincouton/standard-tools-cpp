#pragma once

#include "standard_tools/core/value_objects.hpp"

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace standard_tools::marketdata {

class Cache {
public:
    virtual ~Cache() = default;

    virtual std::optional<std::vector<core::OHLCV>> Get(const std::string& key) = 0;
    virtual void Put(const std::string& key, const std::vector<core::OHLCV>& series) = 0;
};

class InMemoryCache : public Cache {
public:
    std::optional<std::vector<core::OHLCV>> Get(const std::string& key) override;
    void Put(const std::string& key, const std::vector<core::OHLCV>& series) override;

private:
    std::mutex mu_;
    std::map<std::string, std::vector<core::OHLCV>> data_;
};

}  // namespace standard_tools::marketdata
