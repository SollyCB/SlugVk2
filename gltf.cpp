#include "gltf.hpp"
#include "file.hpp"

/*
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

// parse object
// parse array
// parse float
// parse int

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

void parse_gltf(const char *file_name, Gltf *gltf) {
    u64 size;
    const char *data = (const char*)file_read_char_heap(file_name, &size);
    
    u64 offset = 0;
    skip_to_char(data, &offset, '"');

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
