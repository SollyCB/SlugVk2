#ifndef SOL_GLTF2_HPP_INCLUDE_GUARD_
#define SOL_GLTF2_HPP_INCLUDE_GUARD_

#include "basic.h"
#include "file.hpp"

enum Gltf_Type {
    SCALAR           = 1,
    VEC2             = 2,
    VEC3             = 3,
    VEC4             = 4,
    MAT2             = 4,
    MAT3             = 9,
    MAT4             = 16,
    BYTE             = 5120,
    UNSIGNED_BYTE    = 5121,
    SHORT            = 5122,
    UNSIGNED_SHORT   = 5123,
    UNSIGNED_INT     = 5125,
    FLOAT            = 5126,
};

struct Gltf_Accessor {
    float max[16];
    float min[16];
    Gltf_Type component_type;
};
struct Gltf {
    int accessor_count;
    Gltf_Accessor *accessors;
};

void parse_accessors(Gltf *gltf, const u8 *data, u64 *offset);
Gltf create_gltf(const char* filename);

#endif
