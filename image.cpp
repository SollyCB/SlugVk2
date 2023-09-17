#include "typedef.h"
#include "image.hpp"
#include "allocator.hpp"

static inline void* stbi_malloc_wrapper_heap(u64 size) {
    return memory_allocate_heap(size, 8);
}
static inline void* stbi_realloc_wrapper_heap(void *p, u64 newsz) {
    return memory_reallocate_heap((u8*)p, newsz);
}
static inline void stbi_free_wrapper_heap(void *p) {
    return memory_free_heap(p);
}

#define STBI_MALLOC(sz)       stbi_malloc_wrapper_heap(sz)
#define STBI_REALLOC(p,newsz) stbi_realloc_wrapper_heap(p, newsz)
#define STBI_FREE(p)          stbi_free_wrapper_heap(p)

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const u8* load_image(const char *filename, int *width, int *height) {
    return (const u8*)stbi_load(filename, width, height, NULL, 0);
}
