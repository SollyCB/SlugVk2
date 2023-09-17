#ifndef SOL_FILE_HPP_INCLUDE_GUARD_
#define SOL_FILE_HPP_INCLUDE_GUARD_

#include "basic.h"

const u8* file_read_bin_temp(const char *file_name, u64 *size);
const u8* file_read_bin_heap(const char *file_name, u64 *size);
const u8* file_read_char_temp(const char *file_name, u64 *size);
const u8* file_read_char_heap(const char *file_name, u64 *size);

#endif // include guard
