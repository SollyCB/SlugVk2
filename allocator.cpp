#include "allocator.hpp"

const u64 DEFAULT_CAP_HEAP_ALLOCATOR = 32 * 1024 * 1024;
const u64 DEFAULT_CAP_TEMP_ALLOCATOR = 32 * 1024;

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
    std::cout << "    Initial Capacity (Heap Allocator): " <<  DEFAULT_CAP_HEAP_ALLOCATOR << '\n';
    std::cout << "    Initial Capacity (Temp Allocator): " <<  DEFAULT_CAP_TEMP_ALLOCATOR << '\n';

    std::cout << '\n';
    init_heap_allocator(DEFAULT_CAP_HEAP_ALLOCATOR);
    init_temp_allocator(DEFAULT_CAP_TEMP_ALLOCATOR);
}
void kill_allocators() {
    println("\nShutting Down Allocators...");
    kill_heap_allocator();
    kill_temp_allocator();
    std::cout << '\n';
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
    allocator->memory = (u8*)malloc(size);
    allocator->used = 0;
}

// Let the OS free the memory... (it does enough random shit)
void kill_heap_allocator() {
#if DEBUG
    Heap_Allocator *allocator = get_instance_heap();
    u64 memory_stats[] = { 0, allocator->capacity };
    pool_t pool = tlsf_get_pool(allocator->tlsf_handle);

    tlsf_walk_pool(pool, NULL, (void*)&memory_stats);
    std::cout << "    Remaining Size in Heap Allocator: " << allocator->used << '\n';
#endif
}
void kill_temp_allocator() {
#if DEBUG
    Linear_Allocator *allocator = get_instance_temp();
    std::cout << "    Remaining Size in Temp Allocator: " <<  allocator->used << '\n';
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
u8 *memory_allocate_temp(u64 size, u64 alignment) {
    Linear_Allocator *allocator = get_instance_temp();
    // pad 

    // @Note i think this is actually wrong, because if the size is already aligned then alignment is just entirely 
    // added???
    allocator->used += alignment - ((u64)(allocator->memory + allocator->used) & (alignment - 1));

    u8 *ret = allocator->memory + allocator->used;
    allocator->used += align(size, alignment);

    ASSERT(allocator->used <= allocator->capacity, "Temp Allocator Overflow");
    return ret;
}
