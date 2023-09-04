#ifndef SOL_ARRAY_HPP_INCLUDE_GUARD_
#define SOL_ARRAY_HPP_INCLUDE_GUARD_

#include "basic.h"

// @Note Both of these could probably use u32s to measure length, as 4 billion items seems enough?? smtg to test

// Uses the temporary allocator, and therefore cannot grow. Data pointer aligned to 8 bytes.
// @Note: (the not growing may change in future, I just worry that this may 'splode the temp allocator)
template<typename T>
struct Static_Array {
    u64 len;
    T *data;
#if DEBUG
    u64 cap;
#endif
};

// Uses the heap allocator, and will grow on overflow. Data pointer aligned to 8 bytes.
template<typename T>
struct Dyn_Array {
    u64 len;
    u64 cap;
    T *data;
};

template<typename T>
inline void init_static_array(Static_Array<T> *array, u64 item_count) {
#if DEBUG
    array->cap = item_count;
#endif
    array->len = 0;
    array->data = (T*)memory_allocate_temp(item_count * sizeof(T), 8);
}
template<typename T>
inline void init_dyn_array(Dyn_Array<T> *array, u64 item_count) {
    array->cap = item_count;
    array->len = 0;
    array->data = (T*)memory_allocate_heap(item_count * sizeof(T), 8);
}
template<typename T>
inline void kill_dyn_array(Dyn_Array<T> *array) {
    memory_free_heap((void*)array->data);
}

template<typename T>
inline Static_Array<T> get_static_array(u64 size) {
    Static_Array<T> ret;
    init_static_array<T>(&ret, size);
    return ret;
}
template<typename T>
inline Dyn_Array<T> get_dyn_array(u64 size) {
    Dyn_Array<T> ret;
    init_dyn_array<T>(&ret, size);
    return ret;
}

template<typename T>
inline void grow_dyn_array(Dyn_Array<T> *array, u64 item_count) {
    T* old_data = array->data;

    array->cap += item_count;
    array->data = (T*)memory_allocate_heap(array->cap * sizeof(T), 8);

    memcpy(array->data, old_data, array->len * sizeof(T));
    memory_free_heap(old_data);
}

template<typename T>
inline T* append_to_static_array(Static_Array<T> *array) {
#if DEBUG
    ASSERT(array->len <= array->cap, "Static Array Overflow");
#endif
    array->len++;
    return array->data + array->len - 1;
}
template<typename T>
inline T* append_to_dyn_array(Dyn_Array<T> *array) {
    if (array->len == array->cap)
        grow_dyn_array<T>(array, array->cap);

    array->len++;
    return array->data + array->len - 1;
}

template<typename T>
inline void copy_to_static_array(Static_Array<T> *array, T *from, u64 item_count) {
#if DEBUG
    ASSERT(array->len + item_count <= array->cap, "Static Array Overflow");
#endif
    memcpy(array->data + array->len, from, item_count * sizeof(T));
    array->len += item_count;
}
template<typename T>
inline void copy_to_dyn_array(Dyn_Array<T> *array, T *from, u64 item_count) {
    // @Note how much to grow by?
    if (array->len + item_count > array->cap)
        grow_dyn_array(array, item_count);

    memcpy(array->data + array->len, from, item_count * sizeof(T));
    array->len += item_count;
}

template<typename T>
inline T* index_static_array(Static_Array<T> *array, u64 index) {
    ASSERT(index < array->len, "Static Array Out of Bounds Access");
    return array->data + index;
}
template<typename T>
inline T* index_dyn_array(Dyn_Array<T> *array, u64 index) {
    ASSERT(index < array->len, "Dyn Array Out of Bounds Access");
    return array->data + index;
}

#endif
