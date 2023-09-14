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

    // begin pool 1[0]
    begin_vk_command_buffer_primary(graphics_buffers_1[0]);
    end_vk_command_buffer(graphics_buffers_1[0]);

    // begin pool 1[1]
    begin_vk_command_buffer_primary(graphics_buffers_1[1]);
    end_vk_command_buffer(graphics_buffers_1[1]);

    Submit_Vk_Command_Buffer_Info submit_info {
        0, NULL, 0, NULL, 2, graphics_buffers_1,
    };

    // Sync Setup
    VkFence *vk_fence = create_vk_fences_unsignalled(gpu->vk_device, 1);
    submit_vk_command_buffer(gpu->vk_queues[0], *vk_fence, 1, &submit_info);

    vkWaitForFences(gpu->vk_device, 1, vk_fence, true, 10e9);
    reset_vk_command_pools(gpu->vk_device, 1, &graphics_command_groups[0].pool);
    destroy_vk_fences(gpu->vk_device, 1, vk_fence);

    // Descriptor setup
    u64 byte_count_vert;
    u64 byte_count_frag;
    const u32 *spirv_vert = (const u32*)file_read_bin_temp("shaders/vertex_1.vert.spv",   &byte_count_vert);
    const u32 *spirv_frag = (const u32*)file_read_bin_temp("shaders/fragment_1.frag.spv", &byte_count_frag);
    // CHECK THE SPIRV OUTPUT WITH ZERO STUFF IN THE SHADER!

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
    Create_Vk_Rendering_Info_Info rendering_info = {};
    rendering_info.color_attachment_count = 1;
    rendering_info.color_attachment_formats = &window->swapchain_info.imageFormat;
    VkPipelineRenderingCreateInfo rendering_create_info = create_vk_pipeline_rendering_info(&rendering_info);

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

    // Input state - currently unused
    Create_Vk_Vertex_Input_Binding_Description_Info vertex_binding_info     = {};
    Create_Vk_Vertex_Input_Attribute_Description_Info vertex_attribute_info = {};

    Create_Vk_Pipeline_Vertex_Input_State_Info vertex_input_state_info = { 0, NULL, 0, NULL };
    VkPipelineVertexInputStateCreateInfo *vertex_input_state = 
        create_vk_pipeline_vertex_input_states(1, &vertex_input_state_info);

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
    //rasterization.polygonMode = VK_POLYGON_MODE_FILL;

    // DepthStencil state  - set dynamically

    // Pipeline setup
    VkGraphicsPipelineCreateInfo pl_create_info = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pl_create_info.flags               =  VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    pl_create_info.pNext               = &rendering_create_info;
    pl_create_info.stageCount          =  shader_stage_count;
    pl_create_info.pStages             =  shader_stages;
    pl_create_info.pVertexInputState   =  vertex_input_state;
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

    Create_Vk_Rendering_Attachment_Info_Info render_attachment_info = {};
    render_attachment_info.image_view = window->vk_image_views[0];
    render_attachment_info.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    render_attachment_info.load_op = VK_LOAD_OP_CLEAR;
    render_attachment_info.store_op = VK_STORE_OP_STORE;

    float clear_color_white[] = {1.0f, 1.0f, 1.0f, 1.0f};
    render_attachment.clear_value.float32 = {clear_color_white};

    VkRenderingAttachmentInfo render_attachment =
        create_vk_rendering_attachment_info(&render_attachment_info);

    Create_Vk_Rendering_Info_Info rendering_info_info = {};
    rendering_info_info.render_area = {
        {
            0, 0,
        },
        {
            window->swapchain_info->extent.width,
            window->swapchain_info->extent.height,
        }
    };
    rendering_info_info.color_attachment_count = 1;
    rendering_info_info.color_attachment_infos = render_attachment;
    rendering_info_info.layer_count = 1;

    VkRenderingInfo rendering_info = create_vk_rendering_info(&rendering_info_info);

    // Before Rendering:
    //     Set depth stencil state before rendering
    //     Transition image
    VkCommandBuffer graphics_cmd;
    cmd_vk_depth_test_disable(graphics_cmd);

    println("Beginning render loop...\n");
    while(!glfwWindowShouldClose(glfw->window)) {
        poll_and_get_input_glfw(glfw);
    }

    // Shutdown
    reset_temp();
    memory_free_heap((void*)vertex_input_state);
    memory_free_heap((void*)vertex_assembly_state);

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
