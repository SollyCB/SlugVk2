#pragma once
#include <immintrin.h>

#include "builtin_wrappers.h"
#include "Basic.hpp"
#include "external/wyhash.h"

#if TEST
#include "test/test.hpp"
#include "String.hpp"
#endif

// Probably should overload the insert functions so that value can be a ptr, while key is copied

namespace Sol {

const u8 EMPTY = 0b1111'1111;
const u8 DEL   = 0b1000'0000;
const u8 GROUP_WIDTH = 16;

// @Implementation I am not going to make a dynamic and a static hashmap like I did with 
// arrays. I guess this one should always be able to grow... a static hashmap doesnt seem 
// as intuitive/sensible as a static array

template <typename T>
inline uint64_t calculate_hash(const T &value, size_t seed = 0) {
    return wyhash(&value, sizeof(T), seed, _wyp);
}

template <size_t N>
inline uint64_t calculate_hash(const char (&value)[N], size_t seed = 0) {
    return wyhash(value, strlen(value), seed, _wyp);
}

inline uint64_t calculate_hash(const char *value, size_t seed) {
    return wyhash(value, strlen(value), seed, _wyp);
}

inline uint64_t hash_bytes(void *data, size_t len, size_t seed = 0) {
    return wyhash(data, len, seed, _wyp);
}

inline size_t align(size_t size, size_t alignment) {
  const size_t alignment_mask = alignment - 1;
  return (size + alignment_mask) & ~alignment_mask;
}

inline void checked_mul(size_t &res, size_t mul) {
    DEBUG_ASSERT(UINT64_MAX / res > mul, "usize checked mul overflow");
    res = res * mul;
}

inline uint32_t count_trailing_zeros(u16 mask) { return builtin_ctzl(mask); }

inline void make_group_empty(uint8_t *bytes) {
    __m128i ctrl = _mm_set1_epi8(EMPTY);
    _mm_store_si128(reinterpret_cast<__m128i *>(bytes), ctrl);
}

struct Group {
    __m128i ctrl;

    static inline Group get_from_index(u64 index, u8 *data) {
        Group ret;
        ret.ctrl = *reinterpret_cast<__m128i*>(data + index);
        return ret;
    }
    static inline Group get_empty() {
        Group ret;
        ret.ctrl = _mm_set1_epi8(EMPTY);
        return ret;
    }
    inline u16 is_empty() {
        __m128i empty = _mm_set1_epi8(EMPTY);
        __m128i res = _mm_cmpeq_epi8(ctrl, empty);
        u16 mask = _mm_movemask_epi8(res);
        return mask;
    }
    inline u16 is_special() {
        uint16_t mask = _mm_movemask_epi8(ctrl);
        return mask;
    }
    inline u16 is_full() {
        u16 mask = is_special();
        uint16_t invert = ~mask;
        return invert;
    }
    inline void fill(uint8_t *bytes) {
        _mm_store_si128(reinterpret_cast<__m128i *>(bytes), ctrl);
    }
    inline u16 match_byte(uint8_t byte) {
        __m128i to_match = _mm_set1_epi8(byte);
        __m128i match = _mm_cmpeq_epi8(ctrl, to_match);
        return (uint16_t)_mm_movemask_epi8(match);
    }
};

template<typename K, typename V>
struct HashMap {

    struct KeyValue {
        K key;
        V value;
    };

    struct Iter {
        usize current_pos;
        HashMap<K, V> *map;

        KeyValue* next() {
			u8 pos_in_group;
			u16 mask;
			u32 tz;
			usize group_index;
			Group gr;
			KeyValue *kv;
			while(current_pos < map->cap) {

				pos_in_group = current_pos & (GROUP_WIDTH - 1);
				group_index = current_pos - pos_in_group;

				gr = *(Group*)(map->data + group_index);
				mask = gr.is_full();
				mask >>= pos_in_group;

				if (mask) {
					tz = count_trailing_zeros(mask);
					current_pos += tz;
					kv = (KeyValue*)(map->data + map->cap + (current_pos * sizeof(KeyValue)));

					++current_pos;
					return kv;
				}

				current_pos += GROUP_WIDTH - pos_in_group;
			}
			return NULL;
        }
    };
    Iter iter() {
        Iter ret = {0, this};
        return ret;
    };

    usize cap; // in key-value pairs
    usize slots_left; // in key-value pairs
    u8 *data;
    Allocator *allocator;

    static inline HashMap<K, V> get(usize initial_cap, Allocator *alloc) {
        HashMap<K, V> ret;
        ret.init(initial_cap, alloc);
        return ret;
    }
    void init(usize initial_cap, Allocator *alloc) {
        cap        = align(initial_cap, GROUP_WIDTH);
        slots_left = ((cap + 1) / 8) * 7;
        allocator  = alloc;
        data       = (u8*)mem_alloca(cap * sizeof(KeyValue) + cap, GROUP_WIDTH, allocator);
        for(usize i = 0; i < cap; i += GROUP_WIDTH) {
            make_group_empty(data + i);
        }
    }

    bool insert_cpy(K key, V value) {
        if (slots_left == 0) {
            u8 *old_data = data;
            usize old_cap = cap;

            checked_mul(cap, 2);
            data = (u8*)mem_alloca(cap + cap * sizeof(KeyValue), GROUP_WIDTH, allocator);
            slots_left = ((cap + 1) / 8) * 7;

            for(usize i = 0; i < cap; i += GROUP_WIDTH) {
                make_group_empty(data + i);
            }

            Group gr;
            KeyValue *kv;
            u16 mask;
            u32 tz;
            for(usize group_index = 0; group_index < old_cap; group_index += GROUP_WIDTH) {
                gr = Group::get_from_index(group_index, old_data);

                mask = gr.is_full();
                while(mask > 0) {
                    tz = count_trailing_zeros(mask);

                    // @FFS This was fking set to '&=' cos I am dumb and tired which means 
                    // infinite loop lol... I fking hate and love programming
                    mask ^= 1 << tz;

                    kv = (KeyValue*)(old_data + old_cap);

                    DEBUG_ASSERT(insert_cpy(kv[group_index + tz].key , kv[group_index + tz].value) != false, "Insert fail while growing");
                }
            }

            mem_free(old_data, allocator);
        }

        u64 hash = hash_bytes((void*)&key, sizeof(key));
        u8 top7 = hash >> 57;

        u64 exact_index = (hash & (cap - 1));
        u64 group_index = exact_index - (exact_index & (GROUP_WIDTH - 1));

        KeyValue *kv;
        Group gr;
        u16 mask;
        u32 tz;
        usize inc = 0;
        while(inc < cap) {
            DEBUG_ASSERT(inc <= cap, "Probe went too far");
            gr = Group::get_from_index(group_index, data);
            mask = gr.is_empty();

            if (!mask) {
                inc += GROUP_WIDTH;
                group_index += inc;
                group_index &= cap - 1;
                continue;
            }

            tz = count_trailing_zeros(mask);
            exact_index = group_index + tz;
            data[exact_index] &= top7;

            kv = (KeyValue*)(data + cap);
            kv[exact_index].key = key;
            kv[exact_index].value = value;

            --slots_left;
            return true;
        }

        return false;
    }
    bool insert_ptr(K *key, V *value) {
		DEBUG_ASSERT(key != nullptr, "pass key == nullptr to HashMap::insert_ptr");
		DEBUG_ASSERT(value != nullptr, "pass value == nullptr to HashMap::insert_ptr");

        if (slots_left == 0) {
            u8 *old_data = data;
            usize old_cap = cap;

            checked_mul(cap, 2);
            data = (u8*)mem_alloca(cap + cap * sizeof(KeyValue), GROUP_WIDTH, allocator);
            slots_left = ((cap + 1) / 8) * 7;

            for(usize i = 0; i < cap; i += GROUP_WIDTH) {
                make_group_empty(data + i);
            }

            KeyValue *kv;
            Group gr;
            u16 mask;
            u32 tz;
            for(usize group_index = 0; group_index < old_cap; group_index += GROUP_WIDTH) {
                gr = *(Group*)(old_data + group_index);

                mask = gr.is_full();
                while(mask > 0) {
                    tz = count_trailing_zeros(mask);

                    // @FFS This was fking set to '&=' cos I am dumb and tired which means 
                    // infinite loop lol... I fking hate and love programming
                    mask ^= 1 << tz;

                    kv = (KeyValue*)(old_data + old_cap);
                    usize exact_index = tz + group_index;
                    DEBUG_ASSERT(
                        insert_ptr(&kv[exact_index].key, &kv[exact_index].value) != false,
                        "Insert fail while growing");
                }
            }

            mem_free(old_data, allocator);
        }

        u64 hash = hash_bytes((void*)key, sizeof(*key));
        u8 top7 = hash >> 57;

        u64 exact_index = (hash & (cap - 1));
        u64 group_index = exact_index - (exact_index & (GROUP_WIDTH - 1));

        KeyValue *kv;
        Group gr;
        u16 mask;
        u32 tz;
        usize inc = 0;
        while(inc < cap) {
            DEBUG_ASSERT(inc < cap, "Probe went too far");
            gr = Group::get_from_index(group_index, data);
            mask = gr.is_empty();

            if (!mask) {
                inc += GROUP_WIDTH;
                group_index += inc;
                group_index &= cap - 1;
                continue;
            }

            tz = count_trailing_zeros(mask);
            exact_index = group_index + tz;
            data[exact_index] &= top7;

            kv = (KeyValue*)(data + cap);
            kv[exact_index].key = *key;
            kv[exact_index].value = *value;

            --slots_left;
            return true;
        }

        return false;
    }

    V* find_cpy(K key) {
        u64 hash = hash_bytes((void*)&key, sizeof(key));
        u8 top7 = hash >> 57;
        u64 exact_index = hash & (cap - 1);
        u64 group_index = exact_index - (exact_index & (GROUP_WIDTH - 1));

        KeyValue *kv;
        Group gr;
        u16 mask;
        u32 tz;
        usize inc = 0;
        while(inc < cap) {
            gr = Group::get_from_index(group_index, data);
            mask = gr.match_byte(top7);

            if (mask) {
                // Ik the while catches an empty mask, but I want to skip the pointer arithmetic
                kv = (KeyValue*)(data + cap);
                while(mask) {
                    tz = count_trailing_zeros(mask);
                    exact_index = group_index + tz;

                    if (kv[exact_index].key == key)
                        return &kv[exact_index].value;

                    mask ^= 1 << tz;
                }
            }

            inc += GROUP_WIDTH;
            group_index += inc;
            group_index &= cap - 1;
        }
        return NULL;
    }

    V* find_ptr(K *key) {
		DEBUG_ASSERT(key != nullptr, "pass key == nullptr to HashMap::find_ptr");

        u64 hash = hash_bytes((void*)key, sizeof(*key));
        u8 top7 = hash >> 57;
        u64 exact_index = hash & (cap - 1);
        u64 group_index = exact_index - (exact_index & (GROUP_WIDTH - 1));

        KeyValue *kv;
        Group gr;
        u16 mask;
        u32 tz;
        usize inc = 0;
        while(inc < cap) {
            gr = Group::get_from_index(group_index, data);
            mask = gr.match_byte(top7);

            if (mask) {
                // Ik the while catches an empty mask, but I want to skip the pointer arithmetic
                kv = (KeyValue*)(data + cap);
                while(mask) {
                    tz = count_trailing_zeros(mask);
                    exact_index = group_index + tz;

                    if (kv[exact_index].key == *key)
                        return &kv[exact_index].value;

                    mask ^= 1 << tz;
                }
            }

            inc += GROUP_WIDTH;
            group_index += inc;
            group_index &= cap - 1;
        }
        return NULL;
    }

    void scratch_kill() {
        DEBUG_ASSERT(allocator == SCRATCH, "Incorrect shutdown method");
        SCRATCH->cut(cap);
        cap = 0;
        slots_left = 0;
        allocator = NULL;
    }
    void heap_kill() {
        DEBUG_ASSERT(allocator == HEAP, "Incorrect shutdown method");
        mem_free(data, HEAP);
        cap = 0;
        slots_left = 0;
        allocator = NULL;
    }



#if TEST
    static void test_cpy() {
        usize cap = 16;
        auto map = HashMap<int, int>::get(cap, HEAP); 
        for(int i = 0; i < cap; ++i) {
            TEST_EQ("EmptyInit", map.data[i], EMPTY, false);
        }
        // @Memory tlsf is not allocated enough for 1'000'000 I dont think
        // as that number segfaults, but as it can do arbitrarily large numbers up to
        // amount, I assume that it is just running out of memory (seems unlikely that 
        // there is an implementation problem when it can do 100k.
        for(int i = 0; i < 100'000; ++i) {
            int key = i;
            int value = i;
           bool index = map.insert_cpy(key, value);

            TEST_NEQ("Insert", index, false, false);
            DEBUG_ASSERT(index != false, "Insert Fail");
            HashMap<int, int>::KeyValue *kv = (HashMap<int, int>::KeyValue*)(map.data + map.cap);

        }

        // End
        map.heap_kill();
    }
    static void test_ptr() {
        usize cap = 16;
        auto map = HashMap<int, int>::get(cap, HEAP); 
        for(int i = 0; i < cap; ++i) {
            TEST_EQ("EmptyInit", map.data[i], EMPTY, false);
        }
        // @Memory tlsf is not allocated enough for 1'000'000 I dont think
        // as that number segfaults, but as it can do arbitrarily large numbers up to
        // amount, I assume that it is just running out of memory (seems unlikely that 
        // there is an implementation problem when it can do 100k.
        for(int i = 0; i < 100'000; ++i) {
            int key = i;
            int value = i;
            int key_mv = i;
            int value_mv = i;
            bool index = map.insert_ptr(&key_mv, &value_mv);

            TEST_NEQ("Insert", index, false, false);
            DEBUG_ASSERT(index != false, "Insert Fail");
            HashMap<int, int>::KeyValue *kv = (HashMap<int, int>::KeyValue*)(map.data + map.cap);

        }

        // End
        map.heap_kill();
    }
    static void test_find_cpy() {
        usize cap = 16;
        auto map = HashMap<int, int>::get(cap, HEAP); 
        for(int i = 0; i < cap; ++i) {
            TEST_EQ("EmptyInit", map.data[i], EMPTY, false);
        }
        // @Memory tlsf is not allocated enough for 1'000'000 I dont think
        // as that number segfaults, but as it can do arbitrarily large numbers up to
        // amount, I assume that it is just running out of memory (seems unlikely that 
        // there is an implementation problem when it can do 100k.
        for(int i = 0; i < 100'000; ++i) {
            int key = i;
            int value = i;
           bool index = map.insert_cpy(key, value);

            TEST_NEQ("Insert", index, false, false);
            DEBUG_ASSERT(index != false, "Insert Fail");
            HashMap<int, int>::KeyValue *kv = (HashMap<int, int>::KeyValue*)(map.data + map.cap);

        }
        for(int i = 0; i < 100'000; ++i) {
            int *f = map.find_cpy(i);
            DEBUG_ASSERT(f != nullptr, "AAARGHHHHHHHH");
        }

        // End
        map.heap_kill();
    }
    static void test_find_ptr() {
        usize cap = 16;
        auto map = HashMap<int, int>::get(cap, HEAP); 
        for(int i = 0; i < cap; ++i) {
            TEST_EQ("EmptyInit", map.data[i], EMPTY, false);
        }
        // @Memory tlsf is not allocated enough for 1'000'000 I dont think
        // as that number segfaults, but as it can do arbitrarily large numbers up to
        // amount, I assume that it is just running out of memory (seems unlikely that 
        // there is an implementation problem when it can do 100k.
        for(int i = 0; i < 100'000; ++i) {
            int key = i;
            int value = i;
            int key_mv = i;
            int value_mv = i;
			bool index = map.insert_ptr(&key_mv, &value_mv);

            TEST_NEQ("Insert", index, false, false);
            DEBUG_ASSERT(index != false, "Insert Fail");
            HashMap<int, int>::KeyValue *kv = (HashMap<int, int>::KeyValue*)(map.data + map.cap);

        }
        for(int i = 0; i < 100'000; ++i) {
            int *f = map.find_ptr(&i);
            DEBUG_ASSERT(f != nullptr, "AAARGHHHHHHHH");
        }

        // End
        map.heap_kill();
    }
    static void test_str() {
        usize cap = 16;
        auto map = HashMap<u64, StringBuffer>::get(cap, HEAP); 
        for(int i = 0; i < cap; ++i) {
            TEST_EQ("EmptyInit", map.data[i], EMPTY, false);
        }
        // @Memory tlsf is not allocated enough for 1'000'000 I dont think
        // as that number segfaults, but as it can do arbitrarily large numbers up to
        // amount, I assume that it is just running out of memory (seems unlikely that 
        // there is an implementation problem when it can do 100k.
        for(int i = 0; i < 10'000; ++i) {
            u64 key = i;
            StringBuffer value = StringBuffer::get("Word", 4, HEAP);
            bool index = map.insert_cpy(key, value);

            TEST_NEQ("Insert", index, false, false);
            DEBUG_ASSERT(index != false, "Insert Fail");
            HashMap<u64, StringBuffer>::KeyValue *kv = (HashMap<u64, StringBuffer>::KeyValue*)(map.data + map.cap);
        }
        for(int i = 0; i < 10'000; ++i) {
            u64 key = i;
            StringBuffer *f = map.find_cpy(key);
            StringBuffer *t = nullptr;
            DEBUG_ASSERT(f != nullptr, "AAARGHHHHHHHH");
            TEST_STR_EQ("find_string_with_cstr_key", f->as_cstr(), "Word", false);
            f->kill();
        }

        // End
        map.heap_kill();
    }
    static void test_iter() {
        usize cap = 16;
        auto map = HashMap<int, int>::get(cap, HEAP); 

        for(int i = 0; i < 100; ++i) {
            map.insert_cpy(i, i);
        }

        auto iter = map.iter();

        auto *kv = iter.next();
		int count = 0;
        while(kv) {
			TEST_EQ("Iter_Key_eq_Val", kv->key, kv->value, false);
            kv = iter.next();
			++count;
        }
		TEST_EQ("all_items_returned", count, 100, false);

        // End
        map.heap_kill();
    }
	static void test_u16() {
        usize cap = 16;
		struct Type {
			union {
			u8 one;
			u16 two;
			};
		};
        auto map = HashMap<u16, Type>::get(cap, HEAP); 

        for(u16 i = 0; i < 10'000; ++i) {
			Type type;
            map.insert_cpy(i, type);
        }

        auto iter = map.iter();

        auto *kv = iter.next();
		int count = 0;
        while(kv) {
			kv = iter.next();
			++count;
        }
		TEST_EQ("all_items_returned", count, 10'000, false);

        // End
        map.heap_kill();
	}
    static void test_nofind() {
        u16 cap = 100;
        auto map = HashMap<int, int>::get(cap, HEAP); 

        for(u16 i = 0; i < 10'000; ++i) {
            map.insert_cpy(i, i);
        }

        int *found = map.find_cpy(10'001);
        int *null = nullptr;
		TEST_EQ("Not_Found", found, null, false);

        // End
        map.heap_kill();
    }
    static void run_tests() {
        TEST_MODULE_BEGIN("HashMapModule1", true, false);
        test_cpy();
        test_ptr();
        test_find_cpy();
        test_find_ptr();
        test_str();
        test_iter();
		test_u16();
        test_nofind();
        TEST_MODULE_END();
    }
#endif 
};

} // namespace Sol
