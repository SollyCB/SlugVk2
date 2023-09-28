#ifndef SOL_GLTF_HPP_INCLUDE_GUARD_
#define SOL_GLTF_HPP_INCLUDE_GUARD_

#include "basic.h"
#include "string.hpp"
#include "math.hpp"

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
    // align this struct to 8 bytes, saves having to use a stride field, instead can just use
    // regular indexing (because the allocation in the linear allocator is aligned to 8 bytes)
    char pad[4];
};
struct Gltf_Animation_Sampler {
    int input;
    int output;
    Gltf_Animation_Interp interp;
    // align this struct to 8 bytes, saves having to use a stride field, instead can just use
    // regular indexing (because the allocation in the linear allocator is aligned to 8 bytes)
    char pad[4];
};
struct Gltf_Animation {
    int stride;

    // @AccessPattern I wonder if there is a nice way to pack these. Without use case, I dont know if interleaving
    // might be useful. For now it seems not to be.
    int channel_count;
    int sampler_count;
    Gltf_Animation_Channel *channels;
    Gltf_Animation_Sampler *samplers;
};

struct Gltf_Buffer {
    int stride; // accounts for the length of the uri string
    int byte_length;
    char *uri;
};

enum Gltf_Buffer_Type {
    GLTF_BUFFER_TYPE_ARRAY_BUFFER         = 34962,
    GLTF_BUFFER_TYPE_ELEMENT_ARRAY_BUFFER = 34963,
};
struct Gltf_Buffer_View {
    int stride;
    int buffer;
    int byte_offset;
    int byte_length;
    int byte_stride;
    Gltf_Buffer_Type buffer_type;
};

struct Gltf_Camera {
    int stride;
    int ortho; // @BoolsInStructs int for bool, alignment
    float znear;
    float zfar;
    float x_factor;
    float y_factor;
};

struct Gltf_Image {
    int stride;
    int jpeg; // @BoolsInStructs int for bool, alignment
    int buffer_view;
    char *uri;
};

enum Gltf_Alpha_Mode {
    GLTF_ALPHA_MODE_OPAQUE = 0,
    GLTF_ALPHA_MODE_MASK   = 1,
    GLTF_ALPHA_MODE_BLEND  = 2,
};
struct Gltf_Material {
    int stride;

    // pbr_metallic_roughness
    float base_color_factor[4] = {1, 1, 1, 1};
    float metallic_factor      = 1;
    float roughness_factor     = 1;
    int base_color_texture_index;
    int base_color_tex_coord;
    int metallic_roughness_texture_index;
    int metallic_roughness_tex_coord;

    // normal_texture
    float normal_scale = 1;
    int normal_texture_index;
    int normal_tex_coord;

    // occlusion_texture
    float occlusion_strength = 1;
    int occlusion_texture_index;
    int occlusion_tex_coord;

    // emissive_texture
    float emissive_factor[3] = {0, 0, 0};
    int emissive_texture_index;
    int emissive_tex_coord;

    // alpha
    Gltf_Alpha_Mode alpha_mode = GLTF_ALPHA_MODE_OPAQUE;
    float alpha_cutoff = 0.5;
    int double_sided; // @BoolsInStructs big bool
};

enum Gltf_Mesh_Attribute_Type {
    GLTF_MESH_ATTRIBUTE_TYPE_POSITION = 1,
    GLTF_MESH_ATTRIBUTE_TYPE_NORMAL   = 2,
    GLTF_MESH_ATTRIBUTE_TYPE_TANGENT  = 3,
    GLTF_MESH_ATTRIBUTE_TYPE_TEXCOORD = 4,
    GLTF_MESH_ATTRIBUTE_TYPE_COLOR    = 5,
    GLTF_MESH_ATTRIBUTE_TYPE_JOINTS   = 6,
    GLTF_MESH_ATTRIBUTE_TYPE_WEIGHTS  = 7,
};
struct Gltf_Mesh_Attribute {
    Gltf_Mesh_Attribute_Type type;
    int n;
    int accessor_index;
};
struct Gltf_Morph_Target {
    int stride;

    int attribute_count;
    Gltf_Mesh_Attribute *attributes;
};
enum Gltf_Primitive_Topology {
    GLTF_PRIMITIVE_TOPOLOGY_POINTS         = 0,
    GLTF_PRIMITIVE_TOPOLOGY_LINES          = 1,
    GLTF_PRIMITIVE_TOPOLOGY_LINE_LOOP      = 2,
    GLTF_PRIMITIVE_TOPOLOGY_LINE_STRIP     = 3,
    GLTF_PRIMITIVE_TOPOLOGY_TRIANGLES      = 4,
    GLTF_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP = 5,
    GLTF_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN   = 6,
};
struct Gltf_Mesh_Primitive {
    int stride;

    int target_count;
    int attribute_count;
    int indices;
    int material;
    int mode = GLTF_PRIMITIVE_TOPOLOGY_TRIANGLES;

    Gltf_Morph_Target   *targets;
    Gltf_Mesh_Attribute *attributes;
};
struct Gltf_Mesh {
    int stride;

    int primitive_count;
    int weight_count;
    Gltf_Mesh_Primitive *primitives;
    float *weights;
};

struct Gltf_Trs {
    Vec4 rotation    = {0.0, 0.0, 0.0, 1.0};
    Vec3 scale       = {1.0, 1.0, 1.0};
    Vec3 translation = {0.0, 0.0, 0.0};
};
struct Gltf_Node {
    int stride;
    int camera;
    int skin;
    int mesh;
    int child_count;
    int weight_count;

    union {
        Gltf_Trs trs;
        Mat4 matrix;
    };

    int *children;
    float *weights;
};

struct Gltf {
    // Each arrayed field has a 'stride' member, which is the byte count required to reach 
    // the next array member;
    //
    // All strides can be calculated from the other info in the struct, but some of these algorithms 
    // are weird and incur unclear overhead. So for consistency's sake they will just be included, 
    // regardless of the ease with which the strides can be calculated. 
    int accessor_count;
    Gltf_Accessor *accessors;
    int animation_count;
    Gltf_Animation *animations;
    int buffer_count;
    Gltf_Buffer *buffers;
    int buffer_view_count;
    Gltf_Buffer_View *buffer_views;
    int camera_count;
    Gltf_Camera *cameras;
    int image_count;
    Gltf_Image *images;
    int material_count;
    Gltf_Material *materials;
    int mesh_count;
    Gltf_Mesh *meshes;
    int node_count;
    Gltf_Node *nodes;
};
Gltf parse_gltf(const char *file_name);

#if TEST
    void test_gltf();
#endif

#endif // include guard
