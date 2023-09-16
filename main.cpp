#include "basic.h"
#include "glfw.hpp"
#include "gpu.hpp"
#include "file.hpp"
#include "spirv.hpp"
#include "glm/glm.hpp"

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
    init_glfw(); 
    Glfw *glfw = get_glfw_instance();

    init_gpu();
    Gpu *gpu = get_gpu_instance();

    init_window(gpu, glfw);
    Window *window = get_window_instance();

    // Command Buffer setup
    Command_Group_Vk graphics_command_groups[] = {
        create_command_group_vk(gpu->vk_device, gpu->vk_queue_indices[0]),
        create_command_group_vk(gpu->vk_device, gpu->vk_queue_indices[0]),
    };
    Command_Group_Vk present_command_groups [2] = {
        create_command_group_vk(gpu->vk_device, gpu->vk_queue_indices[1]),
        create_command_group_vk(gpu->vk_device, gpu->vk_queue_indices[1]),
    };
    Command_Group_Vk transfer_command_groups[2] = {
        create_command_group_vk(gpu->vk_device, gpu->vk_queue_indices[2]),
        create_command_group_vk(gpu->vk_device, gpu->vk_queue_indices[2]),
    }; // allocate 2 so i can have one per frame

    // allocate command buffers
    VkCommandBuffer *graphics_buffers_1 =
        allocate_vk_primary_command_buffers(gpu->vk_device, &graphics_command_groups[0], 4);
    VkCommandBuffer *graphics_buffers_2 =
        allocate_vk_primary_command_buffers(gpu->vk_device, &graphics_command_groups[1], 4);

    VkCommandBuffer *present_buffers_1  =
        allocate_vk_primary_command_buffers(gpu->vk_device, &present_command_groups [0], 4);
    VkCommandBuffer *present_buffers_2  =
        allocate_vk_primary_command_buffers(gpu->vk_device, &present_command_groups [1], 4);

    VkCommandBuffer *transfer_buffers_1 =
        allocate_vk_primary_command_buffers(gpu->vk_device, &transfer_command_groups[0], 4);
    VkCommandBuffer *transfer_buffers_2 =
        allocate_vk_primary_command_buffers(gpu->vk_device, &transfer_command_groups[1], 4);

    const glm::vec3 vertices[] = {
        glm::vec3( 0.0, -0.5, 1.0),  
        glm::vec3( 0.5,  0.5, 1.0),  
        glm::vec3(-0.5,  0.5, 1.0),  
    };

    const u64 vert_buffer_size = 256;
    Gpu_Buffer src_vert_buffer = create_src_vertex_buffer(gpu->vma_allocator, 512);
    void *src_vert_ptr = get_vma_mapped_ptr(gpu->vma_allocator, &src_vert_buffer);
    memcpy(src_vert_ptr, vertices, sizeof(vertices));

    Gpu_Buffer dst_vert_buffer = create_dst_vertex_buffer(gpu->vma_allocator, 512);

    MemDep_Queue_Transfer_Info_Buffer transfer_info = {
        (u16)gpu->vk_queue_indices[2],
        (u16)gpu->vk_queue_indices[0],
        dst_vert_buffer.vk_buffer,
        sizeof(vertices),
    };
    VkBufferMemoryBarrier2 buffer_barrier = fill_buffer_barrier_transfer(&transfer_info);
    VkDependencyInfo buffer_dependency = fill_vk_dependency_buffer(&buffer_barrier);

    Binary_Semaphore_Pool semaphore_pool = create_binary_semaphore_pool(gpu->vk_device, 128);
    Fence_Pool fence_pool = create_fence_pool(gpu->vk_device, 128);

    VkSemaphore *semaphores = get_binary_semaphores(&semaphore_pool, 16);
    VkFence *fences = get_fences(&fence_pool, 16);

    VkSemaphoreSubmitInfo semaphore_info_transfer = {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    semaphore_info_transfer.semaphore = semaphores[0];
    semaphore_info_transfer.stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

    // upload data to gpu
    VkCommandBuffer transfer_cmd = transfer_buffers_1[0];

    Submit_Vk_Command_Buffer_Info transfer_submit_info  = {
        0, NULL, 1, &semaphore_info_transfer, 1, &transfer_cmd,
    };

    begin_vk_command_buffer_primary(transfer_cmd);

        vkCmdPipelineBarrier2(transfer_cmd, &buffer_dependency);
        cmd_vk_copy_buffer(transfer_cmd, src_vert_buffer.vk_buffer, dst_vert_buffer.vk_buffer, sizeof(vertices));

    end_vk_command_buffer(transfer_cmd);

    // Sync Setup
    VkFence fence = fences[0];
    submit_vk_command_buffer(gpu->vk_queues[2], fence, 1, &transfer_submit_info);

    vkWaitForFences(gpu->vk_device, 1, &fence, true, 10e9);
    vkResetFences(gpu->vk_device, 1, &fence);
    reset_vk_command_pools(gpu->vk_device, 1, &graphics_command_groups[0].pool);

    // Shader setup
    u64 byte_count_vert;
    u64 byte_count_frag;
    const u32 *spirv_vert = (const u32*)file_read_bin_temp("shaders/vertex_2.vert.spv",   &byte_count_vert);
    const u32 *spirv_frag = (const u32*)file_read_bin_temp("shaders/fragment_2.frag.spv", &byte_count_frag);

    Parsed_Spirv parsed_spirv_vert = parse_spirv(byte_count_vert, spirv_vert); 

    u32 descriptor_set_layout_count;
    VkDescriptorSetLayout *descriptor_set_layouts = create_vk_descriptor_set_layouts(gpu->vk_device, &parsed_spirv_vert, &descriptor_set_layout_count);

    Create_Vk_Pipeline_Layout_Info pl_layout_info = { 
        descriptor_set_layout_count,
        descriptor_set_layouts,
        0,
        NULL,
    };
    VkPipelineLayout *pl_layout = create_vk_pipeline_layouts(gpu->vk_device, 1, &pl_layout_info);

    // Rendering info
    Create_Vk_Pipeline_Rendering_Info_Info pl_rendering_info = {};
    pl_rendering_info.color_attachment_count   = 1;
    pl_rendering_info.color_attachment_formats = &window->swapchain_info.imageFormat;
    VkPipelineRenderingCreateInfo rendering_create_info = create_vk_pipeline_rendering_info(&pl_rendering_info);

    // Shader stages
    Create_Vk_Pipeline_Shader_Stage_Info shader_stage_infos[] = {
        {
            byte_count_vert, spirv_vert, VK_SHADER_STAGE_VERTEX_BIT, NULL
        }, 
        {
            byte_count_frag, spirv_frag, VK_SHADER_STAGE_FRAGMENT_BIT, NULL
        } 
    };
    u32 shader_stage_count = 2;
    VkPipelineShaderStageCreateInfo *shader_stages =
        create_vk_pipeline_shader_stages(gpu->vk_device, shader_stage_count, shader_stage_infos);

    // Input state
    Create_Vk_Vertex_Input_Binding_Description_Info vertex_binding_info     = { 0, sizeof(glm::vec3) };
    Create_Vk_Vertex_Input_Attribute_Description_Info vertex_attribute_info = { 0, 0, 0, VEC_TYPE_3 };

    VkVertexInputBindingDescription input_binding = create_vk_vertex_binding_description(&vertex_binding_info);
    VkVertexInputAttributeDescription attribute_desc = create_vk_vertex_attribute_description(&vertex_attribute_info);

    Create_Vk_Pipeline_Vertex_Input_State_Info vertex_input_state_info = { 1, &input_binding, 1, &attribute_desc };
    VkPipelineVertexInputStateCreateInfo vertex_input_state = 
        create_vk_pipeline_vertex_input_states(&vertex_input_state_info);

    // Assembly state
    VkPrimitiveTopology topology       = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkBool32 primitive_restart_enabled = VK_FALSE;
    VkPipelineInputAssemblyStateCreateInfo *vertex_assembly_state =
        create_vk_pipeline_input_assembly_states(1, &topology, &primitive_restart_enabled);

    // Viewport
    VkPipelineViewportStateCreateInfo viewport_info = create_vk_pipeline_viewport_state(window);

    // Multisample state   - just plain default, but should be set dynamically
    VkPipelineMultisampleStateCreateInfo multisampling = create_vk_pipeline_multisample_state();

    // Colorblend state
    VkPipelineColorBlendAttachmentState blend_attachment_state = {};
    blend_attachment_state.blendEnable = VK_FALSE;
    blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                            VK_COLOR_COMPONENT_G_BIT |
                                            VK_COLOR_COMPONENT_B_BIT |
                                            VK_COLOR_COMPONENT_A_BIT;
    Create_Vk_Pl_Color_Blend_State_Info blend_state_info = {};
    blend_state_info.logic_op_enable = VK_FALSE;
    blend_state_info.attachment_count = 1;
    blend_state_info.attachment_states = &blend_attachment_state;
    VkPipelineColorBlendStateCreateInfo blend_state = create_vk_pipeline_color_blend_state(&blend_state_info);

    // Dynamic state
    VkPipelineDynamicStateCreateInfo dyn_state = create_vk_pipeline_dyn_state();

    VkPipelineRasterizationStateCreateInfo rasterization = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;

    // DepthStencil state  - set dynamically

    // Pipeline setup
    VkGraphicsPipelineCreateInfo pl_create_info = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pl_create_info.flags               =  VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    pl_create_info.pNext               = &rendering_create_info;
    pl_create_info.stageCount          =  shader_stage_count;
    pl_create_info.pStages             =  shader_stages;
    pl_create_info.pVertexInputState   =  &vertex_input_state;
    pl_create_info.pInputAssemblyState =  vertex_assembly_state;
    pl_create_info.pViewportState      = &viewport_info;
    pl_create_info.pRasterizationState = &rasterization;
    pl_create_info.pMultisampleState   = &multisampling;
    pl_create_info.pDepthStencilState  =  NULL;
    pl_create_info.pColorBlendState    = &blend_state;
    pl_create_info.pDynamicState       = &dyn_state;
    pl_create_info.layout              = *pl_layout;
    pl_create_info.renderPass          =  VK_NULL_HANDLE;

    VkPipeline *pipelines =
        create_vk_graphics_pipelines_heap(gpu->vk_device, VK_NULL_HANDLE, 1, &pl_create_info);

    VkAcquireNextImageInfoKHR acquire_info = {VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR};
    acquire_info.swapchain  = window->vk_swapchain;
    acquire_info.timeout    = 10e9;
    acquire_info.semaphore  = VK_NULL_HANDLE;
    acquire_info.fence      = fence;
    acquire_info.deviceMask = 1;

    u32 present_image_index;
    vkAcquireNextImage2KHR(gpu->vk_device, &acquire_info, &present_image_index);
    vkWaitForFences(gpu->vk_device, 1, &fence, true, 10e9);
    vkResetFences(gpu->vk_device, 1, &fence);

    float clear_color_white[] = {1.0f, 1.0f, 1.0f, 1.0f};
    Create_Vk_Rendering_Attachment_Info_Info render_attachment_info = {};
    render_attachment_info.image_view   = window->vk_image_views[present_image_index];
    render_attachment_info.clear_color  = clear_color_white;

    VkRenderingAttachmentInfo render_attachment =
        create_vk_rendering_attachment_info(&render_attachment_info);

    Create_Vk_Rendering_Info_Info rendering_info_info = {};
    VkRect2D render_area = {
        .offset = { 0, 0, },
        .extent = { 
            window->swapchain_info.imageExtent.width,
            window->swapchain_info.imageExtent.height, 
        }
    };
    rendering_info_info.render_area = render_area;
    rendering_info_info.color_attachment_count = 1;
    rendering_info_info.color_attachment_infos = &render_attachment;
    rendering_info_info.layer_count = 1;
    VkRenderingInfo rendering_info = create_vk_rendering_info(&rendering_info_info);

    VkImageSubresourceRange view  = {};
    view.aspectMask               = VK_IMAGE_ASPECT_COLOR_BIT;
    view.baseMipLevel             = 0;
    view.levelCount               = 1;
    view.baseArrayLayer           = 0;
    view.layerCount               = 1;

    MemDep_Queue_Transition_Info_Image transition_info = {};
    transition_info.image = window->vk_images[present_image_index];
    transition_info.view = view;
    VkImageMemoryBarrier2 transition_barrier =
        fill_image_barrier_transition_undefined_to_cao(&transition_info);

    MemDep_Queue_Transfer_Info_Image cao_to_present_info = {};
    cao_to_present_info.release_queue_index = gpu->vk_queue_indices[0];
    cao_to_present_info.acquire_queue_index = gpu->vk_queue_indices[1];
    cao_to_present_info.image = window->vk_images[present_image_index];
    VkImageMemoryBarrier2 transfer_barrier =
        fill_image_barrier_transfer_cao_to_present(&cao_to_present_info);

    Fill_Vk_Dependency_Info transition_dependency_info = {};
    transition_dependency_info.image_barrier_count = 1;
    transition_dependency_info.image_barriers = &transition_barrier;
    VkDependencyInfo transition_dependency = fill_vk_dependency(&transition_dependency_info);

    Fill_Vk_Dependency_Info transfer_dependency_info = {};
    transfer_dependency_info.image_barrier_count = 1;
    transfer_dependency_info.image_barriers = &transfer_barrier;
    VkDependencyInfo transfer_dependency = fill_vk_dependency(&transfer_dependency_info);

    // Drawing
    VkCommandBuffer graphics_cmd = graphics_buffers_1[0];

    begin_vk_command_buffer_primary(graphics_cmd);

        vkCmdPipelineBarrier2(graphics_cmd, &buffer_dependency);
        vkCmdPipelineBarrier2(graphics_cmd, &transition_dependency);
        set_default_draw_state(graphics_cmd);

        // Bind vertex buffers
        u64 offset = 0;
        u64 size = sizeof(vertices);
        u64 stride = sizeof(glm::vec3);
        Dyn_Vertex_Bind_Info buffer_bind_info = {0, 1, &dst_vert_buffer.vk_buffer, &offset, &size, &stride};
        cmd_vk_bind_vertex_buffers2(graphics_cmd, &buffer_bind_info);

        vkCmdBeginRendering(graphics_cmd, &rendering_info);

            vkCmdBindPipeline(graphics_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelines);
            vkCmdDraw(graphics_cmd, 3, 1, 0, 0);

        vkCmdEndRendering(graphics_cmd);
        vkCmdPipelineBarrier2(graphics_cmd, &transfer_dependency);

    end_vk_command_buffer(graphics_cmd);

    VkSemaphoreSubmitInfo wait_transfer_semaphore_info = {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    wait_transfer_semaphore_info.semaphore = semaphores[0];
    wait_transfer_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;

    VkSemaphoreSubmitInfo signal_render_semaphore_info = {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    signal_render_semaphore_info.semaphore = semaphores[1];
    signal_render_semaphore_info.stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    Submit_Vk_Command_Buffer_Info graphics_submit_info = {
        1, &wait_transfer_semaphore_info, 1, &signal_render_semaphore_info, 1, &graphics_cmd 
    };
    submit_vk_command_buffer(gpu->vk_queues[0], fence, 1, &graphics_submit_info);

    VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.waitSemaphoreCount    = 1;
    present_info.pWaitSemaphores       = &semaphores[1];
    present_info.swapchainCount        = 1;
    present_info.pSwapchains           = &window->vk_swapchain;
    present_info.pImageIndices         = &present_image_index;
    vkQueuePresentKHR(gpu->vk_queues[1], &present_info);

    println("Beginning render loop...\n");
    while(!glfwWindowShouldClose(glfw->window)) {
        poll_and_get_input_glfw(glfw);
    }

    // Shutdown
    reset_temp();
    memory_free_heap((void*)vertex_assembly_state);

    vkDeviceWaitIdle(gpu->vk_device);

    destroy_vma_buffer(gpu->vma_allocator, &dst_vert_buffer);
    destroy_vma_buffer(gpu->vma_allocator, &src_vert_buffer);

    destroy_fence_pool(gpu->vk_device, &fence_pool);
    destroy_binary_semaphore_pool(gpu->vk_device, &semaphore_pool);

    destroy_vk_pipelines_heap(gpu->vk_device, 1, pipelines);
    destroy_vk_pipeline_shader_stages(gpu->vk_device, 2, shader_stages);

    destroy_vk_pipeline_layouts(gpu->vk_device, 1, pl_layout);
    destroy_vk_descriptor_set_layouts(gpu->vk_device, descriptor_set_layout_count, descriptor_set_layouts);

    destroy_command_groups_vk(gpu->vk_device, 2, graphics_command_groups);
    destroy_command_groups_vk(gpu->vk_device, 2,  present_command_groups);
    destroy_command_groups_vk(gpu->vk_device, 2, transfer_command_groups);

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

    end_tests();
}
#endif
