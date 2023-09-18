#ifndef SOL_BASIC_H_INCLUDE_GUARD_
#define SOL_BASIC_H_INCLUDE_GUARD_

#include "typedef.h"
#include "assert.h"
#include "print.hpp"
#include "allocator.hpp"
#include "array.hpp"

// @Todo move this to a more sensible file
inline static int get_str_len(const char *x) {
    int i = 0;
    while(x[i] != '\0')
        i++;
    return i;
}


#endif
