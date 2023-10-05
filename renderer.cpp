#include "renderer.hpp"
#include "gpu.hpp"

int renderer_get_byte_stride(Gltf_Accessor_Format);

VkPipelineShaderStageCreateInfo* renderer_create_shader_stages(VkDevice device, int count, Renderer_Create_Shader_Stage_Info *infos) {
    Create_Vk_Pipeline_Shader_Stage_Info *shader_infos =
        (Create_Vk_Pipeline_Shader_Stage_Info*)memory_allocate_temp(sizeof(Create_Vk_Pipeline_Shader_Stage_Info) * count, 8);

    for(int i = 0; i < count; ++i) {
        shader_infos[i].code_size   = infos[i].code_size;
        shader_infos[i].shader_code = infos[i].spirv;
        shader_infos[i].stage       = infos[i].stage;
    }
    return create_vk_pipeline_shader_stages(device, count, shader_infos);
}
void renderer_destroy_shader_stages(VkDevice device, int count, VkPipelineShaderStageCreateInfo *stages) {
    for(int i = 0; i < count; ++i)
        vkDestroyShaderModule(device, stages[i].module, ALLOCATION_CALLBACKS_VULKAN);
    memory_free_heap(stages);
}
VkPipeline renderer_create_pipeline(VkDevice vk_device, Renderer_Create_Pipeline_Info *info) {
    //
    // @Todo I need to figure out the shader pipeline. Shader stage state seems like
    // something which can be a fixed set. It seems like you would have some set of
    // shaders which work together, and then at load time you can just create state
    // for this set... (This is why I am passing precompiled shader stages to the
    // create_pipeline function in gpu.cpp, I think by this point shader state
    // should be known; shader code for stages shouldnt be being grouped so late...
    //

    VkPipeline pl;
    // @Todo pipeline caching
    create_vk_graphics_pipelines(vk_device, VK_NULL_HANDLE, 1, (Create_Vk_Pipeline_Info*)info, &pl);
    return pl;
}
struct Buffer_Range {
    int byte_offset;
    int byte_length;
    int buffer_view;
    int buffer;
};
Renderer_Resource_List renderer_get_model_resource_list(Gltf *model) {
    // @Todo Animation buffer ranges filtered into uniform buffer allocators.
    // @Todo Images filtered into image allocators.
    // @Todo Node transforms filtered into uniform buffers.
    // @Todo Primitive indices filtered into index buffer allocators.
    // @Todo Primitive vertex filtered into vertex buffer allocators.

}
Gpu_Vertex_Input_State renderer_define_vertex_input_state(
    Gltf_Mesh_Primitive *mesh_primitive, Gltf *model, Renderer_Draw_Info *draw_info) {
    
    Gpu_Vertex_Input_State state = {};

    // Enough memory for each array field in 'state' (6 int pointers for 4 bindings)
    int *memory_block = (int*)memory_allocate_temp(sizeof(int) * 6 * 4, 4);
    state.binding_description_bindings    = memory_block;
    state.binding_description_strides     = memory_block + 4;
    state.attribute_description_locations = memory_block + 8;
    state.attribute_description_bindings  = memory_block + 12;
    state.formats = (VkFormat*)(memory_block + 16);
    state.offsets = (memory_block + 20);

    // @Todo add normals etc.
    // @Todo account for sparse accessors (replace the correct vertices at draw time)
    state.topology = (VkPrimitiveTopology)mesh_primitive->topology;

    // pos, norm, tang, tex_coord_0
    state.input_binding_description_count   = 4;
    state.input_attribute_description_count = 4;
    draw_info->offset_count = 4;
    draw_info->offsets = (int*)memory_allocate_temp(sizeof(int) * draw_info->offset_count, 4);

    // pos
    state.binding_description_bindings  [0] = 0;
    state.attribute_description_bindings[0] = 0;
    // norm
    state.binding_description_bindings  [1] = 1;
    state.attribute_description_bindings[1] = 1;
    // tang
    state.binding_description_bindings  [2] = 2;
    state.attribute_description_bindings[2] = 2;
    // tex_coord_0
    state.binding_description_bindings  [3] = 3;
    state.attribute_description_bindings[3] = 3;

    state.attribute_description_locations[0] = 0; // pos
    state.attribute_description_locations[1] = 1; // norm
    state.attribute_description_locations[2] = 2; // tang
    state.attribute_description_locations[3] = 3; // tex_coord_0

    int position    = mesh_primitive->position;
    int normal      = mesh_primitive->normal;
    int tangent     = mesh_primitive->tangent;
    int tex_coord_0 = mesh_primitive->tex_coord_0;

    // position
    Gltf_Accessor *accessor = (Gltf_Accessor*)((u8*)model->accessors + model->accessor_count[position]);
    state.formats[0] = (VkFormat)accessor->format;
    state.offsets[0] = accessor->byte_offset;

    // normal
    accessor = (Gltf_Accessor*)((u8*)model->accessors + model->accessor_count[normal]);
    state.formats[1] = (VkFormat)accessor->format;
    state.offsets[1] = accessor->byte_offset;

    // tangent
    accessor = (Gltf_Accessor*)((u8*)model->accessors + model->accessor_count[tangent]);
    state.formats[2] = (VkFormat)accessor->format;
    state.offsets[2] = accessor->byte_offset;

    // tex_coord_0
    accessor = (Gltf_Accessor*)((u8*)model->accessors + model->accessor_count[tex_coord_0]);
    state.formats[3] = (VkFormat)accessor->format;
    state.offsets[3] = accessor->byte_offset;

    return state;
}

Gpu_Rasterization_State renderer_define_rasterization_state(Gpu_Polygon_Mode_Flags polygon_mode_flags, VkCullModeFlags cull_mode_flags) {
    Gpu_Rasterization_State state = {};

    switch(polygon_mode_flags) {
        case 0:
            state.polygon_mode_count = 1;
            state.polygon_modes[0] = VK_POLYGON_MODE_FILL;
            break;
        case 1:
            state.polygon_mode_count = 1;
            state.polygon_modes[0] = VK_POLYGON_MODE_FILL;
            break;
        case 2:
            state.polygon_mode_count = 1;
            state.polygon_modes[0] = VK_POLYGON_MODE_LINE;
            break;
        case 4:
            state.polygon_mode_count = 1;
            state.polygon_modes[0] = VK_POLYGON_MODE_POINT;
            break;
        case 3:
            state.polygon_mode_count = 2;
            state.polygon_modes[0] = VK_POLYGON_MODE_FILL;
            state.polygon_modes[1] = VK_POLYGON_MODE_LINE;
            break;
        case 5:
            state.polygon_mode_count = 2;
            state.polygon_modes[0] = VK_POLYGON_MODE_FILL;
            state.polygon_modes[1] = VK_POLYGON_MODE_POINT;
            break;
        case 6:
            state.polygon_mode_count = 2;
            state.polygon_modes[0] = VK_POLYGON_MODE_LINE;
            state.polygon_modes[1] = VK_POLYGON_MODE_POINT;
            break;
        case 7:
            state.polygon_mode_count = 3;
            state.polygon_modes[0] = VK_POLYGON_MODE_FILL;
            state.polygon_modes[1] = VK_POLYGON_MODE_LINE;
            state.polygon_modes[2] = VK_POLYGON_MODE_POINT;
            break;
        default:
            state.polygon_mode_count = 1;
            state.polygon_modes[0] = VK_POLYGON_MODE_FILL;
            ASSERT(false, "This is not a valid set of polygon flags");
            break;
    }
    state.cull_mode  = (VkCullModeFlags)cull_mode_flags;
    state.front_face = (VkFrontFace)(cull_mode_flags >> 7); // top bit stores clockwise bool

    return state; 
}

//
// These last two function will be built out to do more real work when I start using stuff like
// blending and multisampling...
//

Gpu_Fragment_Shader_State renderer_define_fragment_shader_state(Renderer_Fragment_Shader_State_Info *info) {
    Gpu_Fragment_Shader_State state = {};
    state.flags = info->flags;
    state.depth_compare_op = info->depth_compare_op;
    state.sample_count = info->sample_count;
    state.min_depth_bounds = info->min_depth_bounds;
    state.max_depth_bounds = info->max_depth_bounds;

    return state;
}

Gpu_Fragment_Output_State renderer_define_fragment_output_state(Gpu_Blend_Setting blend_setting) {
    Gpu_Fragment_Output_State state = {};

    switch (blend_setting) {
        case GPU_BLEND_SETTING_OPAQUE_FULL_COLOR:
        {
            state.blend_state.blendEnable    = VK_FALSE;
            state.blend_state.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | 
                VK_COLOR_COMPONENT_G_BIT | 
                VK_COLOR_COMPONENT_B_BIT | 
                VK_COLOR_COMPONENT_A_BIT;
            break;
        }
        default:
            state.blend_state.blendEnable    = VK_FALSE;
            state.blend_state.colorWriteMask = 
                VK_COLOR_COMPONENT_R_BIT | 
                VK_COLOR_COMPONENT_G_BIT | 
                VK_COLOR_COMPONENT_B_BIT | 
                VK_COLOR_COMPONENT_A_BIT;
            ASSERT(false, "Not a valid blend setting");
            break;
    }

    return state;
}

// functions like this are such a waste of time to write...
int renderer_get_byte_stride(Gltf_Accessor_Format accessor_format) {
        switch(accessor_format) {
            case GLTF_ACCESSOR_FORMAT_SCALAR_U8:
            case GLTF_ACCESSOR_FORMAT_SCALAR_S8:
                return 1;

            case GLTF_ACCESSOR_FORMAT_VEC2_U8:
            case GLTF_ACCESSOR_FORMAT_VEC2_S8:
                return 2;

            case GLTF_ACCESSOR_FORMAT_VEC3_U8:
            case GLTF_ACCESSOR_FORMAT_VEC3_S8:
                return 3;

            case GLTF_ACCESSOR_FORMAT_VEC4_U8:
            case GLTF_ACCESSOR_FORMAT_VEC4_S8:
                return 4;

            case GLTF_ACCESSOR_FORMAT_SCALAR_U16:
            case GLTF_ACCESSOR_FORMAT_SCALAR_S16:
            case GLTF_ACCESSOR_FORMAT_SCALAR_FLOAT16:
                return 2;

            case GLTF_ACCESSOR_FORMAT_VEC2_U16:
            case GLTF_ACCESSOR_FORMAT_VEC2_S16:
            case GLTF_ACCESSOR_FORMAT_VEC2_FLOAT16:
                return 4;

            case GLTF_ACCESSOR_FORMAT_VEC3_U16:
            case GLTF_ACCESSOR_FORMAT_VEC3_S16:
            case GLTF_ACCESSOR_FORMAT_VEC3_FLOAT16:
                return 6;

            case GLTF_ACCESSOR_FORMAT_VEC4_U16:
            case GLTF_ACCESSOR_FORMAT_VEC4_S16:
            case GLTF_ACCESSOR_FORMAT_VEC4_FLOAT16:
                 return 8;

            case GLTF_ACCESSOR_FORMAT_SCALAR_U32:
            case GLTF_ACCESSOR_FORMAT_SCALAR_S32:
            case GLTF_ACCESSOR_FORMAT_SCALAR_FLOAT32:
                return 4;

            case GLTF_ACCESSOR_FORMAT_VEC2_U32:
            case GLTF_ACCESSOR_FORMAT_VEC2_S32:
            case GLTF_ACCESSOR_FORMAT_VEC2_FLOAT32:
                return 8;

            case GLTF_ACCESSOR_FORMAT_VEC3_U32:
            case GLTF_ACCESSOR_FORMAT_VEC3_S32:
            case GLTF_ACCESSOR_FORMAT_VEC3_FLOAT32:
                return 12;

            case GLTF_ACCESSOR_FORMAT_VEC4_U32:
            case GLTF_ACCESSOR_FORMAT_VEC4_S32:
            case GLTF_ACCESSOR_FORMAT_VEC4_FLOAT32:
                return 16;

            case GLTF_ACCESSOR_FORMAT_MAT2_U8:
            case GLTF_ACCESSOR_FORMAT_MAT2_S8:
                return 4;

            case GLTF_ACCESSOR_FORMAT_MAT3_U8:
            case GLTF_ACCESSOR_FORMAT_MAT3_S8:
                return 9;

            case GLTF_ACCESSOR_FORMAT_MAT4_U8:
            case GLTF_ACCESSOR_FORMAT_MAT4_S8:
                return 16;

            case GLTF_ACCESSOR_FORMAT_MAT2_U16:
            case GLTF_ACCESSOR_FORMAT_MAT2_S16:
            case GLTF_ACCESSOR_FORMAT_MAT2_FLOAT16:
                return 8;

            case GLTF_ACCESSOR_FORMAT_MAT3_U16:
            case GLTF_ACCESSOR_FORMAT_MAT3_S16:
            case GLTF_ACCESSOR_FORMAT_MAT3_FLOAT16:
                return 18;

            case GLTF_ACCESSOR_FORMAT_MAT4_U16:
            case GLTF_ACCESSOR_FORMAT_MAT4_S16:
            case GLTF_ACCESSOR_FORMAT_MAT4_FLOAT16:
                return 32;

            case GLTF_ACCESSOR_FORMAT_MAT2_U32:
            case GLTF_ACCESSOR_FORMAT_MAT2_S32:
            case GLTF_ACCESSOR_FORMAT_MAT2_FLOAT32:
                return 16;

            case GLTF_ACCESSOR_FORMAT_MAT3_U32:
            case GLTF_ACCESSOR_FORMAT_MAT3_S32:
            case GLTF_ACCESSOR_FORMAT_MAT3_FLOAT32:
                return 36;

            case GLTF_ACCESSOR_FORMAT_MAT4_U32:
            case GLTF_ACCESSOR_FORMAT_MAT4_S32:
            case GLTF_ACCESSOR_FORMAT_MAT4_FLOAT32:
                return 64;

            default:
                ASSERT(false, "Invalid Accessor Format");
                return -1;
        }
}
