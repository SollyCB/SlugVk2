#include "gltf2.hpp"

enum Gltf_Key {
    GLTF_KEY_INVALID = 0,
    GLTF_KEY_ACCESSORS = 1,
    GLTF_KEY_BUFFER_VIEW = 2,
};

bool cmp_str(const char *a, const char *b, int len) {
    for(int i = 0; i < len; ++i)
        if (a[i] != b[i])
            return false;
    return true;
}
int str_len(const char* str) {
    int i = 0;
    while(str[i] != '\0')
        i++;
    return i;
}

Gltf_Key find_key(const char *data, u64 *offset) {
    while(data[*offset] != '"')
        *offset += 1;

    *offset += 1;
    if (cmp_str(data + *offset, "accessors", str_len("accessors")))
        return GLTF_KEY_ACCESSORS;
    else
        ASSERT(false, "invalid key");

    return GLTF_KEY_INVALID;
}

void skip_to_char(const char *data, u64 *offset, char c) {
    while(data[*offset] != c)
        *offset += 1;
}

void parse_accessors(Gltf *gltf, const char *data, u64 *offset) {
    gltf->accessor_count = 0;
    skip_to_char(data, offset, '{');
    *offset += 1;
    int depth = 1;
    while(data[*offset] != ']' && depth != 0) {
        if (data[*offset] == '}' && depth == 1)
            gltf->accessor_count++;

        if (data[*offset] == '{')
            depth++;
        if (data[*offset] == '}')
            depth--;

        *offset += 1;
    }
    gltf->accessors = (Gltf_Accessor*)memory_allocate_temp(sizeof(Gltf_Accessor) * gltf->accessor_count, 8);
}

Gltf create_gltf(const char* filename) {
    u64 size;    
    const char *data = (const char*)file_read_char_heap(filename, &size);
    u64 offset = 0;

    Gltf gltf;
    Gltf_Key key = find_key(data, &offset);

    switch(key) {
    case GLTF_KEY_ACCESSORS:
    {
        parse_accessors(&gltf, data, &offset);
        break;
    }
    default:
        ASSERT(false, "Invalid Key In Switch");
    } // switch key

    return gltf;
}
