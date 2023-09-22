#ifndef SOL_GLTF_HPP_INCLUDE_GUARD_
#define SOL_GLTF_HPP_INCLUDE_GUARD_

#include "basic.h"

enum Gltf_Type {
    GLTF_TYPE_NONE           = 0,
    GLTF_TYPE_SCALAR         = 1,
    GLTF_TYPE_VEC2           = 2,
    GLTF_TYPE_VEC3           = 3,
    GLTF_TYPE_VEC4           = 4,
    GLTF_TYPE_MAT2           = 5,
    GLTF_TYPE_MAT3           = 6,
    GLTF_TYPE_MAT4           = 7,
    GLTF_TYPE_BYTE           = 5120,
    GLTF_TYPE_UNSIGNED_BYTE  = 5121,
    GLTF_TYPE_SHORT          = 5122,
    GLTF_TYPE_UNSIGNED_SHORT = 5123,
    GLTF_TYPE_UNSIGNED_INT   = 5125,
    GLTF_TYPE_FLOAT          = 5126,
};

struct Gltf_Accessor { // 68 bytes - ewww it was 64 before i remebered sparse count 
    Gltf_Type type;   // (maybe the enum is less than 32bit acutally);
    Gltf_Type component_type;
    Gltf_Type indices_component_type;
    int indices_buffer_view;
    int values_buffer_view;
    int indices_byte_offset;
    int values_byte_offset;
    int buffer_view;
    int byte_offset;
    int normalized;
    int count;
    int sparse_count;

    // 'size' field is the total size of the Accessor struct accounting for the number of floats in the float
    // arrays: the float arrays are allocated in memory immediately following the main struct. 
    int stride;
    float *max;
    float *min;
};

struct Gltf {
    int accessor_count;
    Gltf_Accessor *accessors;
};
Gltf parse_gltf(const char *file_name);

#endif // include guard
