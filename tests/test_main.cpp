#include "test_framework.hpp"

#include <exception>
#include <iostream>

int main()
{
    int failures = 0;
    for (const auto& test : fontmaker::test::registry()) {
        try {
            test.function();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        } catch (...) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
        }
    }

    std::cout << (failures == 0 ? "All tests passed.\n" : "Tests failed.\n");
    return failures == 0 ? 0 : 1;
}
