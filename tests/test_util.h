#pragma once
#include <cstdio>

inline int g_fsic_test_failures = 0;

#define FSIC_CHECK(cond) \
    do { if (!(cond)) { std::fprintf(stderr, "%s:%d: CHECK FAILED: %s\n", __FILE__, __LINE__, #cond); ++g_fsic_test_failures; } } while (0)

#define FSIC_CHECK_EQ(a, b) \
    do { if (!((a) == (b))) { std::fprintf(stderr, "%s:%d: CHECK_EQ FAILED: %s == %s\n", __FILE__, __LINE__, #a, #b); ++g_fsic_test_failures; } } while (0)

#define FSIC_CHECK_NEAR(a, b, eps) \
    do { double _d = (double)(a) - (double)(b); if (_d < 0) _d = -_d; if (_d > (eps)) { std::fprintf(stderr, "%s:%d: CHECK_NEAR FAILED: %s ~= %s (eps=%s)\n", __FILE__, __LINE__, #a, #b, #eps); ++g_fsic_test_failures; } } while (0)

#define FSIC_TEST_MAIN_END return g_fsic_test_failures ? 1 : 0;
