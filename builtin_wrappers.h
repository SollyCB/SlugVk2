#pragma once
#include "typedef.h"

// builtin wrappers (why the fuck do they differ between compilers!!! the world is retarded)
#ifndef _WIN32
inline int count_trailing_zeros_u16(u16 num) {
    return int(__builtin_ctz(num));
}

inline int count_trailing_zeros_u32(u32 num) {
    return __builtin_ctzl(num);
}
inline int count_leading_zeros_u16(u16 mask) {
    return __builtin_clzs(mask);
}
inline int count_leading_zeros_u32(u32 mask) {
    return __builtin_clzl(mask);
}
inline int pop_count16(u16 num) {
    return (int)__builtin_popcount(num);
}
inline int pop_count32(u32 num) {
    return (int)__builtin_popcount(num);
}
inline int pop_count64(u64 num) {
    return (int)__builtin_popcount(num);
}
#else
inline int count_trailing_zeros_u16(u16 num) {
    unsigned long tz;
    // Who needs the return value??
    _BitScanForward(&tz, num);
    return tz;
}
inline int ctzl(unsigned long num) {
    unsigned long tz;
    // Who needs the return value??
    _BitScanForward(&tz, num);
    return tz;
}
inline int count_leading_zeros_u16(u16 mask) {
    return __lzcnt16(mask);
}
inline int count_leading_zeros_u32(u32 mask) {
    return __lzcnt(mask);
}
inline int pop_count16(u16 num) {
    return (int)__popcnt16(num);
}
inline int pop_count32(u32 num) {
    return (int)__popcnt(num);
}
inline int pop_count64(u64 num) {
    return (int)__popcnt64(num);
}
#endif
