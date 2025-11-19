#pragma once

#include <cstdio>
#include <exception>
#include <vector>

namespace doctest {
namespace detail {
struct TestCase {
    void (*func)();
    const char *name;
};

inline std::vector<TestCase> &registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

struct TestRegistrar {
    TestRegistrar(void (*func)(), const char *name)
    {
        registry().push_back(TestCase{func, name});
    }
};

struct TestFailure : public std::exception {
    const char *what() const noexcept override { return "doctest requirement failed"; }
};

inline int &failure_count()
{
    static int count = 0;
    return count;
}

inline void report(bool condition, const char *expr, const char *file, int line, bool require)
{
    if (!condition) {
        std::fprintf(stderr,
                     "%s:%d: %s(%s) failed\n",
                     file,
                     line,
                     require ? "REQUIRE" : "CHECK",
                     expr);
        ++failure_count();
        if (require) {
            throw TestFailure();
        }
    }
}

inline int run_tests()
{
    int failed_cases = 0;
    for (const auto &test : registry()) {
        int  before_failures = failure_count();
        bool aborted         = false;
        try {
            test.func();
        } catch (const TestFailure &) {
            aborted = true;
        } catch (const std::exception &ex) {
            std::fprintf(stderr, "%s: unexpected exception: %s\n", test.name, ex.what());
            aborted = true;
        } catch (...) {
            std::fprintf(stderr, "%s: unknown exception\n", test.name);
            aborted = true;
        }
        if (failure_count() > before_failures || aborted) {
            ++failed_cases;
        }
    }
    return failed_cases;
}

}  // namespace detail
}  // namespace doctest

#define DOCTEST_CONCAT_IMPL(x, y) x##y
#define DOCTEST_CONCAT(x, y) DOCTEST_CONCAT_IMPL(x, y)

#define TEST_CASE(name)                                                                                                 \
    static void DOCTEST_CONCAT(doctest_test_func_, __LINE__)();                                                         \
    static ::doctest::detail::TestRegistrar                                                                              \
        DOCTEST_CONCAT(doctest_registrar_, __LINE__)(DOCTEST_CONCAT(doctest_test_func_, __LINE__), name);               \
    static void DOCTEST_CONCAT(doctest_test_func_, __LINE__)()

#define CHECK(expr) ::doctest::detail::report(static_cast<bool>(expr), #expr, __FILE__, __LINE__, false)
#define REQUIRE(expr) ::doctest::detail::report(static_cast<bool>(expr), #expr, __FILE__, __LINE__, true)

#ifdef DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
int main()
{
    return ::doctest::detail::run_tests() == 0 ? 0 : 1;
}
#endif
