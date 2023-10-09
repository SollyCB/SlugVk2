#include "GLFW/glfw3.h"
// #include "basic.h"
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

    Gpu_Linear_Allocator host_index_allocator =
        gpu_create_linear_allocator_host(
            gpu->vma_allocator, 10000, GPU_ALLOCATOR_TYPE_INDEX_TRANSFER_SRC);
    Gpu_Linear_Allocator host_vertex_allocator =
        gpu_create_linear_allocator_host(
            gpu->vma_allocator, 10000, GPU_ALLOCATOR_TYPE_VERTEX_TRANSFER_SRC);

    Gpu_Linear_Allocator device_index_allocator =
        gpu_create_linear_allocator_device(
            gpu->vma_allocator, 10000, GPU_ALLOCATOR_TYPE_INDEX_TRANSFER_DST);
    Gpu_Linear_Allocator device_vertex_allocator =
        gpu_create_linear_allocator_device(
            gpu->vma_allocator, 10000, GPU_ALLOCATOR_TYPE_VERTEX_TRANSFER_DST);

    Renderer_Gpu_Allocator_Group gpu_allocator_group = {
        .index_allocator  = &host_index_allocator,
        .vertex_allocator = &host_vertex_allocator,
    };
    Renderer_Model_Resources resource_list =
        renderer_setup_model_resources(&model, &gpu_allocator_group);
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
    // layouts are destroyed...
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

    Gpu_Image depth_attachment =
        gpu_create_depth_attachment(gpu->vma_allocator, window->info.imageExtent.width, window->info.imageExtent.height);
    VkImageView depth_attachment_view =
        gpu_create_depth_attachment_view(gpu->vk_device, depth_attachment.vk_image);

    attachment_description_info.format = VK_FORMAT_D16_UNORM;
    attachment_description_info.setting = GPU_ATTACHMENT_DESCRIPTION_SETTING_DEPTH_LOAD_UNDEFINED_STORE;
    VkAttachmentDescription depth_attachment_description =
        gpu_get_attachment_description(&attachment_description_info);

    VkAttachmentReference color_attachment_reference = {
        0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };
    VkAttachmentReference depth_attachment_reference = {
        1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };
    Create_Vk_Subpass_Description_Info subpass_description_info = {};
    subpass_description_info.color_attachment_count   = 1;
    subpass_description_info.color_attachments        = &color_attachment_reference;
    subpass_description_info.depth_stencil_attachment = &depth_attachment_reference;
    VkSubpassDescription subpass_description =
        create_vk_graphics_subpass_description(&subpass_description_info);

    Create_Vk_Subpass_Dependency_Info subpass_dependency_info = {};
    subpass_dependency_info.access_rules = GPU_SUBPASS_DEPENDENCY_SETTING_COLOR_DEPTH_BASIC_DRAW;
    subpass_dependency_info.src_subpass  = VK_SUBPASS_EXTERNAL;
    subpass_dependency_info.dst_subpass  = 0;
    VkSubpassDependency subpass_dependency = create_vk_subpass_dependency(&subpass_dependency_info);

    VkAttachmentDescription attachment_descriptions[] = {
        color_attachment_description,
        depth_attachment_description
    };

    Create_Vk_Renderpass_Info renderpass_info = {};
    renderpass_info.attachment_count = 2;
    renderpass_info.subpass_count    = 1;
    renderpass_info.dependency_count = 1;
    renderpass_info.attachments      = attachment_descriptions;
    renderpass_info.subpasses        = &subpass_description;
    renderpass_info.dependencies     = &subpass_dependency;
    VkRenderPass renderpass = create_vk_renderpass(gpu->vk_device, &renderpass_info);

    Gpu_Fence_Pool fence_pool = gpu_create_fence_pool(gpu->vk_device, 1);
    VkFence *fence = gpu_get_fences(&fence_pool, 1);
    u32 image_index;
    vkAcquireNextImageKHR(
        gpu->vk_device, window->vk_swapchain, 10e9, VK_NULL_HANDLE, *fence, &image_index);
    vkWaitForFences(gpu->vk_device, 1, fence, VK_TRUE, 10e9);
    vkResetFences(gpu->vk_device, 1, fence);

    VkImageView framebuffer_attachments[] = {
        window->vk_image_views[image_index],
        depth_attachment_view,
    };
    Gpu_Framebuffer_Info framebuffer_info = {};
    framebuffer_info.renderpass = renderpass;
    framebuffer_info.attachment_count = 2;
    framebuffer_info.attachments = framebuffer_attachments;
    framebuffer_info.width = window->info.imageExtent.width;
    framebuffer_info.height = window->info.imageExtent.height;
    VkFramebuffer framebuffer = gpu_create_framebuffer(gpu->vk_device, &framebuffer_info);

    Renderer_Create_Pipeline_Info pl_info = {};
    pl_info.layout                = pl_layout;
    pl_info.renderpass            = renderpass;
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

    VkBufferCopy index_buffer_copy =
        gpu_linear_allocator_setup_copy(
            &device_index_allocator,
            resource_list.index_allocation_start,
            resource_list.index_allocation_end);
    VkBufferCopy vertex_buffer_copy =
        gpu_linear_allocator_setup_copy(
            &device_vertex_allocator,
            resource_list.vertex_allocation_start,
            resource_list.vertex_allocation_end);

    int transfer_queue_index = gpu->vk_queue_indices[2];
    int graphics_queue_index = gpu->vk_queue_indices[0];
    Gpu_Buffer_Barrier_Info buffer_barrier_infos[] = {
        {
            .setting   = GPU_MEMORY_BARRIER_SETTING_TRANSFER_SRC,
            .src_queue = transfer_queue_index,
            .dst_queue = graphics_queue_index,
            .buffer    = device_index_allocator.buffer,
            .offset    = resource_list.index_allocation_start + device_index_allocator.offset,
            .size      = resource_list.index_allocation_end,
        },
        {
            .setting   = GPU_MEMORY_BARRIER_SETTING_TRANSFER_SRC,
            .src_queue = transfer_queue_index,
            .dst_queue = graphics_queue_index,
            .buffer    = device_vertex_allocator.buffer,
            .offset    = resource_list.index_allocation_start + device_index_allocator.offset,
            .size      = resource_list.index_allocation_end,
        },
        {
            .setting   = GPU_MEMORY_BARRIER_SETTING_INDEX_BUFFER_TRANSFER_DST,
            .src_queue = transfer_queue_index,
            .dst_queue = graphics_queue_index,
            .buffer    = device_index_allocator.buffer,
            .offset    = resource_list.index_allocation_start + device_index_allocator.offset,
            .size      = resource_list.index_allocation_end,
        },
        {
            .setting   = GPU_MEMORY_BARRIER_SETTING_VERTEX_BUFFER_TRANSFER_DST,
            .src_queue = transfer_queue_index,
            .dst_queue = graphics_queue_index,
            .buffer    = device_vertex_allocator.buffer,
            .offset    = resource_list.index_allocation_start + device_index_allocator.offset,
            .size      = resource_list.index_allocation_end,
        },
    };

    VkBufferMemoryBarrier2 buffer_barriers[] = {
        gpu_get_buffer_barrier(&buffer_barrier_infos[0]),
        gpu_get_buffer_barrier(&buffer_barrier_infos[1]),
        gpu_get_buffer_barrier(&buffer_barrier_infos[2]),
        gpu_get_buffer_barrier(&buffer_barrier_infos[3]),
    };

    Gpu_Pipeline_Barrier_Info pl_barrier_info_transfer = {
        .buffer_count = 2,
        .buffer_barriers = buffer_barriers,
    };
    VkDependencyInfo pl_dependency_transfer = gpu_get_pipeline_barrier(&pl_barrier_info_transfer);

    gpu_begin_primary_command_buffer(*transfer_cmd, false);

        vkCmdCopyBuffer(*transfer_cmd,
            host_index_allocator.buffer,
            device_index_allocator.buffer,
            1, &index_buffer_copy);
        vkCmdCopyBuffer(*transfer_cmd,
            host_vertex_allocator.buffer,
            device_vertex_allocator.buffer,
            1, &vertex_buffer_copy);

        vkCmdPipelineBarrier2(*transfer_cmd, &pl_dependency_transfer);

    gpu_end_command_buffer(*transfer_cmd);

    Gpu_Binary_Semaphore_Pool semaphore_pool =
        gpu_create_binary_semaphore_pool(gpu->vk_device, 4);
    VkSemaphore *transfer_semaphore = gpu_get_binary_semaphores(&semaphore_pool, 1);

    VkSemaphoreSubmitInfo transfer_semaphore_submit =
        gpu_define_semaphore_submission(*transfer_semaphore, GPU_PIPELINE_STAGE_TRANSFER);

    Gpu_Queue_Submit_Info transfer_submit_info = {
        .signal_count    = 1,
        .cmd_count       = 1,
        .signal_infos    = &transfer_semaphore_submit,
        .command_buffers = transfer_cmd,
    };
    VkSubmitInfo2 transfer_queue_submit =
        gpu_get_submit_info(&transfer_submit_info);
    vkQueueSubmit2(gpu->vk_queues[2], 1, &transfer_queue_submit, VK_NULL_HANDLE);

    Gpu_Pipeline_Barrier_Info pl_barrier_info_graphics = {
        .buffer_count = 2,
        .buffer_barriers = &buffer_barriers[2],
    };
    VkDependencyInfo pl_dependency_graphics = gpu_get_pipeline_barrier(&pl_barrier_info_graphics);

    VkRenderPassBeginInfo renderpass_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderpass_begin_info.renderPass = renderpass;
    renderpass_begin_info.framebuffer = framebuffer;
    VkRect2D render_area = {
        .offset = {
            0,0
        },
        .extent = {
            window->info.imageExtent.width,
            window->info.imageExtent.height,
        }
    };
    renderpass_begin_info.renderArea = render_area;
    renderpass_begin_info.clearValueCount = 2;

    VkClearValue clear_values[] = {
        {
            .color = {1.0, 1.0, 1.0, 1.0}
        },
        {
            .depthStencil = {1.0, 0},
        }
    };
    renderpass_begin_info.pClearValues = clear_values;

    VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = window->info.imageExtent.width;
        viewport.height = window->info.imageExtent.height;
        viewport.minDepth = 0.0;
        viewport.maxDepth = 1.0;

    VkBuffer vertex_buffers[] = {
        device_vertex_allocator.buffer,
        device_vertex_allocator.buffer,
        device_vertex_allocator.buffer,
        device_vertex_allocator.buffer,
    };
    u64 vertex_buffer_offsets[] = {
        model_draw_infos.draw_infos[0][0].vertex_buffer_offsets[0],
        model_draw_infos.draw_infos[0][0].vertex_buffer_offsets[1],
        model_draw_infos.draw_infos[0][0].vertex_buffer_offsets[2],
        model_draw_infos.draw_infos[0][0].vertex_buffer_offsets[3],
    };
    gpu_begin_primary_command_buffer(*graphics_cmd, false);

        vkCmdSetViewportWithCount(*graphics_cmd, 1, &viewport);
        vkCmdSetScissorWithCount(*graphics_cmd, 1, &render_area);

        vkCmdPipelineBarrier2(*graphics_cmd, &pl_dependency_graphics);

        vkCmdBeginRenderPass(*graphics_cmd, &renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(*graphics_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

            vkCmdBindIndexBuffer(
                *graphics_cmd,
                device_index_allocator.buffer,
                model_draw_infos.draw_infos[0][0].index_buffer_offset,
                VK_INDEX_TYPE_UINT16);
            vkCmdBindVertexBuffers(
                *graphics_cmd,
                0,
                4,
                vertex_buffers,
                vertex_buffer_offsets);
            vkCmdDrawIndexed(
                *graphics_cmd,
                model_draw_infos.draw_infos[0][0].draw_count,
                12,
                0, 0, 0);

        vkCmdEndRenderPass(*graphics_cmd);

    gpu_end_command_buffer(*graphics_cmd);


    VkSemaphore *graphics_semaphore = gpu_get_binary_semaphores(&semaphore_pool, 1);

    VkSemaphoreSubmitInfo graphics_semaphore_wait =
        gpu_define_semaphore_submission(*transfer_semaphore, GPU_PIPELINE_STAGE_VERTEX_INPUT);
    VkSemaphoreSubmitInfo graphics_semaphore_submit =
        gpu_define_semaphore_submission(*graphics_semaphore, GPU_PIPELINE_STAGE_ALL_GRAPHICS);

    Gpu_Queue_Submit_Info graphics_submit_info = {
        .wait_count      = 1,
        .signal_count    = 1,
        .cmd_count       = 1,
        .wait_infos      = &graphics_semaphore_wait,
        .signal_infos    = &graphics_semaphore_submit,
        .command_buffers = graphics_cmd,
    };
    VkSubmitInfo2 graphics_queue_submit =
        gpu_get_submit_info(&graphics_submit_info);
    vkQueueSubmit2(gpu->vk_queues[0], 1, &graphics_queue_submit, *fence);

    VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = graphics_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &window->vk_swapchain;
    present_info.pImageIndices = &image_index;
    VkResult present_result;
    present_info.pResults = &present_result;
    vkQueuePresentKHR(gpu->vk_queues[1], &present_info);

    while(!glfwWindowShouldClose(glfw->window)) {
        window_poll_and_get_input(glfw);
    }

    /* ShutDown Code */
    vkDeviceWaitIdle(gpu->vk_device);
    gpu_destroy_command_allocator(gpu->vk_device, &graphics_command_allocator);
    gpu_destroy_command_allocator(gpu->vk_device, &transfer_command_allocator);

    gpu_destroy_image(gpu->vma_allocator, &depth_attachment); // depth attachment
    gpu_destroy_image_view(gpu->vk_device, depth_attachment_view);
    gpu_destroy_framebuffer(gpu->vk_device, framebuffer);

    gpu_reset_fence_pool(gpu->vk_device, &fence_pool);
    gpu_destroy_fence_pool(gpu->vk_device, &fence_pool);
    gpu_destroy_semaphore_pool(gpu->vk_device, &semaphore_pool);

    gpu_destroy_pipeline(gpu->vk_device, pipeline);
    destroy_vk_renderpass(gpu->vk_device, renderpass);
    destroy_vk_pipeline_layout(gpu->vk_device, pl_layout);
    renderer_destroy_shader_stages(gpu->vk_device, 2, pl_shader_stages);
    renderer_free_model_data(&resource_list);

    gpu_destroy_descriptor_allocator(gpu->vk_device, &descriptor_allocator);
    gpu_destroy_descriptor_set_layouts(gpu->vk_device, set_info_count, descriptor_set_layouts);

    gpu_destroy_linear_allocator(gpu->vma_allocator, &host_index_allocator);
    gpu_destroy_linear_allocator(gpu->vma_allocator, &host_vertex_allocator);
    gpu_destroy_linear_allocator(gpu->vma_allocator, &device_index_allocator);
    gpu_destroy_linear_allocator(gpu->vma_allocator, &device_vertex_allocator);

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
