
/*
    Final Solution:
        Loop the file once, parse as I go. No need to loop once to tokenize or get other info. Memory access
        and branching will never be that clean for this task regardless of the method, and since I expect that
        these files can get pretty large, only needing one pass seems like it would be fastest.
*/


/* ** Old Note See Above **
    After working on the problem more, I hav realised that a json parser would be very difficult. It would not be 
    hard to serialize the text data into a big block of mem with pointers into that memory mapped to keys. 
    Just iterate through keys or do a hash look up to find the matching key, deref its pointer to find its data.
    By just knowing what type of data the key is pointing to, it is trivial to interpret the data on the end of the
    pointer. 

    However I am going to use my original plan but just in one loop. Because this is much faster than the other option.
    To serialize all the text data into a memory block meaningful for C++ would be a waste of time, because I would
    then have to turn that memory block into each of the types of structs which meaningfully map to my application.
    This process is easier with the memory block than just the text data, but it would be interesting work to make the
    parser/serialiser, then tiresome boring work to turn that into Gltf_<> structs. It would be slow, as it is
    essentially two programs, one of which will incur minor cache minsses (the data will be parsed in order, but
    I will still have to jump around in look ups). As such I will turn the data into Gltf_<> structs straight from
    the text. This will be more difficult work, as a general task becomes more specific (each gltf property becomes
    its own task to serialize straight from text, rather than text being largely general (objects, arrays, numbers)
    and then specific to create from the serialized block).

    This will be much faster: I just pass through the data once, all in order. And at the end I have a bunch
    of C++ structs all mapped to gltf properties. Cool. Let's begin!
*/

/* ** Old Note, See Above **
    I am unsure how to implement this file. But I think I have come to a working solution:

    I have a working idea:
    
        Make a list of function pointers. This list is ordered by the order of the gltf keys 
        in the file. Then I do not have to branch for each key. I just loop through the file once 
        to get the memory required for each first level type. As I do this, I push the correct function 
        pointers to the array. Then I loop through this list calling each function, which will 
        use the file data in order, keeping it all in cache. 

    e.g.

    ~~~~~~~~~~~~~~~~~~~~

    Array<FnPtr> parse_functions;

    Key key = match_key(<str>);
    get_size_<key>() {
        gltf-><key>_count = ...
        gltf-><key>s = memory_allocate(...)
        parse_functions.push(&parse_<key>())
    }

    char *data;
    u64 file_offset;
    for(int i = 0; i < parse_functions) {
        // start at the beginning of the file again, offset get incremented to point at the start
        // of the next key inside the parse_<key> function
        parse_fucntions[i](data, offset);
    }

    ~~~~~~~~~~~
    
    This way I loop the entire file 2 times, and each time in entirety, so the prefectcher is only 
    interrupted once. I only have to match keys in order to dispatch correct fucntions once, as the
    second pass can just call the functions in order.
*/

#include "gltf.hpp"
#include "file.hpp"
#include "simd.hpp"
#include "builtin_wrappers.h"

static int find_int(const char *data, u64 *offset) {
    while(data[*offset] < 48 || data[*offset] > 57)
        *offset += 1;

    int len = 0;
    while(data[*offset + len] > 48 && data[*offset] < 57)
        len++;

    return len;
}

inline static int match_int(char c) {
    switch(c) {
    case '0':
        return 0;
    case '1':
        return 1;
    case '2':
        return 2;
    case '3':
        return 3;
    case '4':
        return 4;
    case '5':
        return 5;
    case '6':
        return 6;
    case '7':
        return 7;
    case '8':
        return 8;
    case '9':
        return 9;
    default:
        ASSERT(false && "not an int", "");
    }
}

static inline int ascii_to_int(const char *data, u64 *offset) {
    u64 inc = 0;
    if (!simd_skip_to_int(data, &inc, 128))
        ASSERT(false, "Failed to find an integer in search range");

    int accum = 0;
    while(data[inc] >= '0' && data[inc] <= '9') {
        accum *= 10;
        accum += match_int(data[inc]);
        inc++;
    }
    *offset += inc;
    return accum;
}

static float ascii_to_float(const char *data, u64 *offset) {
    u64 inc = 0;
    if (!simd_skip_to_int(data, &inc, Max_u64))
        ASSERT(false, "Failed to find an integer in search range");

    bool neg = data[inc - 1] == '-';
    bool seen_dot = false;
    int after_dot = 0;
    float accum = 0;
    int num;
    while((data[inc] >= '0' && data[inc] <= '9') || data[inc] == '.') {
        if (data[inc] == '.') {
            seen_dot = true;
            inc++;
        }

        if (seen_dot)
            after_dot++;

        num = match_int(data[inc]);
        accum *= 10;
        accum += num;

        inc++; // will point beyond the last int at the end of the loop, no need for +1
    }
    for(int i = 0; i < after_dot; ++i)
        accum /= 10;

    *offset += inc;

    if (neg)
        accum = -accum;

    return accum;
}

// @Note I am surprised that I cannot find an SSE intrinsic to do this.
inline static int pow(int num, int exp) {
    int accum = 1;
    for(int i = 1; i < exp; ++i) {
        accum *= num;
    }
    return accum;
}

inline static int parse_float_array(const char *data, u64 *offset, float *array) {
    int i = 0;
    u64 inc = 0;
    while(simd_find_int_interrupted(data + inc, ']', &inc)) {
        array[i] = ascii_to_float(data + inc, &inc);
        i++;
    }
    *offset += inc + 1; // +1 go beyond the cloasing square bracket (prevent early exit at accessors list level)
    return i;
}

// algorithms end

typedef void (*Gltf_Parser_Func)(Gltf *gltf, const char *data, u64 *offset);

void parse_accessor_sparse(const char *data, u64 *offset, Gltf_Accessor *accessor) {
    u64 inc = 0;
    simd_find_char_interrupted(data + inc, '{', '}', &inc); // find sparse start
    while(simd_find_char_interrupted(data + inc, '"', '}', &inc)) {
        inc++; // go beyond the '"'
        if (simd_strcmp_short(data + inc, "countxxxxxxxxxxx", 11) == 0)  {
            accessor->sparse_count = ascii_to_int(data + inc, &inc);
            continue;
        } else if (simd_strcmp_short(data + inc, "indicesxxxxxxxxx", 9) == 0) {
            simd_find_char_interrupted(data + inc, '{', '}', &inc); // find indices start
            while(simd_find_char_interrupted(data + inc, '"', '}', &inc)) {
                inc++; // go passed the '"'
                if (simd_strcmp_short(data + inc, "bufferViewxxxxxx", 6) == 0) {
                    accessor->indices_buffer_view = ascii_to_int(data + inc, &inc);
                    continue;
                }
                if (simd_strcmp_short(data + inc, "byteOffsetxxxxxx", 6) == 0) {
                    accessor->indices_byte_offset = ascii_to_int(data + inc, &inc);
                    continue;
                }
                if (simd_strcmp_short(data + inc, "componentTypexxx", 3) == 0) {
                    accessor->indices_component_type = (Gltf_Type)ascii_to_int(data + inc, &inc);
                    continue;
                }
            }
            simd_find_char_interrupted(data + inc, '}', '{', &inc); // find indices end
            inc++; // go beyond
            continue;
        } else if (simd_strcmp_short(data + inc, "valuesxxxxxxxxx", 10) == 0) {
            simd_find_char_interrupted(data + inc, '{', '}', &inc); // find values start
            while(simd_find_char_interrupted(data + inc, '"', '}', &inc)) {
                inc++; // go passed the '"'
                if (simd_strcmp_short(data + inc, "bufferViewxxxxxx", 6) == 0) {
                    accessor->values_buffer_view = ascii_to_int(data + inc, &inc);
                    continue;
                }
                if (simd_strcmp_short(data + inc, "byteOffsetxxxxxx", 6) == 0) {
                    accessor->values_byte_offset = ascii_to_int(data + inc, &inc);
                    continue;
                }
            }
            simd_find_char_interrupted(data + inc, '}', '{', &inc); // find indices end
            inc++; // go beyond
            continue;
        }
    }
    *offset += inc + 1; // +1 go beyond the last curly brace in sparse object
}

Gltf_Accessor* parse_accessors(const char *data, u64 *offset, int *accessor_count) {
    /*Key_Pad keys[] = {
        {"bufferViewxxxxxx",  6},
        {"byteOffsetxxxxxx",  6},
        {"componentTypexxx",  3},
        {"countxxxxxxxxxxx", 11},
        {"maxxxxxxxxxxxxxx", 13},
        {"minxxxxxxxxxxxxx", 13},
        {"typexxxxxxxxxxxx", 12},
        {"sparsexxxxxxxxxx", 10},
    };
    int key_count = 8; */


    u64 inc = 0; // track position in file
    simd_skip_passed_char(data, &inc, '[', Max_u64);

    int count = 0; // accessor count
    float max[16];
    float min[16];
    int min_max_len;
    int temp;
    bool min_found;
    bool max_found;
    Gltf_Accessor *accessor; 

    Gltf_Accessor *ret = (Gltf_Accessor*)memory_allocate_temp(0, 8);

    // This loop searches at the list level, stopping if it finds a closing square bracket before it finds
    // an opening curly brace - (all closing square brackets below accessor list level will be skipped by 
    // the inner loop that searches objects for keys)
    while(simd_find_char_interrupted(data + inc, '{', ']', &inc)) {
        count++; // increment accessor count

        // Temp allocation made for every accessor struct. Keeps shit packed, linear allocators are fast...
        accessor = (Gltf_Accessor*)memory_allocate_temp(sizeof(Gltf_Accessor), 8);
        accessor->indices_component_type = GLTF_TYPE_NONE;
        accessor->type = GLTF_TYPE_NONE;
        min_max_len = 0;
        min_found = false;
        max_found = false;

        // This loop searches objects for keys, it stops if it finds a closing brace before a key.
        // Curly braces not at the accessor object level (such as accessor.sparse) are skipped inside the loop
        while (simd_find_char_interrupted(data + inc, '"', '}', &inc)) {
            inc++; // go beyond the '"' found by find_char_inter...
            //simd_skip_passed_char(data + inc, &inc, '"', Max_u64); // skip to beginning of key
            
            //
            // I do not like all these branch misses, but I cant see a better way. Even if I make a system 
            // to remove options if they have already been chosen, there would still be at least one branch
            // with no improvement in predictability... (I think)
            //
            // ...I think this is just how parsing text goes...
            //

            // match keys to parse methods
            if (simd_strcmp_short(data + inc, "bufferViewxxxxxx",  6) == 0) {
                accessor->buffer_view = ascii_to_int(data + inc, &inc);
                continue; // go to next key
            }
            if (simd_strcmp_short(data + inc, "byteOffsetxxxxxx",  6) == 0) {
                accessor->byte_offset = ascii_to_int(data + inc, &inc);
                continue; // go to next key
            }
            if (simd_strcmp_short(data + inc, "countxxxxxxxxxxx", 11) == 0) {
                accessor->count = ascii_to_int(data + inc, &inc);
                continue; // go to next key
            }
            if (simd_strcmp_short(data + inc, "componentTypexxx",  3) == 0) {
                accessor->component_type = (Gltf_Type)ascii_to_int(data + inc, &inc);
                continue; // go to next key
            }

            // match type key to a Gltf_Type
            if (simd_strcmp_short(data + inc, "typexxxxxxxxxxxx", 12) == 0) {

                simd_skip_passed_char(data + inc, &inc, '"', Max_u64); // skip to end of "type" key
                simd_skip_passed_char(data + inc, &inc, '"', Max_u64); // skip to beginning of "type" value

                // Why are these types stored as strings?? Why are 'componentType's castable integers, 
                // but these are strings?? This file format is a little insane, there must be a better way 
                // to store this data, surely...???
                if (simd_strcmp_short(data + inc, "SCALARxxxxxxxxxx", 10) == 0)
                    accessor->type = GLTF_TYPE_SCALAR;
                else if (simd_strcmp_short(data + inc, "VEC2xxxxxxxxxxxx", 12) == 0)
                     accessor->type = GLTF_TYPE_VEC2;
                else if (simd_strcmp_short(data + inc, "VEC3xxxxxxxxxxxx", 12) == 0)
                     accessor->type = GLTF_TYPE_VEC3;
                else if (simd_strcmp_short(data + inc, "VEC4xxxxxxxxxxxx", 12) == 0)
                     accessor->type = GLTF_TYPE_VEC4;
                else if (simd_strcmp_short(data + inc, "MAT2xxxxxxxxxxxx", 12) == 0)
                     accessor->type = GLTF_TYPE_MAT2;
                else if (simd_strcmp_short(data + inc, "MAT3xxxxxxxxxxxx", 12) == 0)
                     accessor->type = GLTF_TYPE_MAT3;
                else if (simd_strcmp_short(data + inc, "MAT4xxxxxxxxxxxx", 12) == 0)
                    accessor->type = GLTF_TYPE_MAT4;

                simd_skip_passed_char(data + inc, &inc, '"', Max_u64); // skip passed the value string
                continue; // go to next key
            }

            // why the fuck are bools represented as ascii strings hahhahahahah wtf!!!
            // just use 1 or 0!!! Its just wasting space! Human readable, but so much dumber to parse!!!
            if (simd_strcmp_short(data + inc, "normalizedxxxxxx", 6) == 0) {
                simd_skip_passed_char(data + inc, &inc, ':', Max_u64);
                simd_skip_whitespace(data + inc, &inc); // go the beginning of 'true' || 'false' ascii string
                if (simd_strcmp_short(data + inc, "truexxxxxxxxxxxx", 12) == 0) {
                    accessor->normalized = 1;
                    inc += 5; // go passed the 'true' in the file (no quotation marks to skip)
                }
                else {
                    accessor->normalized = 0;
                    inc += 6; // go passed the 'false' in the file (no quotation marks to skip)
                }
                
                continue; // go to next key
            }

            // sparse object gets its own function
            if (simd_strcmp_short(data + inc, "sparsexxxxxxxxxx", 10) == 0) {
                parse_accessor_sparse(data + inc, &inc, accessor);
                continue; // go to next key
            }

            // @Lame type can be defined after min max arrays, (which is incredibly inefficient), so this
            // must be handled
            if (simd_strcmp_short(data + inc, "maxxxxxxxxxxxxxx", 13) == 0) {
                min_max_len = parse_float_array(data + inc, &inc, max);
                max_found = true;
                continue; // go to next key
            }
            if (simd_strcmp_short(data + inc, "minxxxxxxxxxxxxx", 13) == 0) {
                min_max_len = parse_float_array(data + inc, &inc, min);
                min_found = true;
                continue; // go to next key
            }

        }
        if (min_found && max_found && accessor->type != GLTF_TYPE_NONE) {
            // Have to be careful with memory alignment here: each accessor is allocated individually,
            // so if the temp allocator fragments or packs wrong, reading from the first allocation 
            // and treating the linear allocator as an array would break...
            temp = align(sizeof(float) * min_max_len * 2, 8);
            accessor->max = (float*)memory_allocate_temp(temp, 8); 
            accessor->min = accessor->max + min_max_len;

            memcpy(accessor->max, max, sizeof(float) * min_max_len);
            memcpy(accessor->min, min, sizeof(float) * min_max_len);

            accessor->stride = sizeof(Gltf_Accessor) + (temp);
        } else {
            accessor->stride = sizeof(Gltf_Accessor);
        }
    }
    // handle end...

    // @Note @Memalign
    //
    // ** If accessor info is garbled shit, come back here **
    //
    // The way the list is handled is I make an allocation for every accessor, and then decrement the final
    // accessor pointer to point to the first allocation, because the allocations are made in a linear allocator.
    // I think that this works despite alignment rules, as in indexing by accessor will work fine, because
    // there will not be alignment problems in the linear allocator.
    *accessor_count = count;
    *offset += inc;
    return ret; // point the returned pointer to the beginning of the list
}

Gltf parse_gltf(const char *filename) {
    u64 size;
    const char *data = (const char*)file_read_char_heap_padded(filename, &size, 16);
    Gltf gltf;
    char buf[16];
    u64 offset = 0;

    /*Key_Pad keys[] = {
        {"accessorsxxxxxxx", 7},
    };
    const int key_count = 1; */

    simd_skip_passed_char(data + offset, &offset, '"', size);

    if (simd_strcmp_short(data + offset, "accessorsxxxxxxx", 7) == 0) {
        gltf.accessors = parse_accessors(data + offset, &offset, &gltf.accessor_count);
        return gltf;
    }

    return gltf;
}

/* Below is old code related to this file. I am preserving it because it is cool code imo, does some cool things... */

/* **** Code example for calculating the length of json arrays with simd *****

// string assumed 16 len
inline static u16 simd_match_char(const char *string, char c) {
    __m128i a = _mm_loadu_si128((__m128i*)string);
    __m128i b = _mm_set1_epi8(c);
    a = _mm_cmpeq_epi8(a, b);
    return _mm_movemask_epi8(a);
}

inline static int pop_count(u16 mask) {
    return __builtin_popcount(mask);
}
inline static int ctz(u16 mask) {
    return __builtin_ctz(mask);
}
inline static int clzs(u16 mask) {
    return __builtin_clzs(mask);
}

int resolve_depth(u16 mask1, u16 mask2, int current_depth) {
    u8 tz;
    int open;
    while(mask1) {
        tz = ctz(mask1); 
        open = pop_count(mask2 << (16 - tz));

        current_depth -= 1;
        if (current_depth + open == 0)
            return tz;

        mask1 ^= 1 << tz;
    }
    return -1;
}

int get_obj_array_len(const char *string, u64 *offset) {
    u16 mask1;
    u16 mask2;
    int array_depth = 0;
    int ret;
    while(true) {
        mask1 = simd_match_char(string + *offset, ']');
        mask2 = simd_match_char(string + *offset, '[');
        if (array_depth - pop_count(mask1) <= 0) {
            ret = resolve_depth(mask1, mask2, array_depth);
            if(ret != -1)
                return *offset + ret;
        }
        array_depth += pop_count(mask2) - pop_count(mask1);
        *offset += 16;
    }
}
int main() {
    const char *d = "[[[[xxxxxxxxxxxxx]]]xxxxxxxxxxx]";

    u64 offset = 0;
    int x = get_obj_array_len(d, &offset);
    printf("%i", x);

    return 0;
}
*/

/* 
    ** Template Key Pad **

    Key_Pad keys[] = {
        {"bufferViewxxxxxx",  6},
        {"byteOffsetxxxxxx",  6},
        {"componentTypexxx",  3},
        {"countxxxxxxxxxxx", 11},
        {"maxxxxxxxxxxxxxx", 13},
        {"minxxxxxxxxxxxxx", 13},
        {"typexxxxxxxxxxxx", 12},
        {"sparsexxxxxxxxxx", 10},
    };
    int key_count = 8; 


// Old but imo cool code, so I am preserving it
int resolve_depth(u16 mask1, u16 mask2, int current_depth, int *results) {
    u8 tz;
    int open;
    int index = 0;
    while(mask1) {
        tz = count_trailing_zeros_u16(mask1); 
        open = pop_count16(mask2 << (16 - tz));

        current_depth -= 1;
        if (current_depth + open == 0) {
            results[index] = tz; 
            ++index;
        }

        mask1 ^= 1 << tz;
    }
    return index;
}

int parse_object_array_len(const char *string, u64 *offset, u64 *object_offsets) {
    u64 inc = 0;
    int array_depth  = 0;
    int object_depth = 0;
    int object_count = 0;
    int rd;
    int rds[8];
    u16 mask1;
    u16 mask2;
    u16 temp_count;
    while(true) {
        // check array
        mask1 = simd_match_char(string + inc, ']');
        mask2 = simd_match_char(string + inc, '[');
        if (array_depth - pop_count16(mask1) <= 0) {
            rd = resolve_depth(mask1, mask2, array_depth, rds);
            if(rd != 0) {
                // check object within the end of the array
                mask1 = simd_match_char(string + inc, '}') << (16 - rds[0]);
                mask2 = simd_match_char(string + inc, '{') << (16 - rds[0]);
                if (object_depth - pop_count16(mask1) <= 0) {
                    temp_count = object_count;
                    object_count += resolve_depth(mask1, mask2, object_depth, rds);
                    for(int i = temp_count; i < object_count; ++i)
                        object_offsets[i] = inc + rds[i - temp_count];
                }
                inc += rds[0];
                *offset += inc; // deref offset only once just to keep cache as clean as possible
                return object_count;
            }
        }
        array_depth += pop_count16(mask2) - pop_count16(mask1);

        // check object
        mask1 = simd_match_char(string + inc, '}');
        mask2 = simd_match_char(string + inc, '{');
        if (object_depth - pop_count16(mask1) <= 0) {
            temp_count = object_count;
            object_count += resolve_depth(mask1, mask2, object_depth, rds);
            for(int i = temp_count; i < object_count; ++i)
                object_offsets[i] = inc + rds[i - temp_count];
        }
        object_depth += pop_count16(mask2) - pop_count16(mask1);

        inc += 16;
    }
}
enum Gltf_Key {
    GLTF_KEY_BUFFER_VIEW,
    GLTF_KEY_BYTE_OFFSET,
    GLTF_KEY_COMPONENT_TYPE,
    GLTF_KEY_COUNT,
    GLTF_KEY_MAX,
    GLTF_KEY_MIN,
    GLTF_KEY_TYPE,
    GLTF_KEY_SPARSE,
    GLTF_KEY_INDICES,
    GLTF_KEY_VALUES,
};
// increments beyond end char
void collect_string(const char *data, u64 *offset, char c, char *buf) {
    int i = 0;
    u64 inc = 0;
    while(data[inc] != c) {
        buf[i] = data[inc];
        ++inc;
        ++i;
    }
    buf[i] = '\0';
    *offset += inc + 1;
}


// string must be assumed 16 in len
static int find_string_in_list(const char *string, const Key_Pad *keys, int count) {
    for(int i = 0; i < count; ++i) {
        if(simd_strcmp_short(string, keys[i].key, keys[i].padding) == 0)
            return i;
    }
    return -1;
}

struct Key_Pad {
    const char *key;
    u8 padding;
};

*/
