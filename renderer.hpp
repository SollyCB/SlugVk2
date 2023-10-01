#ifndef SOL_RENDERER_HPP_INCLUDE_GUARD_
#define SOL_RENDERER_HPP_INCLUDE_GUARD_

#include "basic.h"
#include "gpu.hpp"
#include "gltf.hpp"
#include "string.hpp"

//
// This file calls the gpu resource creation functions defined in gpu.hpp
//

// ***Notes on file*** (sort of brief planning...)
// 
// Stuff to need:
//     Arrays of resources that need transitioning.
//     Linear allocator on the gpu for each frame.
//     Pools of sync objects
//     Handling out of memory etc type stuff?

// @Todo @Note later I want these to exist in a pool of memory, and when I have enough
// draw info buffered, we dispatch draw calls.
struct Renderer_Draw_Info {
    int draw_count;
    int offset_count;
    int *offsets; // offsets per binding into buffer data

    // @Todo Move away from using uri style file names (as in the gltf files). Instead,
    // keep a list of all resource files and store indices into that list.
    // Group the list by resources which are frequently used together.
    char **file_names; // buffer data

    // @Todo find the best way to reference the file data for the buffers, 
    // e.g. just the filename? and then read the file at draw time? or read file earlier
    // and keep in memory?
};

struct Renderer_Vertex_Input_State {
    VkPrimitiveTopology topology;
    int input_binding_description_count;
    int input_attribute_description_count;

    // binding description info
    int *binding_description_bindings;
    int *binding_description_strides;

    // attribute description info
    int *attribute_description_locations;
    int *attribute_description_bindings;
    VkFormat *formats;
};
Renderer_Vertex_Input_State renderer_define_vertex_input_state(Gltf_Mesh_Primitive *mesh_primitive, Gltf *model, Renderer_Draw_Info *draw_info);

enum Renderer_Polygon_Mode_Flag_Bits {
    RENDERER_POLYGON_MODE_FILL_BIT  = 0x01,
    RENDERER_POLYGON_MODE_LINE_BIT  = 0x02,
    RENDERER_POLYGON_MODE_POINT_BIT = 0x04,
};
// @Note Viewport and scissor set dynamically, so just defaults at pipeline compilation. I will try to keep this as the only dynamic state in the engine...
struct Renderer_Rasterization_State {
    int polygon_mode_count;
    VkPolygonMode polygon_modes[3];
    VkCullModeFlags cull_mode;
    VkFrontFace front_face;
};
Renderer_Rasterization_State renderer_define_rasterization_state(u8 polygon_mode_flags, u8 cull_mode_flags); // top bit of cull mode flags indicates clockwise front face or not; a pipeline is compiled for each polygon mode set

enum Renderer_Fragment_Shader_Flag_Bits {
    RENDERER_FRAGMENT_SHADER_DEPTH_TEST_ENABLE_BIT         = 0x01,
    RENDERER_FRAGMENT_SHADER_DEPTH_WRITE_ENABLE_BIT        = 0x02,
    RENDERER_FRAGMENT_SHADER_DEPTH_BOUNDS_TEST_ENABLE_BIT  = 0x04,
};
// @Todo multisampling, what even is it? what are benefits? (Smoothness ik...)
struct Renderer_Fragment_Shader_State {
    // 32bits for alignment
    u32 flags;
    VkCompareOp depth_compare_op;
    float min_depth_bounds;
    float max_depth_bounds;
};
Renderer_Fragment_Shader_State renderer_define_fragment_shader_state(u8 flags, VkCompareOp depth_compare_op, float min_depth_bounds, float max_depth_bounds);

enum Renderer_Blend_Setting {
    RENDERER_BLEND_SETTING_OPAQUE_FULL_COLOR = 0,
};
// @Todo color blending. The goal here is to have an enum with a bunch of options for typical combinations. (Fill out above)
struct Renderer_Fragment_Ouput_State {
    VkPipelineColorBlendAttachmentState blend_state;
};
Renderer_Fragment_Ouput_State renderer_define_fragment_output_state(Renderer_Blend_Setting blend_setting);

struct Renderer_Create_Pipeline_Info {
    int        subpass;

    int        shader_count;
    u64       *shader_code_sizes;
    const u32 **shader_codes;
    VkShaderStageFlagBits *shader_stages;

    Renderer_Vertex_Input_State    *vertex_input_state;
    Renderer_Rasterization_State   *rasterization_state;
    Renderer_Fragment_Shader_State *fragment_shader_state;
    Renderer_Fragment_Ouput_State  *fragment_output_state;

    VkPipelineLayout pl_layout;
    VkRenderPass     renderpass;
};
VkPipeline renderer_create_pipeline(Renderer_Create_Pipeline_Info *pl_info);

#endif // include guard
