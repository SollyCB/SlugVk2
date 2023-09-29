#include "renderer.hpp"

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

Renderer_Vertex_Input_State renderer_define_vertex_input_state(Gltf_Mesh_Primitive *mesh_primitive, Gltf *model, Renderer_Draw_Info *draw_info) {
    
    Renderer_Vertex_Input_State state = {};

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

    //
    // @Todo look at the benefits of using locations / array access in shaders
    //  I think it is useful for more homogeneous stuff than this?
    //
    state.attribute_description_locations[0] = 0; // pos
    state.attribute_description_locations[1] = 0; // norm
    state.attribute_description_locations[2] = 0; // tang
    state.attribute_description_locations[3] = 0; // tex_coord_0

    int position    = mesh_primitive->position;
    int normal      = mesh_primitive->normal;
    int tangent     = mesh_primitive->tangent;
    int tex_coord_0 = mesh_primitive->tex_coord_0;

    // position
    Gltf_Accessor *accessor = &model->accessors[position];
    state.formats[0] = (VkFormat)accessor->format;

    draw_info->draw_count = accessor->count; // take the number of elements to draw from the 'position' vertex attribute

    Gltf_Buffer_View buffer_view = model->buffer_views[accessor->buffer_view];
    draw_info->offsets[0] = accessor->byte_offset + buffer_view.byte_offset;
    draw_info->file_names[0] = model->buffers[buffer_view.buffer].uri;

    if (buffer_view.byte_stride) {
        state.binding_description_strides[0] = buffer_view.byte_stride;
    } else {
        state.binding_description_strides[0] = renderer_get_byte_stride(accessor->format);
    }

    // normal
    accessor = &model->accessors[normal];
    state.formats[1] = (VkFormat)accessor->format;

    buffer_view = model->buffer_views[accessor->buffer_view];
    draw_info->offsets[1] = accessor->byte_offset + buffer_view.byte_offset;
    draw_info->file_names[1] = model->buffers[buffer_view.buffer].uri;

    if (buffer_view.byte_stride) {
        state.binding_description_strides[1] = buffer_view.byte_stride;
    } else {
        state.binding_description_strides[1] = renderer_get_byte_stride(accessor->format);
    }

    // tangent
    accessor = &model->accessors[tangent];
    state.formats[2] = (VkFormat)accessor->format;

    buffer_view = model->buffer_views[accessor->buffer_view];
    draw_info->offsets[2] = accessor->byte_offset + buffer_view.byte_offset;
    draw_info->file_names[2] = model->buffers[buffer_view.buffer].uri;

    if (buffer_view.byte_stride) {
        state.binding_description_strides[2] = buffer_view.byte_stride;
    } else {
        state.binding_description_strides[2] = renderer_get_byte_stride(accessor->format);
    }

    // tex_coord_0
    accessor = &model->accessors[tex_coord_0];
    state.formats[3] = (VkFormat)accessor->format;

    buffer_view = model->buffer_views[accessor->buffer_view];
    draw_info->offsets[3] = accessor->byte_offset + buffer_view.byte_offset;
    draw_info->file_names[3] = model->buffers[buffer_view.buffer].uri;

    if (buffer_view.byte_stride) {
        state.binding_description_strides[3] = buffer_view.byte_stride;
    } else {
        state.binding_description_strides[3] = renderer_get_byte_stride(accessor->format);
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
