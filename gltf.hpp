#ifndef SOL_GLTF_HPP_INCLUDE_GUARD_
#define SOL_GLTF_HPP_INCLUDE_GUARD_

#include "basic.h"

enum Gltf_Type {
    NONE   = 0,
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

struct Gltf_Accessor {
    Gltf_Type type;
    Gltf_Type component_type;
    Gltf_Type sparse_indices_component_type;
    int indices_buffer_view;
    int values_buffer_view;
    int indices_byte_offset;
    int values_byte_offset;
    int buffer_view;
    int byte_offset;
    int count;
    int normalized;
    float max[16];
    float min[16];
};
struct Gltf {
    int accessor_count;
    Gltf_Accessor *accessors;
};
Gltf parse_gltf(const char *file_name);

#endif // include guard
