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
};
struct Gltf_Animation_Sampler {
    int input;
    int output;
    Gltf_Animation_Interp interp;
};
// @Todo @AccessPatterns
// Come back here when I am actaully loading and using models: should samplers and channels be interleaved?
struct Gltf_Animation {
    int channel_count;
    int sampler_count;
    Gltf_Animation_Channel *channels;
    Gltf_Animation_Sampler *samplers;
};

struct Gltf {
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
