#pragma once

#include <stdexcept>
#include <string>

namespace fontmaker {

class Error : public std::runtime_error {
public:
    explicit Error(const std::string& message) : std::runtime_error(message) {}
};

class CharsetError : public Error {
public:
    explicit CharsetError(const std::string& message) : Error(message) {}
};

class EncodeError : public Error {
public:
    explicit EncodeError(const std::string& message) : Error(message) {}
};

} // namespace fontmaker
