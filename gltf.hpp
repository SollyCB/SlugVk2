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

struct Gltf_Accessor {
    Gltf_Type type;
    Gltf_Type component_type;
    Gltf_Type indices_component_type;

    int stride;

    int indices_buffer_view;
    int values_buffer_view;
    int indices_byte_offset;
    int values_byte_offset;
    int buffer_view;
    int byte_offset;
    int normalized;
    int count;
    int sparse_count;

    float *max;
    float *min;
};

enum Gltf_Animation_Path {
    GLTF_ANIMATION_PATH_NONE        = 0,
    GLTF_ANIMATION_PATH_TRANSLATION = 1,
    GLTF_ANIMATION_PATH_ROTATION    = 2,
    GLTF_ANIMATION_PATH_SCALE       = 3,
    GLTF_ANIMATION_PATH_WEIGHTS     = 4,
};
enum Gltf_Animation_Interp {
    GLTF_ANIMATION_INTERP_LINEAR,
    GLTF_ANIMATION_INTERP_STEP,
    GLTF_ANIMATION_INTERP_CUBICSPLINE,
};
struct Gltf_Animation_Channel {
    int sampler;
    int target_node;
    Gltf_Animation_Path path;
    char pad[4]; // align to 8 bytes, this makes regular indexing work for this array
};
struct Gltf_Animation_Sampler {
    int input;
    int output;
    Gltf_Animation_Interp interp;
    char pad[4]; // align to 8 bytes, this makes regular indexing work for the array
};
struct Gltf_Animation {
    int stride;

    // @AccessPattern I wonder if there is a nice way to pack these. Without use case, I dont know if interleaving
    // might be useful. For now it seems not to be.
    int channel_count;
    int sampler_count;
    Gltf_Animation_Channel *channels;
    Gltf_Animation_Sampler *samplers;

    char pad[4]; // align to 8 bytes, this helps with array indexing rules
};

struct Gltf {
    // Each arrayed field has a 'stride' member, which is the byte count required to reach 
    // the next array member;
    //
    // Explain: all strides can be calculated from the other info in the struct, but some of these algorithms 
    // are weird and incur unclear overhead. So for consistency's sake they will just be included, 
    // regardless of the ease with which the strides can be calculated. 
    int accessor_count;
    Gltf_Accessor *accessors;
    int animation_count;
    Gltf_Animation *animations;
};
Gltf parse_gltf(const char *file_name);

#if TEST
    void test_gltf();
#endif

#endif // include guard
