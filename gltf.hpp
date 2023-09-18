#ifndef SOL_GLTF_HPP_INCLUDE_GUARD_
#define SOL_GLTF_HPP_INCLUDE_GUARD_

#include "basic.h"

struct Gltf_Accessor {
    float max[16];
    float min[16];
    int buffer_view;
};
struct Gltf {
    int accessor_count;
    Gltf_Accessor *accessors;
};
void parse_gltf(const char *file_name, Gltf *gltf);

#endif // include guard
