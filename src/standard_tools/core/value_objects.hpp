#pragma once

#include "standard_tools/core/errors.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace standard_tools::core {

using Date = std::chrono::system_clock::time_point;

Date MakeDate(int year, int month, int day);

class Ticker {
public:
    explicit Ticker(std::string symbol);

    const std::string& Symbol() const noexcept { return symbol_; }
    const std::string& Exchange() const noexcept { return exchange_; }
    void SetExchange(std::string exchange) { exchange_ = std::move(exchange); }

private:
    std::string symbol_;
    std::string exchange_;
};

enum class BarInterval {
    Daily,
    Weekly,
    Monthly,
};

std::string ToString(BarInterval interval);
BarInterval ParseBarInterval(const std::string& s);

class DateRange {
public:
    DateRange(Date start, Date end);

    const Date& Start() const noexcept { return start_; }
    const Date& End() const noexcept { return end_; }

private:
    Date start_;
    Date end_;
};

struct OHLCV {
    Date date;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    std::int64_t volume = 0;
};

struct TickerInfo {
    std::string symbol;
    std::string name;
    std::string sector;
    std::string industry;
    std::int64_t employees = 0;
    std::string city;
    std::string country;
    std::string website;
};

struct FinancialRatios {
    std::string symbol;
    std::string forward_pe;
    std::string trailing_pe;
    std::string price_to_book;
    std::string debt_to_equity;
    std::string roe;
    std::string profit_margins;
    std::string dividend_yield;
    std::int64_t market_cap = 0;
};

struct DataSetMetadata {
    std::string provider;
    bool adjusted = false;
    bool survivorship_free = false;
    bool point_in_time = false;
    std::string frequency;
    std::string timezone;
    Date retrieved_at;
};

// Date formatting / parsing using ISO-8601 date (YYYY-MM-DD).
std::string FormatDate(Date d);
Date ParseDate(const std::string& s);

}  // namespace standard_tools::core
