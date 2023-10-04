#include "basic.h"
#include "glfw.hpp"
#include "gpu.hpp"
#include "file.hpp"
#include "spirv.hpp"
#include "math.hpp"
#include "image.hpp"
#include "gltf.hpp"
#include "simd.hpp"
#include "renderer.hpp"

#if TEST
#include "test.hpp"
void run_tests(); // defined below main
#endif

int main() {
    init_allocators();

#if TEST
    run_tests(); 
#endif

    #if 1
    /* StartUp Code */
    init_glfw(); 
    Glfw *glfw = get_glfw_instance();

    init_gpu();
    Gpu *gpu = get_gpu_instance();

    init_window(gpu, glfw);
    Window *window = get_window_instance();

    /* Begin Code That Actually Does Stuff */
    Gltf model = parse_gltf("models/cube-static/Cube.gltf");
    Renderer_Draw_Info draw_info = {};

    Gpu_Vertex_Input_State pl_stage_1 =
        renderer_define_vertex_input_state(model.meshes->primitives, &model, &draw_info);

    Gpu_Rasterization_State pl_stage_2 =
        renderer_define_rasterization_state(GPU_POLYGON_MODE_FILL_BIT, VK_CULL_MODE_NONE);
    
    u64 code_sizes[2];
    const u32 *shader_blobs[2] = { 
        (const u32*)file_read_bin_heap("shaders/vertex_4.vert.spv", &code_sizes[0]),
        (const u32*)file_read_bin_heap("shaders/fragment_4.frag.spv", &code_sizes[1]),
    };

    int descriptor_set_counts[2];
    Parsed_Spirv parsed_spirv[2] = {
        parse_spirv(code_sizes[0], shader_blobs[0], &descriptor_set_counts[0]),
        parse_spirv(code_sizes[1], shader_blobs[1], &descriptor_set_counts[1]),
    };

    memory_free_heap((void*)shader_blobs[0]); // cast for constness
    memory_free_heap((void*)shader_blobs[1]);

    int set_info_count;
    Create_Vk_Descriptor_Set_Layout_Info *descriptor_set_info = 
        group_spirv(2, parsed_spirv, &set_info_count);

    Gpu_Allocate_Descriptor_Set_Info *descriptor_set_layouts =
        create_vk_descriptor_set_layouts(gpu->vk_device, 2, descriptor_set_info);

    Gpu_Descriptor_Allocator descriptor_allocator =
        gpu_create_descriptor_allocator(gpu->vk_device, 64, 64); // allocate 64 sampler descriptors, 64 buffer descriptors

    Gpu_Descriptor_Allocation descriptor_sets = gpu_queue_descriptor_set_allocation(&descriptor_allocator, 2, descriptor_set_layouts);

    /* ShutDown Code */
    gpu_destroy_descriptor_allocator(gpu->vk_device, &descriptor_allocator);
    gpu_destroy_descriptor_set_layouts(gpu->vk_device, 2, descriptor_set_layouts);
    kill_window(gpu, window);
    kill_gpu(gpu);
    kill_glfw(glfw);
    #endif

    kill_allocators();
    return 0;
}

#if TEST
void run_tests() {
    load_tests();

    test_spirv();
    test_gltf();

    end_tests();
}
#endif
