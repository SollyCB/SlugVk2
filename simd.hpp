#ifndef SOL_SIMD_HPP_INCLUDE_GUARD_
#define SOL_SIMD_HPP_INCLUDE_GUARD_

#include <nmmintrin.h>
#include <emmintrin.h>
#include "basic.h"
#include "builtin_wrappers.h"


/*
 *****************************************

    @Note Lots of (if not all) of these functions are inlined despite looking too long for inlining.
    But in reality they only *look* long, because the number of instructions is very low, because of the intrinsics

 *****************************************
*/

inline static bool simd_find_char_interrupted(const char *string, char find, char interrupt, u64 *pos) {
    __m128i a = _mm_loadu_si128((__m128i*)string);
    __m128i b = _mm_set1_epi8(find);
    __m128i c = _mm_set1_epi8(interrupt);
    __m128i d = _mm_cmpeq_epi8(a, b);
    u16 mask1 = _mm_movemask_epi8(d);
    d = _mm_cmpeq_epi8(a, c);
    u16 mask2 = _mm_movemask_epi8(d);

    // @Note @Branching There is probably some smart way to do this to cut down branching...
    // Idk how to deal with the UB for mask == 0x0 other than branching... Maybe the compiler
    // will figure out smtg smart, @Todo check godbolt for this
    u64 inc = 0;
    while(!mask1 && !mask2) {
        inc += 16;
        a = _mm_loadu_si128((__m128i*)string + inc);
        d = _mm_cmpeq_epi8(a, b);
        mask1 = _mm_movemask_epi8(d);
        d = _mm_cmpeq_epi8(a, c);
        mask2 = _mm_movemask_epi8(d);
    }

    // Intel optimisation manual: fallthrough condition chosen if nothing in btb
    if (mask1 && mask2) {
        int tz1 = count_trailing_zeros_u16(mask1);
        int tz2 = count_trailing_zeros_u16(mask2);
        *pos += count_trailing_zeros_u16(mask2 | mask1) + inc;
        return tz1 < tz2;
    } else if (!mask1 && mask2) {
        *pos += count_trailing_zeros_u16(mask2) + inc;
        return false;
    }

    *pos += count_trailing_zeros_u16(mask1);
    return true;
}

// Must be safe to assume that x and y have len 16 bytes, must return u16
inline static u64 simd_search_for_char(const char *string, char c) {
    __m128i a =  _mm_loadu_si128((const __m128i*)string);
    __m128i b =  _mm_set1_epi8(c);
    a = _mm_cmpeq_epi8(a, b);
    u16 mask = _mm_movemask_epi8(a);
    u64 inc = 0;
    while(!mask) {
        inc += 16;
        a = _mm_loadu_si128((__m128i*)(string + inc));
        a = _mm_cmpeq_epi8(a, b);
        mask = _mm_movemask_epi8(a);
    }
    u16 tz = count_trailing_zeros_u16(mask);
    return inc + tz;
}

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
    u64 inc = 0;
    __m128i a = _mm_loadu_si128((__m128i*)(string));
    __m128i b = _mm_set1_epi8(c);
    a = _mm_cmpeq_epi8(a, b);
    u16 mask = _mm_movemask_epi8(a);
    while(!mask) {
        inc += 16;
        if (inc > limit) {
            return false;
        }
        a = _mm_loadu_si128((__m128i*)(string + inc));
        a = _mm_cmpeq_epi8(a, b);
        mask = _mm_movemask_epi8(a);
    }
    int tz = count_trailing_zeros_u32(mask);
    inc += tz;
    *offset += inc;
    return true;
}

// Must be safe to assume string has len 16 bytes
inline static bool simd_skip_passed_char(const char *string, u64 *offset, char c, u64 limit) {
    u64 inc = 0;
    __m128i a = _mm_loadu_si128((__m128i*)(string + inc));
    __m128i b = _mm_set1_epi8(c);
    a = _mm_cmpeq_epi8(a, b);
    u16 mask = _mm_movemask_epi8(a);
    // if mask is empty, no character was matched in the 16 bytes
    while(!mask) {
        inc += 16;
        if (inc > limit) {
            inc -= 16;
            *offset += inc;
            return false;
        }
        a = _mm_loadu_si128((__m128i*)(string + inc));
        a = _mm_cmpeq_epi8(a, b);
        mask = _mm_movemask_epi8(a);
    }
    int tz = count_trailing_zeros_u32(mask);
    *offset += tz + 1 + inc;
    return true;
}

inline static void simd_skip_whitespace(const char *string, u64 *offset) {
    u64 inc = 0;
    __m128i a = _mm_loadu_si128((__m128i*)string);
    __m128i b = _mm_set1_epi8(' ');
    __m128i c = _mm_set1_epi8('\n');
    // Idk if checking tabs is necessary but I think it is because for some reason tabs exist...
    // THEY ARE JUST SOME NUMBER OF SPACES! THERE ISNT EVEN CONSENSUS ON  HOW MANY SPACES!! JSUT SOME NUMBER OF THEM!
    __m128i e = _mm_set1_epi8('\t');
    __m128i d;
    d = _mm_cmpeq_epi8(a, b);
    u16 mask = _mm_movemask_epi8(d);
    d = _mm_cmpeq_epi8(a, e);
    mask |= _mm_movemask_epi8(d);
    d = _mm_cmpeq_epi8(a, c);
    mask |= _mm_movemask_epi8(d);
    while(mask == 0xffff) {
        inc += 16;
        a = _mm_loadu_si128((__m128i*)string + inc);
        d = _mm_cmpeq_epi8(a, b);
        mask = _mm_movemask_epi8(d);
        d = _mm_cmpeq_epi8(a, b);
        mask |= _mm_movemask_epi8(d);
        d = _mm_cmpeq_epi8(a, e);
        mask |= _mm_movemask_epi8(d);
    }
    int tz = count_trailing_zeros_u16(mask);
    *offset += inc + tz;
}

// Must be safe to assume string has len 16 bytes
inline static bool simd_skip_to_int(const char *string, u64 *offset, u64 limit) {
    __m128i b = _mm_set1_epi8(47); // ascii 0 - 1
    __m128i c = _mm_set1_epi8(58); // ascii 9 + 1

    __m128i a = _mm_loadu_si128((__m128i*)(string));
    __m128i d = _mm_cmpgt_epi8(a, b);
    a = _mm_cmplt_epi8(a, c);
    a = _mm_and_si128(d, a);
    u16 mask = _mm_movemask_epi8(a);

    u64 inc = 0;
    while(!mask) {
        inc += 16;
        // @Note inc can be less than limit, but then the match check will check beyond limit.
        // So make limit limit the check range, dont place it at the eof for example...
        if (inc > limit) {
            return false;
        }
            
        a = _mm_loadu_si128((__m128i*)(string + inc));
        d = _mm_cmpgt_epi8(a, b);
        a = _mm_cmplt_epi8(a, c);
        a = _mm_and_si128(d, a);
        mask = _mm_movemask_epi8(a);
    }

    int tz = count_trailing_zeros_u16(mask);
    *offset += inc + tz;

    return true;
}

// Must be safe to assume string has len 16
inline static bool simd_find_int_interrupted(const char *string, char interrupt, u64 *pos) {
    __m128i b = _mm_set1_epi8(47); // ascii 0 - 1
    __m128i c = _mm_set1_epi8(58); // ascii 9 + 1
    __m128i d = _mm_set1_epi8(interrupt);

    __m128i a = _mm_loadu_si128((__m128i*)string);
    __m128i e = _mm_cmpeq_epi8(a, d);
    u16 mask2 = _mm_movemask_epi8(e);

    e = _mm_cmpgt_epi8(a, b);
    a = _mm_cmplt_epi8(a, c);
    a = _mm_and_si128(a, e);
    u16 mask1 = _mm_movemask_epi8(a);

    u64 inc = 0;
    // Intel optimisation manual: fallthrough condition chosen if nothing in btb
    while(!mask1 && !mask2) {
        inc += 16;
        a = _mm_loadu_si128((__m128i*)string + inc);
        e = _mm_cmpeq_epi8(a, d);
        mask2 = _mm_movemask_epi8(e);

        e = _mm_cmpgt_epi8(a, c);
        a = _mm_cmplt_epi8(a, b);
        mask1 = _mm_movemask_epi8(a);
    }

    if (mask1 && mask2) {
        int tz1 = count_trailing_zeros_u16(mask1);
        int tz2 = count_trailing_zeros_u16(mask2);
        *pos += count_trailing_zeros_u16(mask1 | mask2) + inc; // avoid a branch
        return tz1 < tz2;
    }

    if (!mask1 && mask2) {
        *pos += count_trailing_zeros_u16(mask2) + inc;
        return false;
    }

    *pos += count_trailing_zeros_u16(mask1) + inc;
    return true;
}

#endif // include guard
