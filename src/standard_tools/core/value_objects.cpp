#include "standard_tools/core/value_objects.hpp"

#include <date/date.h>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace standard_tools::core {

namespace {

std::string Trim(const std::string& s) {
    auto start = std::find_if_not(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
    auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) { return std::isspace(c); }).base();
    if (start >= end) return "";
    return std::string(start, end);
}

}  // namespace

Ticker::Ticker(std::string symbol) {
    symbol_ = Trim(symbol);
    if (symbol_.empty()) {
        throw InvalidTickerError{};
    }
}

std::string ToString(BarInterval interval) {
    switch (interval) {
        case BarInterval::Daily:
            return "daily";
        case BarInterval::Weekly:
            return "weekly";
        case BarInterval::Monthly:
            return "monthly";
    }
    return "daily";
}

BarInterval ParseBarInterval(const std::string& s) {
    std::string lower;
    for (unsigned char c : Trim(s)) {
        lower.push_back(static_cast<char>(std::tolower(c)));
    }
    if (lower.empty() || lower == "daily") return BarInterval::Daily;
    if (lower == "weekly") return BarInterval::Weekly;
    if (lower == "monthly") return BarInterval::Monthly;
    throw InvalidCommandError{"invalid interval " + s + " (want daily, weekly, or monthly)"};
}

DateRange::DateRange(Date start, Date end) : start_(start), end_(end) {
    if (end_ < start_) {
        throw InvalidDateRangeError{};
    }
}

std::string FormatDate(Date d) {
    return date::format("%F", d);
}

Date ParseDate(const std::string& s) {
    std::istringstream in(s);
    Date d;
    in >> date::parse("%F", d);
    if (in.fail()) {
        throw InvalidCommandError{"invalid date " + s};
    }
    return d;
}

Date MakeDate(int year, int month, int day) {
    return date::sys_days{
        date::year_month_day{
            date::year{year},
            date::month{static_cast<unsigned>(month)},
            date::day{static_cast<unsigned>(day)}}};
}

}  // namespace standard_tools::core
