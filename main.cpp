#include "basic.h"
#include "glfw.hpp"
#include "gpu.hpp"
#include "file.hpp"
#include "spirv.hpp"

#if TEST
#include "test.hpp"
void run_tests(); // defined below main
#endif

int main() {
    init_allocators();

#if TEST
    run_tests(); 
#endif
    
#if 0
    init_glfw(); 
    Glfw *glfw = get_glfw_instance();

    init_gpu();
    Gpu *gpu = get_gpu_instance();

    init_window(gpu, glfw);
    Window *window = get_window_instance();

    // Command Buffer setup
    u32 command_pool_count = 2;
    VkCommandPool *graphics_command_pools = (VkCommandPool*)memory_allocate_heap(sizeof(VkCommandBuffer) * command_pool_count, 8);
    VkCommandPool  *present_command_pools = (VkCommandPool*)memory_allocate_heap(sizeof(VkCommandBuffer) * command_pool_count, 8);
    VkCommandPool *transfer_command_pools = (VkCommandPool*)memory_allocate_heap(sizeof(VkCommandBuffer) * command_pool_count, 8);

    Create_Vk_Command_Pool_Info pool_infos[] = { 
        {gpu->vk_queue_indices[0]},
        {gpu->vk_queue_indices[1]},
        {gpu->vk_queue_indices[2]},
    };

    create_vk_command_pools(gpu->vk_device, &pool_infos[0], command_pool_count, graphics_command_pools);
    create_vk_command_pools(gpu->vk_device, &pool_infos[1], command_pool_count, present_command_pools );
    create_vk_command_pools(gpu->vk_device, &pool_infos[2], command_pool_count, transfer_command_pools);

    u32 command_buffer_count = 4;
    Allocate_Vk_Command_Buffer_Info allocate_infos[] = {
        {graphics_command_pools[0], command_buffer_count},
        {graphics_command_pools[1], command_buffer_count},
        {present_command_pools [0], command_buffer_count},
        {present_command_pools [1], command_buffer_count},
        {transfer_command_pools[0], command_buffer_count},
        {transfer_command_pools[1], command_buffer_count},
    };

    VkCommandBuffer *graphics_command_buffers =
        (VkCommandBuffer*)memory_allocate_heap(sizeof(VkCommandBuffer) * command_buffer_count * 2, 8);
    VkCommandBuffer *present_command_buffers =
        (VkCommandBuffer*)memory_allocate_heap(sizeof(VkCommandBuffer) * command_buffer_count * 2, 8);
    VkCommandBuffer *transfer_command_buffers =
        (VkCommandBuffer*)memory_allocate_heap(sizeof(VkCommandBuffer) * command_buffer_count * 2, 8);

    allocate_vk_command_buffers(gpu->vk_device, &allocate_infos[0], &graphics_command_buffers[0]);
    allocate_vk_command_buffers(gpu->vk_device, &allocate_infos[1], &graphics_command_buffers[1]);
    allocate_vk_command_buffers(gpu->vk_device, &allocate_infos[2], &present_command_buffers [0]); 
    allocate_vk_command_buffers(gpu->vk_device, &allocate_infos[3], &present_command_buffers [1]); 
    allocate_vk_command_buffers(gpu->vk_device, &allocate_infos[4], &transfer_command_buffers[0]); 
    allocate_vk_command_buffers(gpu->vk_device, &allocate_infos[5], &transfer_command_buffers[1]); 

    begin_vk_command_buffer_primary(graphics_command_buffers[0]);
    end_vk_command_buffer(graphics_command_buffers[0]);
    begin_vk_command_buffer_primary(graphics_command_buffers[1]);
    end_vk_command_buffer(graphics_command_buffers[1]);

    Submit_Vk_Command_Buffer_Info submit_info {
        0, NULL, 0, NULL, 2, graphics_command_buffers,
    };

    // Sync Setup
    VkFence vk_fence;
    create_vk_fences_unsignalled(gpu->vk_device, 1, &vk_fence);
    submit_vk_command_buffer(gpu->vk_queues[0], vk_fence, 1, &submit_info);
    vkWaitForFences(gpu->vk_device, 1, &vk_fence, true, 10e9);
    destroy_vk_fences(gpu->vk_device, 1, &vk_fence);

    

    println("Beginning render loop...\n");
    while(!glfwWindowShouldClose(glfw->window)) {
        poll_and_get_input_glfw(glfw);
    }

    // Command Buffer shutdown
    destroy_vk_command_pools(gpu->vk_device, 2, graphics_command_pools);
    memory_free_heap(graphics_command_pools);
    memory_free_heap(graphics_command_buffers);
    destroy_vk_command_pools(gpu->vk_device, 2,  present_command_pools);
    memory_free_heap(present_command_pools);
    memory_free_heap(present_command_buffers);
    destroy_vk_command_pools(gpu->vk_device, 2, transfer_command_pools);
    memory_free_heap(transfer_command_pools);
    memory_free_heap(transfer_command_buffers);

    kill_window(gpu, window);
    kill_gpu(gpu);
    kill_glfw(glfw);
#endif

    kill_allocators();
    return 0;
}

#if TEST
void run_tests() {
    test_spirv();
}
#endif
