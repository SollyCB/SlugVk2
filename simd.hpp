#ifndef SOL_SIMD_HPP_INCLUDE_GUARD_
#define SOL_SIMD_HPP_INCLUDE_GUARD_

#include <nmmintrin.h>
#include <emmintrin.h>
#include "basic.h"
#include "builtin_wrappers.h"

// Must be safe to assume that x and y have len 16 bytes
inline static int simd_strcmp_short(const char *x, const char *y, int pad) {
    __m128i a =  _mm_loadu_si128((const __m128i*)x);
    __m128i b =  _mm_loadu_si128((const __m128i*)y);
    a = _mm_cmpeq_epi8(a, b);
    return (_mm_movemask_epi8(a) << pad) ^ (Max_u16 << pad);
}
// Must be safe to assume that x has len 16 bytes
inline static u16 simd_match_char(const char *string, char c) {
    __m128i a = _mm_loadu_si128((__m128i*)string);
    __m128i b = _mm_set1_epi8(c);
    a = _mm_cmpeq_epi8(a, b);
    return _mm_movemask_epi8(a);
}

#endif // include guard
