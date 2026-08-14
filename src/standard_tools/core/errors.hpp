#pragma once

#include <stdexcept>
#include <string>

namespace standard_tools::core {

class Error : public std::runtime_error {
public:
    explicit Error(const std::string& msg) : std::runtime_error(msg) {}
    explicit Error(const char* msg) : std::runtime_error(msg) {}
};

class InvalidTickerError : public Error {
public:
    InvalidTickerError() : Error("invalid ticker") {}
};

class InvalidDateRangeError : public Error {
public:
    InvalidDateRangeError() : Error("invalid date range") {}
};

class InvalidCommandError : public Error {
public:
    explicit InvalidCommandError(const std::string& msg) : Error("invalid command: " + msg) {}
};

class NotFoundError : public Error {
public:
    NotFoundError() : Error("not found") {}
};

class DataQualityError : public Error {
public:
    explicit DataQualityError(const std::string& msg) : Error("data quality: " + msg) {}
};

class ProviderNotAvailableError : public Error {
public:
    explicit ProviderNotAvailableError(const std::string& name)
        : Error("provider not available: " + name) {}
};

class InternalError : public Error {
public:
    explicit InternalError(const std::string& msg) : Error("internal error: " + msg) {}
};

class InsufficientDataError : public Error {
public:
    explicit InsufficientDataError(const std::string& msg) : Error("insufficient data: " + msg) {}
};

class InvalidPricesError : public Error {
public:
    explicit InvalidPricesError(const std::string& msg) : Error("invalid prices: " + msg) {}
};

}  // namespace standard_tools::core
