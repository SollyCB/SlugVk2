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

struct Renderer_Rasterization_State_Info {};
struct Renderer_Rasterization_State {};
Renderer_Rasterization_State renderer_define_rasterization_state(Renderer_Rasterization_State_Info *info);

struct Renderer_Fragment_Shader_State_Info {};
struct Renderer_Fragment_Shader_State {};
Renderer_Fragment_Shader_State renderer_define_fragment_shader_state(Renderer_Fragment_Shader_State_Info *info);

struct Renderer_Fragment_Ouput_State_Info {};
struct Renderer_Fragment_Ouput_State {};
Renderer_Fragment_Ouput_State renderer_define_fragment_output_state(Renderer_Fragment_Ouput_State_Info *info);

#endif // include guard
