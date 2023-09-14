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
    u32 command_pool_count = 2;
    Create_Vk_Command_Pool_Info pool_infos[] = { 
        {gpu->vk_queue_indices[0]},
        {gpu->vk_queue_indices[1]},
        {gpu->vk_queue_indices[2]},
    };
    VkCommandPool *graphics_command_pools = create_vk_command_pools(gpu->vk_device, &pool_infos[0], command_pool_count);
    VkCommandPool *present_command_pools  = create_vk_command_pools(gpu->vk_device, &pool_infos[1], command_pool_count);
    VkCommandPool *transfer_command_pools = create_vk_command_pools(gpu->vk_device, &pool_infos[2], command_pool_count);

    u32 command_buffer_count = 4;
    Allocate_Vk_Command_Buffer_Info allocate_infos[] = {
        {graphics_command_pools[0], command_buffer_count},
        {graphics_command_pools[1], command_buffer_count},
        {present_command_pools [0], command_buffer_count},
        {present_command_pools [1], command_buffer_count},
        {transfer_command_pools[0], command_buffer_count},
        {transfer_command_pools[1], command_buffer_count},
    };

    // @Leak these are not freed. Fixing now...
    VkCommandBuffer *graphics_command_buffers_pool_1 =
        allocate_vk_command_buffers(gpu->vk_device, &allocate_infos[0]);
    VkCommandBuffer *graphics_command_buffers_pool_2 =
        allocate_vk_command_buffers(gpu->vk_device, &allocate_infos[1]);
    VkCommandBuffer *present_command_buffers_pool_1 =
        allocate_vk_command_buffers(gpu->vk_device, &allocate_infos[2]); 
    VkCommandBuffer *present_command_buffers_pool_2 =
        allocate_vk_command_buffers(gpu->vk_device, &allocate_infos[3]); 
    VkCommandBuffer *transfer_command_buffers_pool_1 =
        allocate_vk_command_buffers(gpu->vk_device, &allocate_infos[4]); 
    VkCommandBuffer *transfer_command_buffers_pool_2 =
        allocate_vk_command_buffers(gpu->vk_device, &allocate_infos[5]); 

    begin_vk_command_buffer_primary(graphics_command_buffers_pool_1[0]);
    end_vk_command_buffer(graphics_command_buffers_pool_1[0]);
    begin_vk_command_buffer_primary(graphics_command_buffers_pool_2[1]);
    end_vk_command_buffer(graphics_command_buffers_pool_1[1]);

    Submit_Vk_Command_Buffer_Info submit_info {
        0, NULL, 0, NULL, 2, graphics_command_buffers_pool_1,
    };

    // Sync Setup
    VkFence *vk_fence = create_vk_fences_unsignalled(gpu->vk_device, 1);
    submit_vk_command_buffer(gpu->vk_queues[0], *vk_fence, 1, &submit_info);

    vkWaitForFences(gpu->vk_device, 1, vk_fence, true, 10e9);
    destroy_vk_fences(gpu->vk_device, 1, vk_fence);

    // Descriptor setup
    u64 byte_count_vert;
    u64 byte_count_frag;
    const u32 *spirv_vert = (const u32*)file_read_bin_temp("shaders/vertex_1.vert.spv",   &byte_count_vert);
    const u32 *spirv_frag = (const u32*)file_read_bin_temp("shaders/fragment_1.frag.spv", &byte_count_frag);

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
    VkPipelineRenderingCreateInfo rendering_create_info = create_vk_rendering_info(&rendering_info);

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
    VkPipelineViewportStateCreateInfo viewport_info = create_vk_pl_viewport_state(window);

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
    VkPipelineColorBlendStateCreateInfo blend_state = create_vk_pl_color_blend_state(&blend_state_info);

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

    println("Beginning render loop...\n");
    while(!glfwWindowShouldClose(glfw->window)) {
        poll_and_get_input_glfw(glfw);
    }

    // Shutdown
    reset_temp(); // just to clear the allocation from the file read
    memory_free_heap((void*)vertex_input_state);
    memory_free_heap((void*)vertex_assembly_state);

    destroy_vk_pipelines_heap(gpu->vk_device, 1, pipelines);
    destroy_vk_pl_shader_stages(gpu->vk_device, 2, shader_stages);
    destroy_vk_pl_layouts(gpu->vk_device, 1, pl_layout);
    destroy_vk_descriptor_set_layouts(gpu->vk_device, descriptor_set_layout_count, descriptor_set_layouts);
    destroy_vk_command_pools(gpu->vk_device, 2, graphics_command_pools);
    destroy_vk_command_pools(gpu->vk_device, 2,  present_command_pools);
    destroy_vk_command_pools(gpu->vk_device, 2, transfer_command_pools);

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
