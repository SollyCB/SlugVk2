/*
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

#include "gltf.hpp"
#include "file.hpp"
#include "simd.hpp"
#include "builtin_wrappers.h"

// string assumed 16 len
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

int get_object_array_len(const char *string, u64 *offset) {
    int array_depth  = 0;
    int object_depth = 0;
    int object_count = 0;
    int rd;
    int rds[8];
    u16 mask1;
    u16 mask2;
    while(true) {
        // check array
        mask1 = simd_match_char(string + *offset, ']');
        mask2 = simd_match_char(string + *offset, '[');
        if (array_depth - pop_count16(mask1) <= 0) {
            rd = resolve_depth(mask1, mask2, array_depth, rds);
            if(rd != 0) {
                // check object within the end of the array
                mask1 = simd_match_char(string + *offset, '}') << (16 - rds[0]);
                mask2 = simd_match_char(string + *offset, '{') << (16 - rds[0]);
                if (object_depth - pop_count16(mask1) <= 0) {
                    object_count += resolve_depth(mask1, mask2, object_depth, rds);
                }
                *offset += rds[0];
                return object_count;
            }
        }
        array_depth += pop_count16(mask2) - pop_count16(mask1);

        // check object
        mask1 = simd_match_char(string + *offset, '}');
        mask2 = simd_match_char(string + *offset, '{');
        if (object_depth - pop_count16(mask1) <= 0) {
            object_count += resolve_depth(mask1, mask2, object_depth, rds);
        }
        object_depth += pop_count16(mask2) - pop_count16(mask1);

        *offset += 16;
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
enum Gltf_Type {
    SCALAR = 1,
    VEC2   = 2,
    VEC3   = 3,
    VEC4   = 4,
    MAT2   = 5,
    MAT3   = 6,
    MAT4   = 7,
    BYTE = 5120,
    UNSIGNED_BYTE = 5121,
    SHORT = 5122,
    UNSIGNED_SHORT = 5123,
    UNSIGNED_INT = 5125,
    FLOAT = 5126,
};

void skip_to_char(const char *data, u64 *offset, char c) {
    while(data[*offset] != c)
        *offset += 1;
}

void skip_passed_char(const char *data, u64 *offset, char c) {
    while(data[*offset] != c)
        *offset += 1;
    *offset += 1;
}

// increments beyond end char
void collect_string(const char *data, u64 *offset, char c, char *buf) {
    int i = 0;
    while(data[*offset] != c) {
        buf[i] = data[*offset];
        *offset += 1;
        i++;
    }
    buf[i] = '\0';
    *offset += 1;
}

struct Key_Pad {
    const char *key;
    u8 padding;
};

// string must be assumed 16 in len
static int find_string_in_list(const char *string, const Key_Pad *keys, int count) {
    for(int i = 0; i < count; ++i) {
        if(simd_strcmp_short(string, keys[i].key, keys[i].padding) == 0)
            return i;
    }
    return -1;
}

static int find_int(const char *data, u64 *offset) {
    while(data[*offset] < 48 || data[*offset] > 57)
        *offset += 1;

    int len = 0;
    while(data[*offset + len] > 48 && data[*offset] < 57)
        len++;

    return len;
}

static int match_int(char c) {
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

// @Note I am surprised that I cannot find a SSE intrinsic to do this.
inline static int pow(int num, int exp) {
    int accum = 1;
    for(int i = 1; i < exp; ++i) {
        accum *= num;
    }
    return accum;
}

static int get_int(const char *data, u64 *offset) {
    int len = find_int(data, offset);

    int num;
    int mul;
    int accum = 0;
    for(int i = len; i > 0; --i) {
        num = match_int(data[*offset]);
        mul = pow(10, i);

        accum += mul * num;
        *offset += 1;
    }
    return accum;
}
// algorithms end

typedef void (*Gltf_Parser_Func)(Gltf *gltf, const char *data, u64 *offset);

void parse_accessor(Gltf_Accessor *accessor, const char *data, u64 *offset) {
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

    Gltf_Parser_Func parser_funcs[] = {
        // parse_bufferview
        // ^^ this so small, just do it on the match probably
    };

// @TODO Current task:
//      create list of function pointers to matching keys (see parse gltf function)
//      write these parser functions

    int key_index;
    while(data[*offset] != '}') {
        skip_passed_char(data, offset, '"');

        if (simd_strcmp_short(data + *offset, keys[0].key, keys[0].padding) == 0) {
            accessor->buffer_view = get_int(data, offset);
            println("Buffer View: %u", accessor->buffer_view);
            ASSERT(false, "Break For Now");
        }

    }
}

void parse_accessors(Gltf *gltf, const char *data, u64 *offset) {
    u64 start = *offset;
    gltf->accessor_count = get_object_array_len(data, offset);
    gltf->accessors =
        (Gltf_Accessor*)memory_allocate_temp( sizeof(Gltf_Accessor) * gltf->accessor_count, 8);
    for(int i = 0; i < gltf->accessor_count; ++i) {
        parse_accessor(&gltf->accessors[i], data, &start);
    }
}

Gltf parse_gltf(const char *filename) {
    u64 size;
    const char *data = (const char*)file_read_char_heap(filename, &size);
    Gltf gltf;
    char buf[16];
    u64 offset = 0;

    Key_Pad keys[] = {
        {"accessorsxxxxxxx", 7},
    };
    const int key_count = 1;
    int match_count = 0;

    //void (*gltf_parser_func[])(Gltf *gltf, const char *data, u64 *offset) = {
    Gltf_Parser_Func parser_funcs[] = {
        &parse_accessors,
    };

    skip_passed_char(data, &offset, '"');

    // @Note I could reshuffle the list on each match (swap the found one 
    // with the one closest to the end which hasnt been found), but the list is so short
    // and the branches so unpredictable that who cares...
    int index = find_string_in_list(data + offset, keys, key_count);
    (*parser_funcs[index])(&gltf, data, &offset);

    return gltf;
}

#if 0

*********************************************
enum Gltf_Key {
    GLTF_KEY_INVALID,
    GLTF_KEY_ACCESSORS,
};

int str_len(const char *str, char c) {
    int i = 0;
    while(str[i] != c)
        ++i;
    return i;
}
bool cmp_str(const char *a, const char *b, int len) {
    for(int i = 0; i < len; ++i)
        if (a[i] != b[i])
            return false;
    return true;
}

Gltf_Key match_key(const char *key, int *len) {
    if (cmp_str(key, "accessors", 9)) {
        if (len)
            *len = str_len("accessors", '\0');
        return GLTF_KEY_ACCESSORS;
    }

    ASSERT(false, "Invalid key");
    return GLTF_KEY_INVALID;
}

enum Gltf_Value {
    GLTF_VALUE_INT    = 1,
    GLTF_VALUE_FLOAT  = 2,
    GLTF_VALUE_ARRAY  = 3,
    GLTF_VALUE_OBJECT = 4,
};

void skip_to_char(const char *data, u64 *offset, char c) {
    while(data[*offset] != c)
        *offset += 1;
}

bool look_ahead_for_char(const char *data, char c, char s) {
    for(int i = 0; true; ++i) {
        if (data[i] == c)
            return true;
        if (data[i] != s)
            return false;
    }
}

void skip_white_space(const char *data, u64 *offset) {
    while(data[*offset] == ' ' || data[*offset] == '\n')
        *offset += 1;
}
void skip_useless_chars(const char *data, u64 *offset) {
    while(true) {
        skip_white_space(data, offset);
        switch(data[*offset]) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '-':
        case '.':
        case ',':
            *offset += 1;
            continue;
        case ':':
        {
            *offset += 1;
            skip_white_space(data, offset);
            switch(data[*offset]) {
            case '"':
            {
                *offset += 1;
                skip_to_char(data, offset, '"');
                *offset += 1;
                continue;
            }
            case '[':
            {
                skip_to_char(data, offset, ']');
                *offset += 1;
                continue;
            }
            default:
                continue;
            }
        }
        default:
            return;
        } // switch data[*offset]
    }
}

void collect_string(const char *data, u64 *offset, char *buf) {
    int i = 0;
    while(data[*offset] != '"') {
        buf[i] = data[*offset];
        i++;
        *offset += 1;
    }
    buf[i] = '\0';
}

// This can spit out a list of keys, and a list of values.
Static_Array<Gltf_Key> parse_accessors(Gltf *gltf, const char *data, u64 *offset) {
    skip_to_char(data, offset, '[');
    *offset += 1;

    int depth = 0;
    int key_count = 0;
    char key_buf[16];
    gltf->accessor_count = 0;
    while(true) {
        skip_useless_chars(data, offset);

        if (data[*offset] == '{') {
            if (depth == 0) {
                gltf->accessor_count++;
            }
            *offset += 1;
            depth++;
            continue;
        } else if (data[*offset] == '}') {
            *offset += 1;
            depth--;
            continue;
        }

        if (data[*offset] == '"') {
            *offset += 1;
            collect_string(data, offset, key_buf); 
            key_count++;
            *offset += 1;
            continue;
        }

        if (data[*offset] == ']' && depth == 0)
            break;
    }

    gltf->accessors = (Gltf_Accessor*)memory_allocate_temp(sizeof(Gltf_Accessor) * gltf->accessor_count, 8);

    println("Accessor Count: %u", gltf->accessor_count);
    println("Key Count: %u", key_count);
    //CAP_TO_LEN_STATIC_ARRAY(keys);
    return {};
}

static void match_key(int count, const char **keys, const char *key, void **function_pointers) {
    for(itn
}

void parse_gltf(const char *file_name, Gltf *gltf) {
    u64 size;
    // pad file to not segfault off the end with simd
    const char *data = (const char*)file_read_char_heap_padded(file_name, &size, 16);
    
    u64 offset = 0;
    skip_passed_char(data, &offset, '"');

    offset++;
    int key_len;
    Gltf_Key key = match_key(data + offset, &key_len);
    offset += key_len + 1;

    switch(key) {
    case GLTF_KEY_ACCESSORS:
    {
        parse_accessors(gltf, data, &offset);
        break;
    }
    default:
        ASSERT(false, "Unimplemented Key");
    }
}
#endif
