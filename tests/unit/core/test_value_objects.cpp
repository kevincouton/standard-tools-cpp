#include "standard_tools/core/errors.hpp"
#include "standard_tools/core/value_objects.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace standard_tools::core;

TEST_CASE("Ticker rejects empty symbols", "[core]") {
    REQUIRE_THROWS_AS(Ticker(""), InvalidTickerError);
    REQUIRE_THROWS_AS(Ticker("   "), InvalidTickerError);
}

TEST_CASE("Ticker trims and stores symbol", "[core]") {
    Ticker t("  AAPL  ");
    REQUIRE(t.Symbol() == "AAPL");
}

TEST_CASE("ParseBarInterval accepts valid values", "[core]") {
    REQUIRE(ToString(ParseBarInterval("daily")) == "daily");
    REQUIRE(ToString(ParseBarInterval("weekly")) == "weekly");
    REQUIRE(ToString(ParseBarInterval("monthly")) == "monthly");
    REQUIRE(ToString(ParseBarInterval("")) == "daily");
}

TEST_CASE("ParseBarInterval rejects invalid values", "[core]") {
    REQUIRE_THROWS_AS(ParseBarInterval("hourly"), InvalidCommandError);
}

TEST_CASE("DateRange rejects inverted ranges", "[core]") {
    auto start = MakeDate(2024, 1, 10);
    auto end = MakeDate(2024, 1, 1);
    REQUIRE_THROWS_AS(DateRange(start, end), InvalidDateRangeError);
}

TEST_CASE("ParseDate and FormatDate round-trip", "[core]") {
    auto d = MakeDate(2024, 6, 15);
    REQUIRE(FormatDate(d) == "2024-06-15");
    REQUIRE(ParseDate("2024-06-15") == d);
}
