#ifndef SOL_SIMD_HPP_INCLUDE_GUARD_
#define SOL_SIMD_HPP_INCLUDE_GUARD_

#include <nmmintrin.h>
#include <emmintrin.h>
#include "basic.h"
#include "builtin_wrappers.h"

// Must be safe to assume that x and y have len 16 bytes, must return u16
inline static u16 simd_strcmp_short(const char *x, const char *y, int pad) {
    __m128i a =  _mm_loadu_si128((const __m128i*)x);
    __m128i b =  _mm_loadu_si128((const __m128i*)y);
    a = _mm_cmpeq_epi8(a, b);
    return (_mm_movemask_epi8(a) << pad) ^ (0xffff << pad);
}
// Must be safe to assume that x has len 16 bytes
inline static u16 simd_match_char(const char *string, char c) {
    __m128i a = _mm_loadu_si128((__m128i*)string);
    __m128i b = _mm_set1_epi8(c);
    a = _mm_cmpeq_epi8(a, b);
    return _mm_movemask_epi8(a);
}

// Must be safe to assume string has len 16 bytes
inline static bool simd_skip_to_char(const char *string, u64 *offset, char c, u64 limit) {
    __m128i a = _mm_loadu_si128((__m128i*)(string + *offset));
    __m128i b = _mm_set1_epi8(c);
    a = _mm_cmpeq_epi8(a, b);
    u16 mask = _mm_movemask_epi8(a);
    while(!mask) {
        *offset += 16;
        if (*offset > limit) {
            *offset -= 16;
            return false;
        }
        a = _mm_loadu_si128((__m128i*)(string + *offset));
        a = _mm_cmpeq_epi8(a, b);
        mask = _mm_movemask_epi8(a);
    }
    int tz = count_trailing_zeros_u32(mask);
    *offset += tz;
    return true;
}

// Must be safe to assume string has len 16 bytes
inline static bool simd_skip_passed_char(const char *string, u64 *offset, char c, u64 limit) {
    __m128i a = _mm_loadu_si128((__m128i*)(string + *offset));
    __m128i b = _mm_set1_epi8(c);
    a = _mm_cmpeq_epi8(a, b);
    u16 mask = _mm_movemask_epi8(a);
    // if mask is empty, no character was matched in the 16 bytes
    while(!mask) {
        *offset += 16;
        if (*offset > limit) {
            *offset -= 16;
            return false;
        }
        a = _mm_loadu_si128((__m128i*)(string + *offset));
        a = _mm_cmpeq_epi8(a, b);
        mask = _mm_movemask_epi8(a);
    }
    int tz = count_trailing_zeros_u32(mask);
    *offset += tz + 1;
    return true;
}

#endif // include guard
