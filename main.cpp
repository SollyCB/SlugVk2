#include "GLFW/glfw3.h"
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
#include "vulkan/vulkan_core.h"

#if TEST
#include "test.hpp"
void run_tests(); // defined below main
#endif

// @Todo Find a good memory footprint
// Some completely arbitrary sizes for now. This should be managed as the other memory resources are.
static constexpr u64 DRAW_INFO_ALLOCATOR_SIZE  = 10000;

int main() {
    init_allocators();

#if TEST
    run_tests();
#endif

    /* StartUp Code */
    init_glfw();
    Glfw *glfw = get_glfw_instance();

    init_gpu();
    Gpu *gpu = get_gpu_instance();

    #if 1
    init_window(gpu, glfw);
    Window *window = get_window_instance();

    // Has to be called after init window for screen extent values for attachments
    gpu_init_memory_resources(gpu);


    /* Begin Code That Actually Does Stuff */
    Gltf model = parse_gltf("models/cube-static/Cube.gltf");

    Gpu_Buf_Allocator *device_index_allocator  = gpu->index_device_allocator;
    Gpu_Buf_Allocator *device_vertex_allocator = gpu->vertex_device_allocator;

    Gpu_Buf_Allocator *host_index_allocator  = gpu->index_host_allocator;
    Gpu_Buf_Allocator *host_vertex_allocator = gpu->vertex_host_allocator;

    Linear_Allocator draw_info_allocator = create_linear_allocator(DRAW_INFO_ALLOCATOR_SIZE);

    Renderer_Gpu_Allocator_Group gpu_allocator_group = {
        .draw_info_allocator = &draw_info_allocator,
        .index_allocator     = host_index_allocator,
        .vertex_allocator    = host_vertex_allocator,
    };
    Renderer_Vertex_Attribute_Resources resource_list =
        renderer_setup_vertex_attribute_resources_static_model(&model, &gpu_allocator_group);
    Renderer_Draws model_draw_infos =
        renderer_download_model_data(&model, &resource_list, "models/cube-static/");

    Gpu_Vertex_Input_State pl_stage_1 = resource_list.vertex_state_infos[0][0];

    Gpu_Rasterization_State pl_stage_2 =
        renderer_define_rasterization_state(GPU_POLYGON_MODE_FILL_BIT, VK_CULL_MODE_NONE);

    // Defaults to depth test + write enabled, stencil off, depth bounds off
    Renderer_Fragment_Shader_State_Info renderer_stage3_info = {};
    Gpu_Fragment_Shader_State pl_stage_3 =
        renderer_define_fragment_shader_state(&renderer_stage3_info);

    Gpu_Fragment_Output_State pl_stage_4 =
        renderer_define_fragment_output_state(GPU_BLEND_SETTING_OPAQUE_FULL_COLOR);

    u64 code_sizes[2];
    const u32 *shader_blobs[2] = {
        (const u32*)file_read_bin_temp("shaders/vertex_cube_basic.spv", &code_sizes[0]),
        (const u32*)file_read_bin_temp("shaders/fragment_cube_basic.spv", &code_sizes[1]),
    };

    int descriptor_set_counts[2];
    Parsed_Spirv parsed_spirv[2] = {
        parse_spirv(code_sizes[0], shader_blobs[0], &descriptor_set_counts[0]),
        parse_spirv(code_sizes[1], shader_blobs[1], &descriptor_set_counts[1]),
    };

    Renderer_Create_Shader_Stage_Info create_shader_stage_infos[] = {
        {
            VK_SHADER_STAGE_VERTEX_BIT, code_sizes[0], shader_blobs[0]
        },
        {
            VK_SHADER_STAGE_FRAGMENT_BIT, code_sizes[1], shader_blobs[1]
        }
    };
    VkPipelineShaderStageCreateInfo* pl_shader_stages =
        renderer_create_shader_stages(gpu->vk_device, 2, create_shader_stage_infos);

    int set_info_count;
    Create_Vk_Descriptor_Set_Layout_Info *descriptor_set_info =
        group_spirv(2, parsed_spirv, &set_info_count);

    Gpu_Descriptor_List descriptor_list =
        gpu_make_descriptor_list(set_info_count, descriptor_set_info);

    // @Warn These layouts are created in a temp allocation with the expectation that after shaders
    // are loaded and all the corresponding pl_layouts and descriptor sets have been allocated, the
    // layouts are destroyed... @Note Perhaps they should be moved into a non global allocator
    // in the same way as Renderer_Draw_Infos are allocated. (@Todo)
    VkDescriptorSetLayout *descriptor_set_layouts =
        create_vk_descriptor_set_layouts(gpu->vk_device, set_info_count, descriptor_set_info);

    Gpu_Descriptor_Allocator descriptor_allocator =
        gpu_create_descriptor_allocator(gpu->vk_device, 64, descriptor_list.counts);

    Gpu_Queue_Descriptor_Set_Allocation_Info descriptor_set_allocation_info = {
        2, descriptor_set_layouts, descriptor_list.counts,
    };

    Gpu_Queue_Descriptor_Set_Allocation_Info descriptor_sets_queue_info = {};
    descriptor_sets_queue_info.layout_count = set_info_count;
    descriptor_sets_queue_info.layouts = descriptor_set_layouts;
    descriptor_sets_queue_info.descriptor_counts = descriptor_list.counts;

    VkResult descriptor_result;
    VkDescriptorSet *descriptor_sets =
        gpu_queue_descriptor_set_allocation(
            &descriptor_allocator, &descriptor_sets_queue_info, &descriptor_result);
    gpu_allocate_descriptor_sets(gpu->vk_device, &descriptor_allocator);

    VkDescriptorSetLayout pl_layout_sets[8];
    for(int i = 0; i < set_info_count; ++i)
        pl_layout_sets[i] = descriptor_set_layouts[i];
    Create_Vk_Pipeline_Layout_Info pl_layout_info = {(u32)set_info_count, pl_layout_sets, 0, NULL};

    VkPipelineLayout pl_layout = create_vk_pipeline_layout(gpu->vk_device, &pl_layout_info);

    Gpu_Attachment_Description_Info attachment_description_info = {};
    attachment_description_info.format = window->info.imageFormat;
    attachment_description_info.setting = GPU_ATTACHMENT_DESCRIPTION_SETTING_COLOR_LOAD_UNDEFINED_STORE;
    VkAttachmentDescription color_attachment_description =
        gpu_get_attachment_description(&attachment_description_info);

    VkImage depth_attachment = gpu->memory_resources.depth_attachments[0];
    VkImageView depth_attachment_view =
        gpu_create_depth_attachment_view(gpu->vk_device, depth_attachment);

    attachment_description_info.format = VK_FORMAT_D16_UNORM;
    attachment_description_info.setting = GPU_ATTACHMENT_DESCRIPTION_SETTING_DEPTH_LOAD_UNDEFINED_STORE;
    VkAttachmentDescription depth_attachment_description =
        gpu_get_attachment_description(&attachment_description_info);

    Gpu_Fence_Pool fence_pool = gpu_create_fence_pool(gpu->vk_device, 1);
    VkFence *fence = gpu_get_fences(&fence_pool, 1);
    u32 image_index;
    vkAcquireNextImageKHR(
        gpu->vk_device, window->vk_swapchain, 10e9, VK_NULL_HANDLE, *fence, &image_index);
    vkWaitForFences(gpu->vk_device, 1, fence, VK_TRUE, 10e9);
    vkResetFences(gpu->vk_device, 1, fence);
    
    Gpu_Renderpass_Info renderpass_info = {};
    renderpass_info.sample_count = 1;
    renderpass_info.color_attachment_count = 1;
    renderpass_info.color_views = &window->vk_image_views[image_index];
    renderpass_info.depth_view = &depth_attachment_view;
    // renderpass_info.color_clear_value = {.color = {1, 1, 1, 1}};
    // renderpass_info.depth_clear_value = {.depthStencil = {1, 0}};
    Gpu_Renderpass_Framebuffer renderpass_framebuffer =
        gpu_create_renderpass_framebuffer_graphics_single(gpu->vk_device, &renderpass_info);

    Renderer_Create_Pipeline_Info pl_info = {};
    pl_info.layout                = pl_layout;
    pl_info.renderpass            = renderpass_framebuffer.renderpass;
    pl_info.subpass               = 0;
    pl_info.shader_stage_count    = 2;
    pl_info.shader_stages         = pl_shader_stages;
    pl_info.vertex_input_state    = &pl_stage_1;
    pl_info.rasterization_state   = &pl_stage_2;
    pl_info.fragment_shader_state = &pl_stage_3;
    pl_info.fragment_output_state = &pl_stage_4;
    VkPipeline pipeline = renderer_create_pipeline(gpu->vk_device, &pl_info);

    Gpu_Command_Allocator graphics_command_allocator =
        gpu_create_command_allocator(gpu->vk_device, gpu->vk_queue_indices[0], false, 4);
    Gpu_Command_Allocator transfer_command_allocator =
        gpu_create_command_allocator(gpu->vk_device, gpu->vk_queue_indices[2], false, 4);

    VkCommandBuffer *graphics_cmd =
        gpu_allocate_command_buffers(gpu->vk_device, &graphics_command_allocator, 1, false);
    VkCommandBuffer *transfer_cmd =
        gpu_allocate_command_buffers(gpu->vk_device, &transfer_command_allocator, 1, false);

    VkCopyBufferInfo2 buffer_copy_infos[] = {
        gpu_buf_allocator_setup_copy(
            device_index_allocator,
            host_index_allocator,
            resource_list.index_allocation_start,
            resource_list.index_allocation_end),
        gpu_buf_allocator_setup_copy(
            device_vertex_allocator,
            host_vertex_allocator,
            resource_list.vertex_allocation_start,
            resource_list.vertex_allocation_end),
    };

    Gpu_Binary_Semaphore_Pool semaphore_pool =
        gpu_create_binary_semaphore_pool(gpu->vk_device, 4);
    VkSemaphore *transfer_semaphore = gpu_get_binary_semaphores(&semaphore_pool, 1);

    VkBuffer buffers[] = {device_index_allocator->buf, device_vertex_allocator->buf};

    Gpu_Buffer_Copy_Info buffer_copy = {};
    buffer_copy.transfer_signal = *transfer_semaphore;
    buffer_copy.transfer_cmd = *transfer_cmd;
    buffer_copy.graphics_cmd = *graphics_cmd;
    buffer_copy.buffer_count = 2;
    buffer_copy.buffers = buffers;
    buffer_copy.copy_infos = buffer_copy_infos;

    VkBuffer vertex_buffers[] = {
        device_vertex_allocator->buf,
        device_vertex_allocator->buf,
        device_vertex_allocator->buf,
        device_vertex_allocator->buf,
    };
    u64 vertex_buffer_offsets[] = {
        model_draw_infos.meshes[0].primitive_draw_infos[0].vertex_buffer_offsets[0],
        model_draw_infos.meshes[0].primitive_draw_infos[0].vertex_buffer_offsets[1],
        model_draw_infos.meshes[0].primitive_draw_infos[0].vertex_buffer_offsets[2],
        model_draw_infos.meshes[0].primitive_draw_infos[0].vertex_buffer_offsets[3],
    };

    VkViewport viewport = gpu_get_complete_screen_viewport();
    VkRect2D render_area = gpu_get_complete_screen_area();

    Gpu_Renderpass_Begin_Info renderpass_begin_info = {
        &renderpass_framebuffer, &render_area,
    };
    
    VkSubmitInfo2 graphics_submit_info = gpu_cmd_begin_buf_transfer_graphics(&buffer_copy);
        vkCmdSetViewportWithCount(*graphics_cmd, 1, &viewport);
        vkCmdSetScissorWithCount(*graphics_cmd, 1, renderpass_begin_info.render_area);

        gpu_cmd_primary_begin_renderpass(*graphics_cmd, &renderpass_begin_info);

            vkCmdBindPipeline(*graphics_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

            vkCmdBindIndexBuffer(
                *graphics_cmd,
                device_index_allocator->buf,
                model_draw_infos.meshes[0].primitive_draw_infos[0].index_buffer_offset,
                VK_INDEX_TYPE_UINT16);
            vkCmdBindVertexBuffers(
                *graphics_cmd,
                0,
                4,
                vertex_buffers,
                vertex_buffer_offsets);
            vkCmdDrawIndexed(
                *graphics_cmd,
                model_draw_infos.meshes[0].primitive_draw_infos[0].draw_count,
                1,
                0, 0, 0);

        vkCmdEndRenderPass(*graphics_cmd);

    gpu_end_command_buffer(*graphics_cmd);


    VkSemaphore *graphics_semaphore = gpu_get_binary_semaphores(&semaphore_pool, 1);

    VkSemaphoreSubmitInfo graphics_signal =
        gpu_define_semaphore_submission(*graphics_semaphore, GPU_PIPELINE_STAGE_ALL_GRAPHICS);

    graphics_submit_info.signalSemaphoreInfoCount = 1;
    graphics_submit_info.pSignalSemaphoreInfos = &graphics_signal;

    vkQueueSubmit2(gpu->vk_queues[0], 1, &graphics_submit_info, *fence);

    VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = graphics_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &window->vk_swapchain;
    present_info.pImageIndices = &image_index;
    VkResult present_result;
    present_info.pResults = &present_result;
    vkQueuePresentKHR(gpu->vk_queues[1], &present_info);

    zero_temp();
    while(!glfwWindowShouldClose(glfw->window)) {
        // vkAcquireNextImageKHR(
        //     gpu->vk_device, window->vk_swapchain, 10e9, VK_NULL_HANDLE, *fence, &image_index);
        window_poll_and_get_input(glfw);
    }

    /* ShutDown Code */
    vkDeviceWaitIdle(gpu->vk_device);
    gpu_destroy_command_allocator(gpu->vk_device, &graphics_command_allocator);
    gpu_destroy_command_allocator(gpu->vk_device, &transfer_command_allocator);

    gpu_destroy_image_view(gpu->vk_device, depth_attachment_view);
    gpu_destroy_renderpass_framebuffer(gpu->vk_device, &renderpass_framebuffer);

    gpu_reset_fence_pool(gpu->vk_device, &fence_pool);
    gpu_destroy_fence_pool(gpu->vk_device, &fence_pool);
    gpu_destroy_semaphore_pool(gpu->vk_device, &semaphore_pool);

    gpu_destroy_pipeline(gpu->vk_device, pipeline);
    destroy_vk_pipeline_layout(gpu->vk_device, pl_layout);
    renderer_destroy_shader_stages(gpu->vk_device, 2, pl_shader_stages);

    destroy_linear_allocator(&draw_info_allocator);
    gpu_destroy_descriptor_allocator(gpu->vk_device, &descriptor_allocator);
    gpu_destroy_descriptor_set_layouts(gpu->vk_device, set_info_count, descriptor_set_layouts);

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
