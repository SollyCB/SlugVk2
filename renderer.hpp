#ifndef SOL_RENDERER_HPP_INCLUDE_GUARD_
#define SOL_RENDERER_HPP_INCLUDE_GUARD_

#include "basic.h"
#include "gpu.hpp"
#include "gltf.hpp"
#include "string.hpp"

struct Renderer_Draw_Info {
    int draw_count;
    u64 index_buffer_offset;
    u64 vertex_buffer_offsets[4]; // position, normal, tangent, tex_coord_0
};
struct Renderer_Draws {
    int mesh_count;
    int *primitive_counts;
    Renderer_Draw_Info **draw_infos;
};
struct Renderer_Buffer_View {
    u64 byte_length;
    u64 byte_offset;
    void *data;
};
struct Renderer_Model_Resources {
    int buffer_view_count;
    int mesh_count; // The number of integers in 'primitive_counts'
    Renderer_Buffer_View *buffer_views;

    int *primitive_counts;
    Renderer_Draw_Info     **draw_infos; // Heap allocated
    Gpu_Vertex_Input_State **vertex_state_infos; // Temp allocated
};
struct Renderer_Gpu_Allocator_Group {
    Gpu_Linear_Allocator *index_allocator;
    Gpu_Linear_Allocator *vertex_allocator;
    Gpu_Linear_Allocator *uniform_allocator;
};
// Get list of required resources from gltf model
Renderer_Model_Resources renderer_setup_model_resources(
    Gltf *model, Renderer_Gpu_Allocator_Group *allocators);
void renderer_free_model_data(Renderer_Model_Resources *list);
Renderer_Draws renderer_download_model_data(Gltf *model, Renderer_Model_Resources *list);

// Pl_Stage_1
Gpu_Vertex_Input_State renderer_define_vertex_input_state(Gltf_Mesh_Primitive *mesh_primitive, Gltf *model);

// Pl_Stage_2
Gpu_Rasterization_State renderer_define_rasterization_state(Gpu_Polygon_Mode_Flags polygon_mode_flags = GPU_POLYGON_MODE_FILL_BIT, VkCullModeFlags cull_mode_flags = VK_CULL_MODE_FRONT_BIT);

// Pl_Stage_3
struct Renderer_Fragment_Shader_State_Info {
    Gpu_Fragment_Shader_Flags flags = 
        GPU_FRAGMENT_SHADER_DEPTH_TEST_ENABLE_BIT |
        GPU_FRAGMENT_SHADER_DEPTH_WRITE_ENABLE_BIT;
    VkCompareOp depth_compare_op = VK_COMPARE_OP_LESS;
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
