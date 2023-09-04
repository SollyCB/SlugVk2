#ifndef SOL_ASSERT_H_INCLUDE_GUARD_
#define SOL_ASSERT_H_INCLUDE_GUARD_

#include <iostream>
#include "print.h"

#ifndef _WIN32
#define HALT_EXECUTION() __builtin_trap()
#else
#define HALT_EXECUTION() abort()
#endif

#if DEBUG 
#define ASSERT(predicate, fmt, ...) if (!(predicate)) { \
    std::cout << "ASSERT FAILED IN " << __FILE__ << ", " << __LINE__ << ": " << #predicate << '\n'; \
    println(fmt, __VA_ARGS__); \
    HALT_EXECUTION(); \
}
#else

#define ASSERT(predicate, fmt, ...) {}

#endif

#endif
