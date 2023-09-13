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

    // Multisample state   - just plain default, but should be set dynamically
    VkPipelineMultisampleStateCreateInfo multi_sample_state = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};//}create_vk_pipeline_multisample_state();

    // Dynamic state
    VkPipelineDynamicStateCreateInfo dyn_state = create_vk_pipeline_dyn_state();

    // Viewport state      - set dynamically
    // Rasterization state - set dynamically
    // DepthStencil state  - set dynamically
    // ColorBlend state    - we will see if this can all be supported on my device (blend equation seems not to be)
    // maybe can be dyn idk the full rules

    // Pipeline setup
    VkGraphicsPipelineCreateInfo pl_create_info = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pl_create_info.flags               = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    pl_create_info.pNext               = &rendering_create_info;
    pl_create_info.stageCount          = shader_stage_count;
    pl_create_info.pStages             = shader_stages;
    pl_create_info.pVertexInputState   = vertex_input_state;
    pl_create_info.pInputAssemblyState = vertex_assembly_state;
    pl_create_info.pViewportState      = NULL;
    pl_create_info.pRasterizationState = NULL;
    pl_create_info.pMultisampleState   = &multi_sample_state;
    pl_create_info.pDepthStencilState  = NULL;
    pl_create_info.pColorBlendState    = NULL;
    pl_create_info.pDynamicState       = &dyn_state;
    pl_create_info.layout              = *pl_layout;
    pl_create_info.renderPass          = VK_NULL_HANDLE;

    VkPipeline *pipelines =
        create_vk_graphics_pipelines_heap(gpu->vk_device, VK_NULL_HANDLE, 1, &pl_create_info);

    println("Beginning render loop...\n");
    while(!glfwWindowShouldClose(glfw->window)) {
        poll_and_get_input_glfw(glfw);
    }

    // Shutdown
    reset_temp(); // just to clear the allocation from the file read
    //destroy_vk_pipelines_heap(gpu->vk_device, 1, pipelines);
    destroy_vk_descriptor_set_layouts(gpu->vk_device, descriptor_set_layout_count, descriptor_set_layouts);
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
    load_tests();

    test_spirv();

    end_tests();
}
#endif
