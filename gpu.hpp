#ifndef SOL_GPU_HPP_INCLUDE_GUARD_
#define SOL_GPU_HPP_INCLUDE_GUARD_

#include <vulkan/vulkan_core.h>

#include "basic.h"
#include "glfw.hpp"

struct GpuInfo {};
struct Gpu {
    VkInstance vk_instance;
    //VmaAllocator allocator

    VkPhysicalDevice vk_physical_device;
    VkDevice vk_device;
    VkQueue *vk_queues; // 3 queues ordered graphics, presentation, transfer
    u32 *vk_queue_indices;

    GpuInfo *info;
};
Gpu* get_gpu_instance();

void init_gpu();
void kill_gpu(Gpu *gpu);

// Instance
struct Create_Vk_Instance_Info {
    const char *application_name = "SlugApp";
    u32 application_version_major  = 0;
    u32 application_version_middle = 0;
    u32 application_version_minor  = 0;

    const char *engine_name = "SlugEngine";
    u32 engine_version_major  = 0;
    u32 engine_version_middle = 0;
    u32 engine_version_minor  = 0;

    u32 vulkan_api_version = VK_API_VERSION_1_3;
};
VkInstance create_vk_instance(Create_Vk_Instance_Info *info); 

// Device and Queues
VkDevice create_vk_device(Gpu *gpu);
VkQueue create_vk_queue(Gpu *gpu);
void destroy_vk_device(Gpu *gpu);
void destroy_vk_queue(Gpu *gpu);

// Surface and Swapchain
struct Window {
    VkSwapchainCreateInfoKHR swapchain_info;

    VkSwapchainKHR vk_swapchain;
    u32 image_count;
    VkImage *vk_images;
    VkImageView *vk_image_views;
};
Window* get_window_instance();

void init_window(Gpu *gpu, Glfw *glfw);
void kill_window(Gpu *gpu, Window *window);

VkSurfaceKHR create_vk_surface(VkInstance vk_instance, Glfw *glfw);
void destroy_vk_surface(VkInstance vk_instance, VkSurfaceKHR vk_surface);

VkSwapchainKHR create_vk_swapchain(Gpu *gpu, VkSurfaceKHR vk_surface); // This function is a little weird, because it does a lot, but at the same time the swapchain basically is the window, so it makes sense that it such a big function
void destroy_vk_swapchain(VkDevice vk_device, Window *window); // also destroys image views
VkSwapchainKHR recreate_vk_swapchain(Gpu *gpu, Window *window);

// CommandPools and CommandBuffers
struct Create_Vk_Command_Pool_Info {
    u32 queue_family_index;
    // @BoolsInStructs
    bool transient = false;
    bool reset_buffers = false;
};
VkCommandPool* create_vk_command_pools(VkDevice vk_device, Create_Vk_Command_Pool_Info *info, u32 count);
void destroy_vk_command_pools(VkDevice vk_device, u32 count, VkCommandPool *vk_command_pools);

void reset_vk_command_pools(VkDevice vk_device, u32 count, VkCommandPool *vk_command_pools);
void reset_vk_command_pools_and_release_resources(VkDevice vk_device, u32 count, VkCommandPool *vk_command_pools);

struct Allocate_Vk_Command_Buffer_Info {
    VkCommandPool pool;
    u32 count;
    bool secondary = false;
};
VkCommandBuffer* allocate_vk_command_buffers(VkDevice vk_device, Allocate_Vk_Command_Buffer_Info *info);

void begin_vk_command_buffer_primary(VkCommandBuffer vk_command_buffer);
void begin_vk_command_buffer_primary_onetime(VkCommandBuffer vk_command_buffer);
void end_vk_command_buffer(VkCommandBuffer vk_command_buffer);

struct Submit_Vk_Command_Buffer_Info {
    u32 wait_semaphore_info_count;
    VkSemaphoreSubmitInfo *wait_semaphore_infos;
    u32 signal_semaphore_info_count;
    VkSemaphoreSubmitInfo *signal_semaphore_infos;
    u32 command_buffer_count;
    VkCommandBuffer *command_buffers;
};
void submit_vk_command_buffer(VkQueue vk_queue, VkFence vk_fence, u32 count, Submit_Vk_Command_Buffer_Info *infos);

// Sync
VkFence* create_vk_fences_unsignalled(VkDevice vk_device, u32 count);
VkFence* create_vk_fences_signalled(VkDevice vk_device, u32 count);
void destroy_vk_fences(VkDevice vk_device, u32 count, VkFence *vk_fences);

VkSemaphore* create_vk_semaphores_binary(VkDevice vk_device, u32 count);
VkSemaphore* create_vk_semaphores_timeline(VkDevice vk_device, u64 initial_value, u32 count);
void destroy_vk_semaphores(VkDevice vk_device, u32 count, VkSemaphore *vk_semaphore);

// Descriptors
struct Create_Vk_Descriptor_Set_Layout_Info {
    u32 count;
    VkDescriptorSetLayoutBinding *bindings;
};
struct Parsed_Spirv;
VkDescriptorSetLayout* create_vk_descriptor_set_layouts(VkDevice vk_device, Parsed_Spirv *parsed_spirv, u32 *count);
void destroy_vk_descriptor_set_layouts(VkDevice vk_device, u32 count, VkDescriptorSetLayout *layouts);

// Pipeline Setup
// `ShaderStages
struct Create_Vk_Pipeline_Shader_Stage_Info {
    u64 code_size;
    const u32 *shader_code;
    VkShaderStageFlagBits stage;
    VkSpecializationInfo *spec_info = NULL;
};
VkPipelineShaderStageCreateInfo* create_vk_pipeline_shader_stages(VkDevice vk_device, u32 count, Create_Vk_Pipeline_Shader_Stage_Info *infos);
void destroy_vk_pl_shader_stages(VkDevice vk_device, u32 count, VkPipelineShaderStageCreateInfo *stages);

// `VertexInputState
// @StructPacking pack struct
struct Create_Vk_Vertex_Input_Binding_Description_Info {
    u32 binding;
    u32 stride;
    // @Todo support instance input rate
};
VkVertexInputBindingDescription* create_vk_vertex_binding_description(u32 count, Create_Vk_Vertex_Input_Binding_Description_Info *infos);
struct Create_Vk_Vertex_Input_Attribute_Description_Info {
    u32 location;
    u32 binding;
    u32 offset;
    VkFormat format;
};
VkVertexInputAttributeDescription* create_vk_vertex_attribute_description(u32 count, Create_Vk_Vertex_Input_Attribute_Description_Info *infos);
struct Create_Vk_Pipeline_Vertex_Input_State_Info {
    u32 binding_count;
    VkVertexInputBindingDescription *binding_descriptions;
    u32 attribute_count;
    VkVertexInputAttributeDescription *attribute_descriptions;
};
VkPipelineVertexInputStateCreateInfo* create_vk_pipeline_vertex_input_states(u32 count, Create_Vk_Pipeline_Vertex_Input_State_Info *infos);

// `InputAssemblyState
VkPipelineInputAssemblyStateCreateInfo* create_vk_pipeline_input_assembly_states(u32 count, VkPrimitiveTopology *topologies, VkBool32 *primitive_restart);

// `TessellationState
// @Todo support Tessellation

// Viewport
VkPipelineViewportStateCreateInfo create_vk_pl_viewport_state(Window *window);
static inline void cmd_vk_set_viewports(u32 count, VkCommandBuffer *vk_command_buffers) {
    VkSwapchainCreateInfoKHR info = get_window_instance()->swapchain_info;
    VkViewport viewport = {
        0.0f, 0.0f, // x, y
        (float)info.imageExtent.width,
        (float)info.imageExtent.height,
        0.0f, 1.0f, // mindepth, maxdepth
    };
    for(int i = 0; i < count; ++i)
        vkCmdSetViewportWithCount(vk_command_buffers[i], 1, &viewport);
}

// `RasterizationState
void vkCmdSetDepthClampEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClampEnable);
void vkCmdSetPolygonModeEXT(VkCommandBuffer commandBuffer, VkPolygonMode polygonMode);

static inline void cmd_vk_enable_depth_clamp(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthClampEnableEXT(vk_command_buffer, VK_TRUE);
}
static inline void cmd_vk_disable_depth_clamp(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthClampEnableEXT(vk_command_buffer, VK_FALSE);
}

static inline void cmd_vk_rasterizer_discard_enable(VkCommandBuffer vk_command_buffer) {
    vkCmdSetRasterizerDiscardEnable(vk_command_buffer, VK_TRUE);
}
static inline void cmd_vk_rasterizer_discard_disable(VkCommandBuffer vk_command_buffer) {
    vkCmdSetRasterizerDiscardEnable(vk_command_buffer, VK_FALSE);
}

static inline void cmd_vk_draw_fill(VkCommandBuffer vk_command_buffer) {
    vkCmdSetPolygonModeEXT(vk_command_buffer, VK_POLYGON_MODE_FILL);
}
static inline void cmd_vk_draw_wireframe(VkCommandBuffer vk_command_buffer) {
    vkCmdSetPolygonModeEXT(vk_command_buffer, VK_POLYGON_MODE_LINE);
}
static inline void cmd_vk_draw_points(VkCommandBuffer vk_command_buffer) {
    vkCmdSetPolygonModeEXT(vk_command_buffer, VK_POLYGON_MODE_POINT);
}

static inline void cmd_vk_cull_none(VkCommandBuffer vk_command_buffer) {
    vkCmdSetCullMode(vk_command_buffer, VK_CULL_MODE_NONE);
}
static inline void cmd_vk_cull_front(VkCommandBuffer vk_command_buffer) {
    vkCmdSetCullMode(vk_command_buffer, VK_CULL_MODE_FRONT_BIT);
}
static inline void cmd_vk_cull_back(VkCommandBuffer vk_command_buffer) {
    vkCmdSetCullMode(vk_command_buffer, VK_CULL_MODE_BACK_BIT);
}
static inline void cmd_vk_cull_front_and_back(VkCommandBuffer vk_command_buffer) {
    vkCmdSetCullMode(vk_command_buffer, VK_CULL_MODE_FRONT_AND_BACK);
}

static inline void cmd_vk_front_face_clockwise(VkCommandBuffer vk_command_buffer) {
    vkCmdSetFrontFace(vk_command_buffer, VK_FRONT_FACE_CLOCKWISE);
}
static inline void cmd_vk_front_face_counter_clockwise(VkCommandBuffer vk_command_buffer) {
    vkCmdSetFrontFace(vk_command_buffer, VK_FRONT_FACE_COUNTER_CLOCKWISE);
}

static inline void cmd_vk_depth_bias_enable(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthBiasEnable(vk_command_buffer, VK_TRUE);
}
static inline void cmd_vk_depth_bias_disable(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthBiasEnable(vk_command_buffer, VK_FALSE);
}

static inline void cmd_vk_line_width(VkCommandBuffer vk_command_buffer, float width) {
    vkCmdSetLineWidth(vk_command_buffer, width);
}

// `MultisampleState // @Todo support setting multisampling functions
//struct Create_Vk_Pipeline_Multisample_State_Info {};
VkPipelineMultisampleStateCreateInfo create_vk_pipeline_multisample_state();

// `DepthStencilState
static inline void cmd_vk_depth_test_enable(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthTestEnable(vk_command_buffer, VK_TRUE);
}
static inline void cmd_vk_depth_test_disable(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthTestEnable(vk_command_buffer, VK_FALSE);
}
static inline void cmd_vk_depth_write_enable(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthWriteEnable(vk_command_buffer, VK_TRUE);
}
static inline void cmd_vk_depth_write_disable(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthWriteEnable(vk_command_buffer, VK_FALSE);
}

static inline void cmd_vk_depth_compare_op_never(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthCompareOp(vk_command_buffer, VK_COMPARE_OP_NEVER);
}
static inline void cmd_vk_depth_compare_op_less(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthCompareOp(vk_command_buffer, VK_COMPARE_OP_LESS);
}
static inline void cmd_vk_depth_compare_op_equal(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthCompareOp(vk_command_buffer, VK_COMPARE_OP_EQUAL);
}
static inline void cmd_vk_depth_compare_op_less_or_equal(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthCompareOp(vk_command_buffer, VK_COMPARE_OP_LESS_OR_EQUAL);
}
static inline void cmd_vk_depth_compare_op_greater(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthCompareOp(vk_command_buffer, VK_COMPARE_OP_GREATER);
}
static inline void cmd_vk_depth_compare_op_not_equal(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthCompareOp(vk_command_buffer, VK_COMPARE_OP_NOT_EQUAL);
}
static inline void cmd_vk_depth_compare_op_greater_or_equal(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthCompareOp(vk_command_buffer, VK_COMPARE_OP_GREATER_OR_EQUAL);
}
static inline void cmd_vk_depth_compare_op_always(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthCompareOp(vk_command_buffer, VK_COMPARE_OP_ALWAYS);
}

static inline void cmd_vk_depth_bounds_test_enable(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthBoundsTestEnable(vk_command_buffer, VK_TRUE);
}
static inline void cmd_vk_depth_bounds_test_disable(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthBoundsTestEnable(vk_command_buffer, VK_FALSE);
}

static inline void cmd_vk_stencil_test_enable(VkCommandBuffer vk_command_buffer) {
    vkCmdSetStencilTestEnable(vk_command_buffer, VK_TRUE);
}
static inline void cmd_vk_stencil_test_disable(VkCommandBuffer vk_command_buffer) {
    vkCmdSetStencilTestEnable(vk_command_buffer, VK_FALSE);
}
static inline void cmd_vk_stencil_op(
        VkCommandBuffer vk_command_buffer,
        VkStencilFaceFlags face_mask = VK_STENCIL_FACE_FRONT_BIT,
        VkStencilOp fail_op = VK_STENCIL_OP_KEEP,
        VkStencilOp pass_op = VK_STENCIL_OP_KEEP,
        VkStencilOp depth_fail_op = VK_STENCIL_OP_KEEP,
        VkCompareOp compare_op = VK_COMPARE_OP_NEVER) 
{
    vkCmdSetStencilOp(vk_command_buffer, face_mask, fail_op, pass_op, depth_fail_op, compare_op); 
}
static inline void cmd_vk_depth_bounds(VkCommandBuffer vk_command_buffer, float min, float max) {
    vkCmdSetDepthBounds(vk_command_buffer, min, max);
}

// `BlendState
void vkCmdSetLogicOpEnableEXT(VkCommandBuffer commandBuffer, VkBool32 logicOpEnable);
void vkCmdSetColorBlendEnableEXT(VkCommandBuffer commandBuffer, u32 firstAttachment,
    u32 attachmentCount, VkBool32 *pColorBlendEnables);
void vkCmdSetColorBlendEquationEXT(VkCommandBuffer commandBuffer, u32 firstAttachment, 
    u32 attachmentCount, const VkColorBlendEquationEXT* pColorBlendEquations);
void vkCmdSetColorWriteMaskEXT(VkCommandBuffer commandBuffer, u32 firstAttachment, 
    u32 attachmentCount, const VkColorComponentFlags* pColorWriteMasks);

static inline void cmd_vk_logic_op_enable(VkCommandBuffer vk_command_buffer) {
    // @Note this might fire a not found, even though dyn_state2 is in 1.3 core, to fix just define the PFN fetch
    vkCmdSetLogicOpEnableEXT(vk_command_buffer, VK_TRUE);
}
static inline void cmd_vk_logic_op_disable(VkCommandBuffer vk_command_buffer) {
    // @Note this might fire a not found, even though dyn_state2 is in 1.3 core, to fix just define the PFN fetch
    vkCmdSetLogicOpEnableEXT(vk_command_buffer, VK_FALSE);
}
static inline void cmd_vk_color_blend_enable_or_disables(VkCommandBuffer vk_command_buffer, 
    u32 first_attachment, u32 attachment_count, VkBool32 *enable_or_disables) {
    vkCmdSetColorBlendEnableEXT(vk_command_buffer, first_attachment, attachment_count, enable_or_disables);
}
static inline void cmd_vk_color_blend_equations(VkCommandBuffer vk_command_buffer, 
    u32 first_attachment, u32 attachment_count, VkColorBlendEquationEXT *color_blend_equations) {
    vkCmdSetColorBlendEquationEXT(vk_command_buffer, first_attachment, attachment_count, color_blend_equations);
}
static inline void cmd_vk_color_write_masks(VkCommandBuffer vk_command_buffer, 
    u32 first_attachment, u32 attachment_count, VkColorComponentFlags *color_write_masks) {
    vkCmdSetColorWriteMaskEXT(vk_command_buffer, first_attachment, attachment_count, color_write_masks);
}
static inline void cmd_vk_blend_constants(VkCommandBuffer vk_command_buffer, const float blend_constants[4]) {
    vkCmdSetBlendConstants(vk_command_buffer, blend_constants);
}

// `BlendState - Lots of inlined dyn states
struct Create_Vk_Pl_Color_Blend_State_Info {
    VkBool32  logic_op_enable; // @BoolsInStruct
    VkLogicOp logic_op;
    u32 attachment_count;
    VkPipelineColorBlendAttachmentState *attachment_states;
};
VkPipelineColorBlendStateCreateInfo create_vk_pl_color_blend_state(Create_Vk_Pl_Color_Blend_State_Info *info);

// `DynamicState
VkPipelineDynamicStateCreateInfo create_vk_pipeline_dyn_state();

// `PipelineLayout
struct Create_Vk_Pipeline_Layout_Info {
    u32 descriptor_set_layout_count;
    VkDescriptorSetLayout *descriptor_set_layouts;
    u32 push_constant_count;
    VkPushConstantRange *push_constant_ranges;
};
VkPipelineLayout* create_vk_pipeline_layouts(VkDevice vk_device, u32 count, Create_Vk_Pipeline_Layout_Info *infos);
void destroy_vk_pl_layouts(VkDevice vk_device, u32 count, VkPipelineLayout *pl_layouts);

struct Create_Vk_Rendering_Info_Info {
    u32 view_mask;
    u32 color_attachment_count;
    VkFormat *color_attachment_formats;
    VkFormat  depth_attachment_format;
    VkFormat  stencil_attachment_format;
};
VkPipelineRenderingCreateInfo create_vk_rendering_info(Create_Vk_Rendering_Info_Info *info);

// @Todo pipeline: increase possible use of dyn states, eg. vertex input, raster states etc.
// `Pipeline Final
VkPipeline* create_vk_graphics_pipelines_heap(VkDevice vk_device, VkPipelineCache cache, 
    u32 count, VkGraphicsPipelineCreateInfo *create_infos);
void destroy_vk_pipelines_heap(VkDevice vk_device, u32 count, VkPipeline *pipelines); // Also frees memory associated with the 'pipelines' pointer

#if DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_messenger_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) 
{
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::cerr << "validation layer: " << pCallbackData->pMessage << "\n\n";

    return VK_FALSE;
}

struct Create_Vk_Debug_Messenger_Info {
    VkInstance vk_instance;

    VkDebugUtilsMessageSeverityFlagsEXT severity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT    |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    VkDebugUtilsMessageTypeFlagsEXT type = 
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;

    PFN_vkDebugUtilsMessengerCallbackEXT callback = vk_debug_messenger_callback;
};

VkDebugUtilsMessengerEXT create_debug_messenger(Create_Vk_Debug_Messenger_Info *info); 

VkResult vkCreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger); 
void vkDestroyDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT messenger,
        const VkAllocationCallbacks *pAllocator); 

inline VkDebugUtilsMessengerCreateInfoEXT fill_vk_debug_messenger_info(Create_Vk_Debug_Messenger_Info *info) {
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = 
        {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};

    debug_messenger_create_info.messageSeverity = info->severity;
    debug_messenger_create_info.messageType     = info->type;
    debug_messenger_create_info.pfnUserCallback = info->callback;

    return debug_messenger_create_info;
}

#endif // DEBUG (debug messenger setup)

#endif // include guard
