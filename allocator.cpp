#include <cstdlib>
#include "allocator.hpp"
#include "print.hpp"

const u64 DEFAULT_CAP_HEAP_ALLOCATOR = 32 * 1024 * 1024;
const u64 DEFAULT_CAP_TEMP_ALLOCATOR = 32 * 1024 * 1024;

static Heap_Allocator gHeap;
static Linear_Allocator gTemp;

Heap_Allocator *get_instance_heap() {
    return &gHeap;
}
Linear_Allocator *get_instance_temp() {
    return &gTemp;
}

void init_allocators() {
    println("\nInitializing Allocators:");
    println("    Initial Capacity (Heap Allocator): %u", DEFAULT_CAP_HEAP_ALLOCATOR);
    println("    Initial Capacity (Temp Allocator): %u", DEFAULT_CAP_TEMP_ALLOCATOR);
    println("");
    init_heap_allocator(DEFAULT_CAP_HEAP_ALLOCATOR);
    init_temp_allocator(DEFAULT_CAP_TEMP_ALLOCATOR);
}
void kill_allocators() {
    println("\nShutting Down Allocators...");
    kill_heap_allocator();
    kill_temp_allocator();
    println("");
}

void init_heap_allocator(u64 size) {
    Heap_Allocator *allocator = get_instance_heap();
    allocator->capacity = size;
    allocator->memory = (u8*)malloc(size);
    allocator->used = 0;
    allocator->tlsf_handle = tlsf_create_with_pool(allocator->memory, size);
}
void init_temp_allocator(u64 size) {
    Linear_Allocator *allocator = get_instance_temp();
    allocator->capacity = size;
    void *ptr = malloc(size);
    allocator->memory = (u8*)align((u64)ptr, 16);
    allocator->used = 0;
}

// Let the OS free the memory... (it does enough random shit)
void kill_heap_allocator() {
#if DEBUG
    Heap_Allocator *allocator = get_instance_heap();
    u64 memory_stats[] = { 0, allocator->capacity };
    pool_t pool = tlsf_get_pool(allocator->tlsf_handle);

    tlsf_walk_pool(pool, NULL, (void*)&memory_stats);
    println("    Remaining Size in Heap Allocator: %u", allocator->used);
#endif
}
void kill_temp_allocator() {
#if DEBUG
    Linear_Allocator *allocator = get_instance_temp();
    println("    Remaining Size in Temp Allocator: %u", allocator->used);
#endif
}

// @DebugInfo add array for active allocations?
u8 *memory_allocate_heap(u64 size, u64 alignment) {
    void *ret;

    Heap_Allocator *allocator = get_instance_heap();
    if (alignment == 1)
        ret = tlsf_malloc(allocator->tlsf_handle, size);
    else
        ret = tlsf_memalign(allocator->tlsf_handle, alignment, size);

    // Get actual allocated size
    allocator->used += tlsf_block_size(ret);

    return (u8*)ret;
}

u8 *memory_reallocate_heap(u8 *ptr, u64 new_size) {
    u64 old_size = tlsf_block_size((void*)ptr);
    Heap_Allocator *allocator = get_instance_heap();
    allocator->used -= old_size;
    ptr = (u8*)tlsf_realloc(allocator->tlsf_handle, ptr, new_size);
    allocator->used += tlsf_block_size((void*)ptr);
    return ptr;
}

u8 *memory_allocate_temp(u64 size, u64 alignment) {
    Linear_Allocator *allocator = get_instance_temp();
    size = align(size, alignment);

    // pad 
    allocator->used = align(allocator->used, alignment);

    u8 *ret = allocator->memory + allocator->used;
    allocator->used += size;

    ASSERT(allocator->used <= allocator->capacity, "Temp Allocator Overflow");
    return ret;
}
