#ifndef SOL_SPV_HPP_INCLUDE_GUARD_
#define SOL_SPV_HPP_INCLUDE_GUARD_

#include "basic.h"

enum Shader_Stage_Flags {
    SHADER_STAGE_VERTEX_BIT = 0x00000001,
    SHADER_STAGE_TESSELLATION_CONTROL_BIT = 0x00000002,
    SHADER_STAGE_TESSELLATION_EVALUATION_BIT = 0x00000004,
    SHADER_STAGE_GEOMETRY_BIT = 0x00000008,
    SHADER_STAGE_FRAGMENT_BIT = 0x00000010,
    SHADER_STAGE_COMPUTE_BIT = 0x00000020,
    SHADER_STAGE_ALL_GRAPHICS = 0x0000001F,
    SHADER_STAGE_ALL = 0x7FFFFFFF,
    // Provided by VK_KHR_ray_tracing_pipeline
    SHADER_STAGE_RAYGEN_BIT_KHR = 0x00000100,
    // Provided by VK_KHR_ray_tracing_pipeline
    SHADER_STAGE_ANY_HIT_BIT_KHR = 0x00000200,
    // Provided by VK_KHR_ray_tracing_pipeline
    SHADER_STAGE_CLOSEST_HIT_BIT_KHR = 0x00000400,
    // Provided by VK_KHR_ray_tracing_pipeline
    SHADER_STAGE_MISS_BIT_KHR = 0x00000800,
    // Provided by VK_KHR_ray_tracing_pipeline
    SHADER_STAGE_INTERSECTION_BIT_KHR = 0x00001000,
    // Provided by VK_KHR_ray_tracing_pipeline
    SHADER_STAGE_CALLABLE_BIT_KHR = 0x00002000,
    // Provided by VK_EXT_mesh_shader
    SHADER_STAGE_TASK_BIT_EXT = 0x00000040,
    // Provided by VK_EXT_mesh_shader
    SHADER_STAGE_MESH_BIT_EXT = 0x00000080,
    // Provided by VK_HUAWEI_subpass_shading
    SHADER_STAGE_SUBPASS_SHADING_BIT_HUAWEI = 0x00004000,
    // Provided by VK_HUAWEI_cluster_culling_shader
    SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI = 0x00080000,
    // Provided by VK_NV_ray_tracing
    SHADER_STAGE_RAYGEN_BIT_NV = SHADER_STAGE_RAYGEN_BIT_KHR,
    // Provided by VK_NV_ray_tracing
    SHADER_STAGE_ANY_HIT_BIT_NV = SHADER_STAGE_ANY_HIT_BIT_KHR,
    // Provided by VK_NV_ray_tracing
    SHADER_STAGE_CLOSEST_HIT_BIT_NV = SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
    // Provided by VK_NV_ray_tracing
    SHADER_STAGE_MISS_BIT_NV = SHADER_STAGE_MISS_BIT_KHR,
    // Provided by VK_NV_ray_tracing
    SHADER_STAGE_INTERSECTION_BIT_NV = SHADER_STAGE_INTERSECTION_BIT_KHR,
    // Provided by VK_NV_ray_tracing
    SHADER_STAGE_CALLABLE_BIT_NV = SHADER_STAGE_CALLABLE_BIT_KHR,
    // Provided by VK_NV_mesh_shader
    SHADER_STAGE_TASK_BIT_NV = SHADER_STAGE_TASK_BIT_EXT,
    // Provided by VK_NV_mesh_shader
    SHADER_STAGE_MESH_BIT_NV = SHADER_STAGE_MESH_BIT_EXT,
};
enum Descriptor_Type {
    DESCRIPTOR_TYPE_SAMPLER = 0,
    DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 1,
    DESCRIPTOR_TYPE_SAMPLED_IMAGE = 2,
    DESCRIPTOR_TYPE_STORAGE_IMAGE = 3,
    DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER = 4,
    DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER = 5,
    DESCRIPTOR_TYPE_UNIFORM_BUFFER = 6,
    DESCRIPTOR_TYPE_STORAGE_BUFFER = 7,
    DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC = 8,
    DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC = 9,
    DESCRIPTOR_TYPE_INPUT_ATTACHMENT = 10,
    // Provided by VK_VERSION_1_3
    DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK = 1000138000,
    // Provided by VK_KHR_acceleration_structure
    DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR = 1000150000,
    // Provided by VK_NV_ray_tracing
    DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV = 1000165000,
    // Provided by VK_QCOM_image_processing
    DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM = 1000440000,
    // Provided by VK_QCOM_image_processing
    DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM = 1000440001,
    // Provided by VK_EXT_mutable_descriptor_type
    DESCRIPTOR_TYPE_MUTABLE_EXT = 1000351000,
    // Provided by VK_EXT_inline_uniform_block
    DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT =
    DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
    // Provided by VK_VALVE_mutable_descriptor_type
    DESCRIPTOR_TYPE_MUTABLE_VALVE = DESCRIPTOR_TYPE_MUTABLE_EXT,
};

// @Packing these can be packed better probably
struct Descriptor_Set_Layout_Binding_Info {
    u16 binding;
    u16 descriptor_count;
    Descriptor_Type descriptor_type;
};
struct Descriptor_Set_Layout_Info {
    u16 binding_count;
    Descriptor_Set_Layout_Binding_Info *binding_infos;
};
struct Parsed_Spirv {
    Shader_Stage_Flags stage;
    u32 group_count;
    Descriptor_Set_Layout_Info *layout_infos;
};

Parsed_Spirv parse_spirv(u64 byte_count, const u32 *spirv);

#endif // include guard
