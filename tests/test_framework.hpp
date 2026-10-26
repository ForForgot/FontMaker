#pragma once

#include <exception>
#include <sstream>
#include <string>
#include <vector>

namespace fontmaker::test {

struct Case {
    const char* name;
    void (*function)();
};

inline std::vector<Case>& registry()
{
    static std::vector<Case> cases;
    return cases;
}

class Registrar {
public:
    Registrar(const char* name, void (*function)())
    {
        registry().push_back({name, function});
    }
};

[[noreturn]] inline void fail(const char* expression, const char* file, int line, const std::string& detail)
{
    std::ostringstream stream;
    stream << file << ':' << line << ": check failed: " << expression;
    if (!detail.empty()) {
        stream << " (" << detail << ')';
    }
    throw std::runtime_error(stream.str());
}

} // namespace fontmaker::test

#define FM_TEST(name) \
    static void name(); \
    static ::fontmaker::test::Registrar name##_registrar(#name, &name); \
    static void name()

#define FM_CHECK(condition) \
    do { \
        if (!(condition)) { \
            ::fontmaker::test::fail(#condition, __FILE__, __LINE__, {}); \
        } \
    } while (false)

#define FM_CHECK_EQ(actual, expected) \
    do { \
        const auto& fm_actual = (actual); \
        const auto& fm_expected = (expected); \
        if (!(fm_actual == fm_expected)) { \
            ::fontmaker::test::fail(#actual " == " #expected, __FILE__, __LINE__, {}); \
        } \
    } while (false)
