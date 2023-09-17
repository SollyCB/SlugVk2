#ifndef GLTF_HPP_INCLUDE_GUARD_
#define GLTF_HPP_INCLUDE_GUARD_

#include "external/nlohmann/json.hpp"
#include "basic.h"

// @Note I hate nlohmann json lib. Just from the README: "what if json was in modern c++"
// Json already sucks. I wouldnt really want it anywhere. Coupled with so-called "modern c++"... *shudder*
// I only use it here as I am not going to waste my time writing a json loader just so that I can never use
// Json: gltf files will be loaded, converted to a struct, and then baked as the serialized struct data. 
// I do not need a good json parser to do this. As the point is to eject the json shit asap!


// @Todo lots of these can take extensions. But I do not know what extensions I will ever yet want to support, 
// so so far none are accounted for...


typedef nlohmann::json Json;

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
struct Gltf_Sparse_Accessor_Indices {
    int buffer_view;
    int byte_offset;
    Gltf_Type component_type;
};
struct Gltf_Sparse_Accessor_Values {
    int buffer_view;
    int byte_offset;
};
struct Gltf_Accessor {
    float     *max;
    float     *min;

    int       buffer_view;
    int       offset;
    int       count;
    int       normalized; // Just a big bool, keep shit aligned

    int       sparse_count;
    int       sparse_indices_buffer_view;
    int       sparse_indices_offset;
    int       sparse_values_buffer_view;
    int       sparse_values_offset;

    Gltf_Type type;
    Gltf_Type component_type;
    Gltf_Type sparse_component_type;
};
// This applies to all functions in this file with this signature...
//     Here I use a reference cos this is a dumb lib that uses 'modern c++'. 
//     I would rather use a pointer to be explicit about what the function wants to do
//     but that wont play nice with the lib...
void fill_accessor(Gltf_Accessor *accessor, Json &json);

struct Gltf {
    int accessor_count;
    Gltf_Accessor *accessors;
};

Gltf create_gltf(const char *gltf_filename);
void destroy_gltf(Gltf *gltf);

#endif // include guard
