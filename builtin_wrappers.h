#pragma once
#include <cstdint>

// builtin wrappers (why the fuck do they differ between compilers!!! the world is retarded)
#ifndef _WIN32
inline int builtin_ctzl(unsigned short num) {
    return static_cast<uint32_t>(__builtin_ctzl(num));
}
inline int builtin_ctzl(unsigned long num) {
    return static_cast<uint32_t>(__builtin_ctzl(num));
}
#else
inline int builtin_ctzl(unsigned short num) {
    unsigned long tz;
    // Who needs the return value??
    _BitScanForward(&tz, num);
    return tz;
}
inline int builtin_ctzl(unsigned long num) {
    unsigned long tz;
    // Who needs the return value??
    _BitScanForward(&tz, num);
    return tz;
}
#endif
