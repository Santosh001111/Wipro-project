#pragma once
// Minimal test helper. Unlike assert(), CHECK is NOT removed by -DNDEBUG
// (CMAKE_BUILD_TYPE=Release), so tests can never silently pass.
#include <cstdio>
#include <cstdlib>

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::fprintf(stderr, "CHECK FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            std::exit(1);                                                             \
        }                                                                             \
    } while (0)
