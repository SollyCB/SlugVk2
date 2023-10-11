#include "renderer.hpp"
#include "gpu.hpp"
#include "file.hpp"

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

enum Buffer_Type {
    BT_UNKNOWN = 0,
    BT_VERTEX  = 1,
    BT_INDEX   = 2,
    BT_UNIFORM = 3,
    BT_IMAGE   = 4,
};
// Unimplemented Resource Function todos
    // @Todo Animation buffer ranges filtered into uniform buffer allocators.
    // @Todo Images filtered into image allocators.
    // @Todo Node transforms filtered into uniform buffers.

    // @Todo add sampler and image resource list elements.
    // @Todo add inverse bind matrices resource list elements.
Renderer_Vertex_Attribute_Resources
renderer_setup_vertex_attribute_resources_static_model(Gltf *model, Renderer_Gpu_Allocator_Group *allocators) 
{
    /* Method:

       1. Loop meshes/mesh_primitives to find vertex attribute and index data and mark corresponding
          accessors.
       2. Loop marked accessors, queue buffer allocation in gpu allocator, set pointer in resource list;
          Also mark buffer view as having been queued for allocation.
    */

    int accessor_count    = gltf_accessor_get_count(model);
    int buffer_view_count = gltf_buffer_view_get_count(model);
    int mesh_count        = gltf_mesh_get_count(model);

    Renderer_Vertex_Attribute_Resources ret = {};
    ret.buffer_view_count = buffer_view_count;
    ret.buffer_views = (Renderer_Buffer_View*)memory_allocate_temp(
                            sizeof(Renderer_Buffer_View) * buffer_view_count, 8);
    
    // Put draw information into persistent allocation
    // @Todo Have a separate linear allocator for this data, allocated from the global
    // heap allocator (not std::malloc)
    ret.mesh_count = mesh_count;
    ret.meshes = 
        (Renderer_Mesh*)linear_allocator_allocate(
            allocators->draw_info_allocator, sizeof(Renderer_Mesh) * mesh_count, 8);

    ret.vertex_state_infos = 
        (Gpu_Vertex_Input_State**)memory_allocate_temp(sizeof(u8*) * mesh_count, 8);

    // @Speed @Cache There is A LOT of random access pattern going on here (in the next loop) 
    // (depending on the layout of the gltf model file...). This will all be happening at load 
    // time, so it is better that I take the hit organising the data now rather than later,
    // but at the same time I do want to come back here at some point to work on a better way 
    // of loading these models.

    Gltf_Mesh *mesh = model->meshes;
    Gltf_Mesh_Primitive *primitive;

    int buffer_indices[5];
    Gltf_Accessor *accessors[5];
    Gltf_Buffer_View *buffer_view;

    ASSERT(buffer_view_count <= 64, "Buffer View Mask Too Small");
    u64 buffer_view_mask      = 0x0; // @Note Assumes fewer than 64 buffer views;
    u64 *allocation_offsets   = (u64*)memory_allocate_temp(sizeof(u64) * buffer_view_count, 8);

    // To know which area of the buffer to synchronize
    gpu_make_linear_allocation(allocators->index_allocator,  0, &ret.index_allocation_start);
    gpu_make_linear_allocation(allocators->vertex_allocator, 0, &ret.vertex_allocation_start);

    // I dont like calling heap allocate in a loop at all, but this is not a tight loop anyway,
    // and the data per mesh will still be contiguous in memory... I could make a big allocation
    // and then have each mesh index in, but Idk how much this would help: the data would be
    // more contiguous, but it would still be random access, unless access pattern will be
    // looping the meshes without jumps?? Maybe I can add data to the Gltf struct to say the
    // allocation requirements for all the mesh resources? So that I can make one allocation
    // here and add all the meshes to it in order?...
    // t. I am using a fast
    // allocator anyway, there are never THAT many meshes, but normally lots of primitives,
    // and to use it it would only be a cache miss everytime you begin a new mesh which is not big
    // as you are spending most of the time in the primitives...
    //
    // (The above is all surrounded in a big "I think??" :D)
    //
    for(int i = 0; i < mesh_count; ++i) {
        primitive = mesh->primitives;

        ret.meshes[i].primitive_draw_infos = (Renderer_Draw_Info_Static*)linear_allocator_allocate(
                allocators->draw_info_allocator,
                sizeof(Renderer_Draw_Info_Static) * mesh->primitive_count, 8); 

        ret.vertex_state_infos[i] =
                (Gpu_Vertex_Input_State*)memory_allocate_temp(
                    sizeof(Gpu_Vertex_Input_State) * mesh->primitive_count, 8);

        for(int j = 0; j < mesh->primitive_count; ++j) {

            accessors[0] = gltf_accessor_by_index(model, primitive->indices);
            accessors[1] = gltf_accessor_by_index(model, primitive->position);
            accessors[2] = gltf_accessor_by_index(model, primitive->normal);
            accessors[3] = gltf_accessor_by_index(model, primitive->tangent);
            accessors[4] = gltf_accessor_by_index(model, primitive->tex_coord_0);

            // Fill in draw info
            ret.meshes[i].primitive_draw_infos[j].draw_count          = accessors[0]->count;
            ret.meshes[i].primitive_draw_infos[j].index_buffer_offset = accessors[0]->byte_offset;

            ret.meshes[i].primitive_draw_infos[j].vertex_buffer_offsets[0] = accessors[1]->byte_offset;
            ret.meshes[i].primitive_draw_infos[j].vertex_buffer_offsets[1] = accessors[2]->byte_offset;
            ret.meshes[i].primitive_draw_infos[j].vertex_buffer_offsets[2] = accessors[3]->byte_offset;
            ret.meshes[i].primitive_draw_infos[j].vertex_buffer_offsets[3] = accessors[4]->byte_offset;

            // Less typing
            buffer_indices[0] = accessors[0]->buffer_view;
            buffer_indices[1] = accessors[1]->buffer_view;
            buffer_indices[2] = accessors[2]->buffer_view;
            buffer_indices[3] = accessors[3]->buffer_view;
            buffer_indices[4] = accessors[4]->buffer_view;

            ret.vertex_state_infos[i][j] =
                renderer_define_vertex_input_state_static_model(primitive, model);

            // Check if buffer view has already been queued, if not, get its details and add to queue
            if ((buffer_view_mask & (1 << buffer_indices[0])) == 0) {
                buffer_view = gltf_buffer_view_by_index(model, buffer_indices[0]);

                ret.buffer_views[buffer_indices[0]].byte_length = buffer_view->byte_length;
                ret.buffer_views[buffer_indices[0]].byte_offset = buffer_view->byte_offset;

                ret.buffer_views[buffer_indices[0]].data =
                    gpu_make_linear_allocation(
                        allocators->index_allocator, 
                        ret.buffer_views[buffer_indices[0]].byte_length,
                        &allocation_offsets[buffer_indices[0]]);

                // Point draw infos at the queued allocation
                ret.meshes[i].primitive_draw_infos[j].index_buffer_offset += allocation_offsets[buffer_indices[0]];
            } else {
                // Point draw infos at the queued allocation
                ret.meshes[i].primitive_draw_infos[j].index_buffer_offset += allocation_offsets[buffer_indices[0]];
            }
            if ((buffer_view_mask & (1 << buffer_indices[1])) == 0) {
                buffer_view = gltf_buffer_view_by_index(model, buffer_indices[1]);

                ret.buffer_views[buffer_indices[1]].byte_length = buffer_view->byte_length;
                ret.buffer_views[buffer_indices[1]].byte_offset = buffer_view->byte_offset;

                ret.buffer_views[buffer_indices[1]].data =
                    gpu_make_linear_allocation(
                        allocators->vertex_allocator, 
                        ret.buffer_views[buffer_indices[1]].byte_length,
                        &allocation_offsets[buffer_indices[1]]);

                // Point draw infos at the queued 'position' vertex allocation
                ret.meshes[i].primitive_draw_infos[j].vertex_buffer_offsets[0] += allocation_offsets[buffer_indices[1]];
            } else {
                // Point draw infos at the queued 'position' vertex allocation
                ret.meshes[i].primitive_draw_infos[j].vertex_buffer_offsets[0] += allocation_offsets[buffer_indices[1]];
            }
            if ((buffer_view_mask & (1 << buffer_indices[2])) == 0) {
                buffer_view = gltf_buffer_view_by_index(model, buffer_indices[2]);

                ret.buffer_views[buffer_indices[2]].byte_length = buffer_view->byte_length;
                ret.buffer_views[buffer_indices[2]].byte_offset = buffer_view->byte_offset;

                ret.buffer_views[buffer_indices[2]].data =
                    gpu_make_linear_allocation(
                        allocators->vertex_allocator, 
                        ret.buffer_views[buffer_indices[2]].byte_length,
                        &allocation_offsets[buffer_indices[2]]);

                // Point draw infos at the queued 'normal' vertex allocation
                ret.meshes[i].primitive_draw_infos[j].vertex_buffer_offsets[1] += allocation_offsets[buffer_indices[2]];
            } else {
                // Point draw infos at the queued 'normal' vertex allocation
                ret.meshes[i].primitive_draw_infos[j].vertex_buffer_offsets[1] += allocation_offsets[buffer_indices[2]];
            }
            if ((buffer_view_mask & (1 << buffer_indices[3])) == 0) {
                buffer_view = gltf_buffer_view_by_index(model, buffer_indices[3]);

                ret.buffer_views[buffer_indices[3]].byte_length = buffer_view->byte_length;
                ret.buffer_views[buffer_indices[3]].byte_offset = buffer_view->byte_offset;

                ret.buffer_views[buffer_indices[3]].data =
                    gpu_make_linear_allocation(
                        allocators->vertex_allocator, 
                        ret.buffer_views [buffer_indices[3]].byte_length,
                        &allocation_offsets[buffer_indices[3]]);

                // Point draw infos at the queued 'position' vertex allocation
                ret.meshes[i].primitive_draw_infos[j].vertex_buffer_offsets[2] += allocation_offsets[buffer_indices[3]];
            } else {
                // Point draw infos at the queued 'position' vertex allocation
                ret.meshes[i].primitive_draw_infos[j].vertex_buffer_offsets[2] += allocation_offsets[buffer_indices[3]];
            }
            if ((buffer_view_mask & (1 << buffer_indices[4])) == 0) {
                buffer_view = gltf_buffer_view_by_index(model, buffer_indices[4]);

                ret.buffer_views[buffer_indices[4]].byte_length = buffer_view->byte_length;
                ret.buffer_views[buffer_indices[4]].byte_offset = buffer_view->byte_offset;

                ret.buffer_views[buffer_indices[4]].data =
                    gpu_make_linear_allocation(
                        allocators->vertex_allocator, 
                        ret.buffer_views [buffer_indices[4]].byte_length,
                        &allocation_offsets[buffer_indices[4]]);

                // Point draw infos at the queued 'position' vertex allocation
                ret.meshes[i].primitive_draw_infos[j].vertex_buffer_offsets[3] += allocation_offsets[buffer_indices[4]];
            } else {
                // Point draw infos at the queued 'position' vertex allocation
                ret.meshes[i].primitive_draw_infos[j].vertex_buffer_offsets[3] += allocation_offsets[buffer_indices[4]];
            }

            // Mark buffer as having been queued for allocation
            buffer_view_mask |= 1 << buffer_indices[0];
            buffer_view_mask |= 1 << buffer_indices[1];
            buffer_view_mask |= 1 << buffer_indices[2];
            buffer_view_mask |= 1 << buffer_indices[3];
            buffer_view_mask |= 1 << buffer_indices[4];

            primitive = (Gltf_Mesh_Primitive*)((u8*)primitive + primitive->stride);
        }
        mesh = (Gltf_Mesh*)((u8*)mesh + mesh->stride);
    }

    ret.index_allocation_end  = allocators->index_allocator->used - ret.index_allocation_start;
    ret.vertex_allocation_end = allocators->vertex_allocator->used - ret.vertex_allocation_start;

    return ret;
}
// @Todo @Speed @MemoryAccess. Idk if this function can benefit from rejigging data, because it
// seems that the buffer views already exist is the correct grouping. As in I dont think that I can
// order the data in some way that I can do fewer memcpys using larger contiguous blocks.
Renderer_Draws renderer_download_model_data(
    Gltf *model, Renderer_Vertex_Attribute_Resources *list, const char *model_dir_path) {
    Renderer_Draws ret = {
        .mesh_count = list->mesh_count,
        .meshes = list->meshes,
    };

    // @Note this system assumes that the gltf file use one bin buffer file
    ASSERT(gltf_buffer_get_count(model) == 1, "Too many gltf buffers");
    u64 buffer_len = model->buffers->byte_length;

    char model_path[127];
    int dir_path_len = strlen(model_dir_path);
    memcpy(model_path, model_dir_path, dir_path_len);

    int uri_len = strlen(model->buffers->uri);
    memcpy(model_path + dir_path_len, model->buffers->uri, uri_len);
    model_path[uri_len + dir_path_len] = '\0';

    u64 mark = get_mark_temp();
    const u8 *gltf_buffer = file_read_bin_temp_large(model_path, buffer_len);
    
    // Allocations already made in gpu linear allocators by 'setup_model_resources()'; the pointers 
    // ('list->data') being copied into point to the corresponding allocation for the buffer view.
    Renderer_Buffer_View *buffer_view;
    for(int i = 0; i < list->buffer_view_count; ++i) {
        buffer_view = &list->buffer_views[i];
        memcpy(buffer_view->data, gltf_buffer + buffer_view->byte_offset, buffer_view->byte_length);
    }

    // @Note I could flush the memory range here, to make sure that these memcpys are all visible,
    // but for now I am just assuming that there is no need, because Nvidia, Intel and AMD drivers
    // have for a while had coherent memory for device local.
    reset_to_mark_temp(mark);
    return ret;
}
Gpu_Vertex_Input_State renderer_define_vertex_input_state_static_model(
    Gltf_Mesh_Primitive *mesh_primitive, Gltf *model)
{
    Gpu_Vertex_Input_State state = {};
    // Enough memory for each array field in 'state' (5 int pointers for 4 bindings)
    int *memory_block = (int*)memory_allocate_temp(sizeof(int) * 5 * 4, 4); // @Todo dont just use 4, set this dynamically with max == supported attribute count
    state.binding_description_bindings    = memory_block;
    state.binding_description_strides     = memory_block + 4;
    state.attribute_description_locations = memory_block + 8;
    state.attribute_description_bindings  = memory_block + 12;
    state.formats = (VkFormat*)(memory_block + 16);

    // @Todo account for varying attribute count
    // @Todo account for sparse accessors (replace the correct vertices at draw time)
    state.topology = (VkPrimitiveTopology)mesh_primitive->topology;

    // pos, norm, tang, tex_coord_0
    state.input_binding_description_count   = 4;
    state.input_attribute_description_count = 4;

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

    Gltf_Accessor *accessors[4];
    accessors[0] = gltf_accessor_by_index(model, mesh_primitive->position);
    accessors[1] = gltf_accessor_by_index(model, mesh_primitive->normal);
    accessors[2] = gltf_accessor_by_index(model, mesh_primitive->tangent);
    accessors[3] = gltf_accessor_by_index(model, mesh_primitive->tex_coord_0);

    // position
    state.formats[0] = (VkFormat)accessors[0]->format;
    state.binding_description_strides     [0] = accessors[0]->byte_stride;

    // normal
    state.formats[1] = (VkFormat)accessors[1]->format;
    state.binding_description_strides     [1] = accessors[1]->byte_stride;

    // tangent
    state.formats[2] = (VkFormat)accessors[2]->format;
    state.binding_description_strides     [2] = accessors[2]->byte_stride;

    // tex_coord_0
    state.formats[3] = (VkFormat)accessors[3]->format;
    state.binding_description_strides     [3] = accessors[3]->byte_stride;

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
    state.sample_count     = info->sample_count;
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
