#ifndef SOL_ALLOCATOR_HPP_INCLUDE_GUARD_
#define SOL_ALLOCATOR_HPP_INCLUDE_GUARD_

#include "tlsf.h"
#include "typedef.h"
#include "assert.h"

struct Heap_Allocator {
    u64 capacity;
    u64 used;
    u8 *memory;
    void *tlsf_handle;
};
struct Linear_Allocator {
    u64 capacity;
    u64 used;
    u8 *memory;
};

Heap_Allocator *get_instance_heap();
Linear_Allocator *get_instance_temp();

void init_allocators();
void kill_allocators();

void init_heap_allocator(u64 size);
void init_temp_allocator(u64 size);

void kill_heap_allocator();
void kill_temp_allocator();

u8 *memory_allocate_heap(u64 size, u64 alignment);
u8 *memory_allocate_temp(u64 size, u64 alignment);

// Inlines
inline void memory_free_heap(void *ptr) {
    u64 size = tlsf_block_size(ptr);

    Heap_Allocator *allocator = get_instance_heap();
    ASSERT(size <= allocator->used, "Heap Allocator Underflow");
    allocator->used -= size;
    tlsf_free(allocator->tlsf_handle, ptr);
}

static inline void reset_temp() {
    get_instance_temp()->used = 0;
}
static inline void cut_tail_temp(u64 size) {
    get_instance_temp()->used -= size;
}
static inline void cut_diff_temp(u64 size) {
    get_instance_temp()->used = size;
}
static inline u64 get_mark_temp() {
    return get_instance_temp()->used;
}

static inline size_t align(size_t size, size_t alignment) {
  const size_t alignment_mask = alignment - 1;
  return (size + alignment_mask) & ~alignment_mask;
}

#endif
