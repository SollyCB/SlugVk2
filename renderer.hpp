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
};
enum Renderer_Resource {
    RENDERER_RESOURCE_DEVICE_VERTEX_BUFFER  = 0,
    RENDERER_RESOURCE_DEVICE_INDEX_BUFFER   = 1,
    RENDERER_RESOURCE_DEVICE_UNIFORM_BUFFER = 2,
    RENDERER_RESOURCE_DEVICE_SAMPLED_IMAGE  = 3,
    RENDERER_RESOURCE_DEVICE_STORAGE_BUFFER = 4,
    RENDERER_RESOURCE_DEVICE_STORAGE_IMAGE  = 5,
};
enum Renderer_Resource_View {};
// Counts corresponds to the counts of the above resource types required to draw the model
struct Renderer_Resource_List {
    int counts[6]; 
};
struct Renderer_Resource_View_List {
    int counts[6]; 
};
// Get list of required resources from gltf model
Renderer_Resource_List renderer_get_model_resource_list(Gltf *model);

// Pl_Stage_1
Gpu_Vertex_Input_State renderer_define_vertex_input_state(Gltf_Mesh_Primitive *mesh_primitive, Gltf *model, Renderer_Draw_Info *draw_info);

// Pl_Stage_2
Gpu_Rasterization_State renderer_define_rasterization_state(Gpu_Polygon_Mode_Flags polygon_mode_flags = GPU_POLYGON_MODE_FILL_BIT, VkCullModeFlags cull_mode_flags = VK_CULL_MODE_NONE); // top bit of cull mode flags indicates clockwise front face or not; a pipeline is compiled for each polygon mode set

// Pl_Stage_3
struct Renderer_Fragment_Shader_State_Info {
    Gpu_Fragment_Shader_Flags flags = 0x0;
    VkCompareOp depth_compare_op = VK_COMPARE_OP_NEVER;
    VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
    float min_depth_bounds = 0;
    float max_depth_bounds = 100;
};
Gpu_Fragment_Shader_State renderer_define_fragment_shader_state(Renderer_Fragment_Shader_State_Info *info);

// @Todo color blending. The goal here is to have an enum with a bunch of options for typical combinations. 
// Pl_Stage_4
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

    VkPipelineLayout layout;
    VkRenderPass     renderpass;
};
VkPipeline renderer_create_pipeline(VkDevice vk_device, Renderer_Create_Pipeline_Info *pl_info);

#endif // include guard
