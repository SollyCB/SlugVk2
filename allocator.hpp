#ifndef SOL_ALLOCATOR_HPP_INCLUDE_GUARD_
#define SOL_ALLOCATOR_HPP_INCLUDE_GUARD_

#include "tlsf.h"
#include "typedef.h"
#include "assert.h"

static inline size_t align(size_t size, size_t alignment) {
  const size_t alignment_mask = alignment - 1;
  return (size + alignment_mask) & ~alignment_mask;
}

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
                           /* ** Begin Global Allocators ** */

        /* Every function in this section applies to the two global allocators. */

// Call for instances of the global allocators. 
// These exist for the lifetime of the program.
Heap_Allocator   *get_instance_heap();
Linear_Allocator *get_instance_temp();

void init_allocators();
void kill_allocators();

void init_heap_allocator(u64 size);
void init_temp_allocator(u64 size);

void kill_heap_allocator();
void kill_temp_allocator();

u8 *memory_allocate_heap(u64 size, u64 alignment);
u8 *memory_reallocate_heap(u8 *ptr, u64 new_size);
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
static inline void zero_temp() {
    Linear_Allocator *temp = get_instance_temp();
    memset(temp->memory, 0, temp->used);
    temp->used = 0;
}
static inline void cut_tail_temp(u64 size) {
    get_instance_temp()->used -= size;
}
static inline void reset_to_mark_temp(u64 size) {
    get_instance_temp()->used = size;
}
static inline u64 get_mark_temp() {
    return get_instance_temp()->used;
}

                           /* ** End Global Allocators ** */


/* Functions to for other allocator types, to be allocated from the global heap allocator. */

inline static Linear_Allocator create_linear_allocator(u64 size) 
{
    size = align(size, 8);
    Linear_Allocator allocator;
    allocator.memory = memory_allocate_heap(size, 8);
    allocator.capacity = size;
    allocator.used = 0;
    return allocator;
}
inline static void destroy_linear_allocator(Linear_Allocator *allocator) 
{
    allocator->capacity = 0;
    allocator->used = 0;
    memory_free_heap(allocator->memory);
}

inline static u8*  linear_allocator_allocate(
    Linear_Allocator *allocator, u64 size, int alignment) 
{
    size = align(size, alignment);
    allocator->used = align(allocator->used, alignment);
    allocator->used += size;
    return allocator->memory + (allocator->used - size);
}
inline static void linear_allocator_cut_tail(Linear_Allocator *allocator, u64 size) 
{
    allocator->used -= size;
}
inline static void linear_allocator_reset_to_mark(Linear_Allocator *allocator, u64 mark)
{
    allocator->used = mark;
}
inline static void linear_allocator_zero(Linear_Allocator *allocator)
{
    memset(allocator->memory, 0, allocator->used);
    allocator->used = 0;
}

#endif
