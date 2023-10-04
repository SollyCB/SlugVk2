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
    int file_count;
    char **file_names; // buffer data

    // @Todo find the best way to reference the file data for the buffers, 
    // e.g. just the filename? and then read the file at draw time? or read file earlier
    // and keep in memory?
};

Gpu_Vertex_Input_State renderer_define_vertex_input_state(Gltf_Mesh_Primitive *mesh_primitive, Gltf *model, Renderer_Draw_Info *draw_info);

Gpu_Rasterization_State renderer_define_rasterization_state(Gpu_Polygon_Mode_Flags polygon_mode_flags = GPU_POLYGON_MODE_FILL_BIT, VkCullModeFlags cull_mode_flags = VK_CULL_MODE_NONE); // top bit of cull mode flags indicates clockwise front face or not; a pipeline is compiled for each polygon mode set

Gpu_Fragment_Shader_State renderer_define_fragment_shader_state(Gpu_Fragment_Shader_Flags flags = 0x0, VkCompareOp depth_compare_op = VK_COMPARE_OP_NEVER, float min_depth_bounds = 0, float max_depth_bounds = 100);

// @Todo color blending. The goal here is to have an enum with a bunch of options for typical combinations. 
Gpu_Fragment_Output_State renderer_define_fragment_output_state(Gpu_Blend_Setting blend_setting = GPU_BLEND_SETTING_OPAQUE_FULL_COLOR);

struct Renderer_Create_Shader_Stage_Info {
    VkShaderStageFlagBits stage;
    u64 code_size;
    const u32 *spirv;
};
VkPipelineShaderStageCreateInfo* renderer_create_shader_stages(VkDevice device, int count, Renderer_Create_Shader_Stage_Info *infos);
void renderer_destroy_shader_stages(VkDevice device, int count, VkPipelineShaderStageCreateInfo *stages);

struct Renderer_Create_Pipeline_Info {
    int        subpass;

    // @Todo This is too late in the pipeine for shader grouping...
    int shader_stage_count;
    VkPipelineShaderStageCreateInfo *shader_stages;

    Gpu_Vertex_Input_State    *vertex_input_state;
    Gpu_Rasterization_State   *rasterization_state;
    Gpu_Fragment_Shader_State *fragment_shader_state;
    Gpu_Fragment_Output_State  *fragment_output_state;

    VkPipelineLayout pl_layout;
    VkRenderPass     renderpass;
};
VkPipeline renderer_create_pipeline(Renderer_Create_Pipeline_Info *pl_info);

#endif // include guard
