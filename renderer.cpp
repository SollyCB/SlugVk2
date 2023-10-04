#include "renderer.hpp"
#include "gpu.hpp"

int renderer_get_byte_stride(Gltf_Accessor_Format);

// @Goal. Having to specify the state up front I think is fine, as I can generate
// state for models and then bake that state. So at runtime I have all the state for
// models in whatever configuration prebaked, making pipeline compilation easy. I think.
// so really I would want this code to be in a different file, called "baker.cpp" or smtg,
// which takes gltf files and bakes their state for use in vulkan, and then this state can
// be read at run time. This baking should be pretty easy, as vulkan structs use counts
// and array everywhere, so resetting pointers should be trivial, as everything tells
// you how big it is...

// @Goal draw_infos all kept in a memory pool. The below functions allocate into this pool
// as they read gltf data. When the pool is full, or full enough that dispatching another
// call to the below functions could overflow it, we call draw and empty the pool, ready
// to fill again. input_states etc, use the temp allocator, so all the state info is
// kept contiguous, and if there is extra stuff like for the 'extra_attributes' field, 
// we can just allocate more knowing that the extra stuff is just off the end of the state we
// just defined.

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
}


VkPipeline renderer_create_pipeline(Renderer_Create_Pipeline_Info *info) {
    VkDevice device = get_gpu_instance()->vk_device;

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
    create_vk_graphics_pipelines(device, VK_NULL_HANDLE, 1, (Create_Vk_Pipeline_Info*)info, &pl);
    return pl;
}

Gpu_Vertex_Input_State renderer_define_vertex_input_state(Gltf_Mesh_Primitive *mesh_primitive, Gltf *model, Renderer_Draw_Info *draw_info) {
    
    Gpu_Vertex_Input_State state = {};

    // Enough memory for each array field in 'state'
    int *memory_block = (int*)memory_allocate_temp(sizeof(int) * 4 * 5, 4);
    state.binding_description_bindings    = memory_block;
    state.binding_description_strides     = memory_block + 4;
    state.attribute_description_locations = memory_block + 8;
    state.attribute_description_bindings  = memory_block + 12;
    state.formats = (VkFormat*)(memory_block + 16);

    // @Todo @Incomplete ... This needs to be handled before allocating memory for this 
    // struct? Or keep the basic stuff together and allocate more for this off the end,
    // adding a field to the struct to track how much we added?
    // account for extra attributes
    /*if (mesh_primitive->extra_attribute_count) {
        //state.input_binding_description_count += primitive->extra_attribute_count;
    } */

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

    draw_info->draw_count = accessor->count; // take the number of elements to draw from the 'position' vertex attribute

    char *file_names[4];

    Gltf_Buffer_View *buffer_view = (Gltf_Buffer_View*)((u8*)model->buffer_views + model->buffer_view_count[accessor->buffer_view]);
    draw_info->offsets[0] = accessor->byte_offset + buffer_view->byte_offset;
    Gltf_Buffer *buffer = (Gltf_Buffer*)((u8*)model->buffers + model->buffer_count[buffer_view->buffer]);
    file_names[0] = buffer->uri;

    if (buffer_view->byte_stride) {
        state.binding_description_strides[0] = buffer_view->byte_stride;
    } else {
        state.binding_description_strides[0] = renderer_get_byte_stride(accessor->format);
    }

    // normal
    accessor = (Gltf_Accessor*)((u8*)model->accessors + model->accessor_count[normal]);
    state.formats[1] = (VkFormat)accessor->format;

    buffer_view = (Gltf_Buffer_View*)((u8*)model->buffer_views + model->buffer_view_count[accessor->buffer_view]);
    draw_info->offsets[1] = accessor->byte_offset + buffer_view->byte_offset;
    buffer = (Gltf_Buffer*)((u8*)model->buffers + model->buffer_count[buffer_view->buffer]);
    file_names[1] = buffer->uri;

    if (buffer_view->byte_stride) {
        state.binding_description_strides[1] = buffer_view->byte_stride;
    } else {
        state.binding_description_strides[1] = renderer_get_byte_stride(accessor->format);
    }

    // tangent
    accessor = (Gltf_Accessor*)((u8*)model->accessors + model->accessor_count[tangent]);
    state.formats[2] = (VkFormat)accessor->format;

    buffer_view = (Gltf_Buffer_View*)((u8*)model->buffer_views + model->buffer_view_count[accessor->buffer_view]);
    draw_info->offsets[2] = accessor->byte_offset + buffer_view->byte_offset;
    buffer = (Gltf_Buffer*)((u8*)model->buffers + model->buffer_count[buffer_view->buffer]);
    file_names[2] = buffer->uri;

    if (buffer_view->byte_stride) {
        state.binding_description_strides[2] = buffer_view->byte_stride;
    } else {
        state.binding_description_strides[2] = renderer_get_byte_stride(accessor->format);
    }

    // tex_coord_0
    accessor = (Gltf_Accessor*)((u8*)model->accessors + model->accessor_count[tex_coord_0]);
    state.formats[3] = (VkFormat)accessor->format;

    buffer_view = (Gltf_Buffer_View*)((u8*)model->buffer_views + model->buffer_view_count[accessor->buffer_view]);
    draw_info->offsets[3] = accessor->byte_offset + buffer_view->byte_offset;
    buffer = (Gltf_Buffer*)((u8*)model->buffers + model->buffer_count[buffer_view->buffer]);
    file_names[3] = buffer->uri;

    if (buffer_view->byte_stride) {
        state.binding_description_strides[3] = buffer_view->byte_stride;
    } else {
        state.binding_description_strides[3] = renderer_get_byte_stride(accessor->format);
    }

    //
    // @Ugly This below code is ugly. I dont like it...
    //
    draw_info->file_count = 0;
    draw_info->file_names = (char**)memory_allocate_temp(sizeof(char*) * 4, 8);
    const char *empty = "";
    for(int i = 0; i < 4; ++i)
        draw_info->file_names[i] = (char*)empty;

    bool unique;
    for(int i = 0; i < 4; ++i) {
        unique = true;
        for(int j = 0; j < 4; ++j) {
            if (strcmp(draw_info->file_names[j], file_names[i]) == 0) {
                unique = false;
                break;
            }
        }
        if (!unique)
            continue;
        draw_info->file_names[draw_info->file_count] = file_names[i];
        draw_info->file_count++;
    }

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

// Currently this function really does not have much to do, since multisampling is all 
// defaults for now...
Gpu_Fragment_Shader_State renderer_define_fragment_shader_state(u8 flags, VkCompareOp depth_compare_op, float min_depth_bounds, float max_depth_bounds) {
    Gpu_Fragment_Shader_State state = {};

    state.flags = flags;
    state.depth_compare_op = depth_compare_op;
    state.min_depth_bounds = min_depth_bounds;
    state.max_depth_bounds = max_depth_bounds;

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
