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

    Gpu_Linear_Allocator gpu_index_allocator =
        gpu_create_linear_allocator_host(
            gpu->vma_allocator, 10000, GPU_ALLOCATOR_TYPE_INDEX_TRANSFER_SRC);
    Gpu_Linear_Allocator gpu_vertex_allocator =
        gpu_create_linear_allocator_host(
            gpu->vma_allocator, 10000, GPU_ALLOCATOR_TYPE_VERTEX_TRANSFER_SRC);

    Renderer_Gpu_Allocator_Group gpu_allocator_group = {
        .index_allocator  = &gpu_index_allocator,
        .vertex_allocator = &gpu_vertex_allocator,
    };
    Renderer_Model_Resources resource_list = 
        renderer_create_model_resources(&model, &gpu_allocator_group);
    Gpu_Vertex_Input_State pl_stage_1 = resource_list.vertex_state_infos[0][0];

    Gpu_Rasterization_State pl_stage_2 =
        renderer_define_rasterization_state(GPU_POLYGON_MODE_FILL_BIT, VK_CULL_MODE_NONE);

    Renderer_Fragment_Shader_State_Info renderer_stage3_info = {};
    Gpu_Fragment_Shader_State pl_stage_3 =
        renderer_define_fragment_shader_state(&renderer_stage3_info);

    Gpu_Fragment_Output_State pl_stage_4 =
        renderer_define_fragment_output_state(GPU_BLEND_SETTING_OPAQUE_FULL_COLOR);
    
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

    memory_free_heap((void*)shader_blobs[0]); // cast for constness
    memory_free_heap((void*)shader_blobs[1]);

    int set_info_count;
    Create_Vk_Descriptor_Set_Layout_Info *descriptor_set_info = 
        group_spirv(2, parsed_spirv, &set_info_count);

    Gpu_Descriptor_List descriptor_list =
        gpu_make_descriptor_list(2, descriptor_set_info);

    // @Warn These layouts are created in a temp allocation with the expectation that after shaders
    // are loaded and all the corresponding pl_layouts and descriptor sets have been allocated, the
    // layouts are destroyed...
    VkDescriptorSetLayout *descriptor_set_layouts =
        create_vk_descriptor_set_layouts(gpu->vk_device, 2, descriptor_set_info);

    Gpu_Descriptor_Allocator descriptor_allocator =
        gpu_create_descriptor_allocator(gpu->vk_device, 64, descriptor_list.counts);

    Gpu_Queue_Descriptor_Set_Allocation_Info descriptor_set_allocation_info = {
        2, descriptor_set_layouts, descriptor_list.counts, 
    };

    Gpu_Queue_Descriptor_Set_Allocation_Info descriptor_sets_queue_info = {};
    descriptor_sets_queue_info.layout_count = 2;
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
    Create_Vk_Pipeline_Layout_Info pl_layout_info = {2, pl_layout_sets, 0, NULL};

    VkPipelineLayout pl_layout = create_vk_pipeline_layout(gpu->vk_device, &pl_layout_info);

    Create_Vk_Attachment_Description_Info attachment_description_info = {}; 
    attachment_description_info.image_format = window->swapchain_info.imageFormat;
    attachment_description_info.sample_count = VK_SAMPLE_COUNT_1_BIT;
    attachment_description_info.color_depth_setting = GPU_ATTACHMENT_DESCRIPTION_SETTING_CLEAR_AND_STORE;
    attachment_description_info.layout_transition = GPU_ATTACHMENT_DESCRIPTION_SETTING_UNDEFINED_TO_PRESENT;
    VkAttachmentDescription attachment_description = create_vk_attachment_description(&attachment_description_info);

    VkAttachmentReference color_attachment_reference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    Create_Vk_Subpass_Description_Info subpass_description_info = {};
    subpass_description_info.color_attachment_count = 1;
    subpass_description_info.color_attachments = &color_attachment_reference;
    VkSubpassDescription subpass_description = create_vk_graphics_subpass_description(&subpass_description_info);

    Create_Vk_Subpass_Dependency_Info subpass_dependency_info = {};
    subpass_dependency_info.access_rules = GPU_SUBPASS_DEPENDENCY_SETTING_ACQUIRE_TO_RENDER_TARGET_BASIC;
    subpass_dependency_info.src_subpass = VK_SUBPASS_EXTERNAL;
    subpass_dependency_info.dst_subpass = 0;
    VkSubpassDependency subpass_dependency = create_vk_subpass_dependency(&subpass_dependency_info);

    Create_Vk_Renderpass_Info renderpass_info = {};
    renderpass_info.attachment_count = 1;
    renderpass_info.subpass_count    = 1;
    renderpass_info.dependency_count = 1;
    renderpass_info.attachments      = &attachment_description;
    renderpass_info.subpasses        = &subpass_description;
    renderpass_info.dependencies     = &subpass_dependency;
    VkRenderPass renderpass = create_vk_renderpass(gpu->vk_device, &renderpass_info);

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


    /* ShutDown Code */
    gpu_destroy_pipeline(gpu->vk_device, pipeline);
    destroy_vk_renderpass(gpu->vk_device, renderpass);
    destroy_vk_pipeline_layout(gpu->vk_device, pl_layout);
    renderer_destroy_shader_stages(gpu->vk_device, 2, pl_shader_stages);
    renderer_free_resource_list(&resource_list);
    gpu_destroy_descriptor_allocator(gpu->vk_device, &descriptor_allocator);
    gpu_destroy_descriptor_set_layouts(gpu->vk_device, 2, descriptor_set_layouts);
    gpu_destroy_linear_allocator(gpu->vma_allocator, &gpu_index_allocator);
    gpu_destroy_linear_allocator(gpu->vma_allocator, &gpu_vertex_allocator);
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
