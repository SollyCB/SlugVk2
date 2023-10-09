#ifndef SOL_GPU_HPP_INCLUDE_GUARD_
#define SOL_GPU_HPP_INCLUDE_GUARD_

#include <vulkan/vulkan_core.h>

#define VULKAN_ALLOCATOR_IS_NULL true

#if VULKAN_ALLOCATOR_IS_NULL
#define ALLOCATION_CALLBACKS_VULKAN NULL
#define ALLOCATION_CALLBACKS NULL
#endif

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "external/vk_mem_alloc.h" // vma allocator

#include "basic.h"
#include "glfw.hpp"

/* @Note
        I want to end up with separate code paths beginning right from device creation. For instance, I dont want to 
        be deciding at draw time if I need to do a host device copy via a transfer queue because I am dealing 
        with a discrete gpu (Mike Acton's, decide early). Even measuring PCIe bandwith if possible. This will 
        likely be a long time down the road but I need to keep it in mind so that adapting the code base later 
        is not completely impossible...

        Also regarding memory allocation. Is using VMA necessary? If I am using linear allocators per frame, 
        plus persistent allocations for other stuff, not really as I will very rarely be calling allocate 
        buffer, at the same time what can I gain by not using it? I can just use it to allocate a buffer once,
        and then offset that... We will see. Getting rid of it just for the sake of being self written is huge.
*/


struct GpuInfo {
    VkPhysicalDeviceLimits limits;
};
struct Gpu {
    VkInstance vk_instance;
    VmaAllocator vma_allocator;

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

// Allocator
VmaAllocator create_vma_allocator(Gpu *gpu);

// Surface and Swapchain
struct Window {
    VkSwapchainKHR vk_swapchain;
    VkSwapchainCreateInfoKHR info;
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

// Command Buffers
struct Gpu_Command_Allocator {
    VkCommandPool pool;
    int buffer_count; // Implicit cap of 64 (in_use_mask bit count)
    int cap;
    VkCommandBuffer *buffers;
};
Gpu_Command_Allocator gpu_create_command_allocator(
    VkDevice vk_device, int queue_family_index, bool transient, int size);

// Free allocator.buffers memory; Call 'vk destroy pool'
void gpu_destroy_command_allocator(VkDevice vk_device, Gpu_Command_Allocator *allocator);

// Sets in use to zero; Calls 'vk reset pool'
void gpu_reset_command_allocator(VkDevice vk_device, Gpu_Command_Allocator *allocator);

// Allocate into allocator.buffers + allocators.in_use (offset pointer); Increment in_use by count;
// Return pointer to beginning of new allocation
VkCommandBuffer* gpu_allocate_command_buffers(
    VkDevice vk_device, Gpu_Command_Allocator *allocator, int count, bool secondary);

inline static void gpu_begin_primary_command_buffer(VkCommandBuffer cmd, bool one_time) {
    VkCommandBufferBeginInfo info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    info.flags = (VkCommandBufferUsageFlags)one_time; // one time flag == 0x01
    vkBeginCommandBuffer(cmd, &info);
}
inline static void gpu_end_command_buffer(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);
}

/* Queue */
enum Gpu_Pipeline_Stage_Flags {
    GPU_PIPELINE_STAGE_TRANSFER         = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    GPU_PIPELINE_STAGE_VERTEX_INPUT     = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
    GPU_PIPELINE_STAGE_COLOR_OUTPUT     = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    GPU_PIPELINE_STAGE_ALL_COMMANDS     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    GPU_PIPELINE_STAGE_TOP_OF_PIPE      = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
    GPU_PIPELINE_STAGE_ALL_GRAPHICS     = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
};
VkSemaphoreSubmitInfo gpu_define_semaphore_submission(
    VkSemaphore semaphore, Gpu_Pipeline_Stage_Flags stage);

struct Gpu_Queue_Submit_Info {
    int wait_count;
    int signal_count;
    int cmd_count;
    VkSemaphoreSubmitInfo *wait_infos;  
    VkSemaphoreSubmitInfo *signal_infos;
    VkCommandBuffer *command_buffers;
};
VkSubmitInfo2 gpu_get_submit_info(Gpu_Queue_Submit_Info *info);

/* Sync */

// Pipeline Barriers
enum Gpu_Memory_Barrier_Setting {
    GPU_MEMORY_BARRIER_SETTING_TRANSFER_SRC,
    GPU_MEMORY_BARRIER_SETTING_VERTEX_INDEX_OWNERSHIP_TRANSFER,
    GPU_MEMORY_BARRIER_SETTING_VERTEX_BUFFER_TRANSFER_DST,
    GPU_MEMORY_BARRIER_SETTING_INDEX_BUFFER_TRANSFER_DST,
};
struct Gpu_Buffer_Barrier_Info {
    Gpu_Memory_Barrier_Setting setting;
    int src_queue;
    int dst_queue;
    VkBuffer buffer;
    u64 offset;
    u64 size;
};
VkBufferMemoryBarrier2 gpu_get_buffer_barrier(Gpu_Buffer_Barrier_Info *info);

struct Gpu_Pipeline_Barrier_Info {
    int memory_count;
    int buffer_count;
    int image_count;
    VkMemoryBarrier2       *memory_barriers;
    VkBufferMemoryBarrier2 *buffer_barriers;
    VkImageMemoryBarrier2  *image_barriers;
};
VkDependencyInfo gpu_get_pipeline_barrier(Gpu_Pipeline_Barrier_Info *info);

// Fences
struct Gpu_Fence_Pool {
    u32 len;
    u32 in_use;
    VkFence *vk_fences;
};
Gpu_Fence_Pool gpu_create_fence_pool(VkDevice vk_device, u32 size);
void gpu_destroy_fence_pool(VkDevice vk_device, Gpu_Fence_Pool *pool);

VkFence* gpu_get_fences(Gpu_Fence_Pool *pool, u32 count);
void gpu_reset_fence_pool(VkDevice vk_device, Gpu_Fence_Pool *pool);
void gpu_cut_tail_fences(Gpu_Fence_Pool *pool, u32 size);

// Semaphores
struct Gpu_Binary_Semaphore_Pool {
    u32 len;
    u32 in_use;
    VkSemaphore *vk_semaphores;
};
Gpu_Binary_Semaphore_Pool gpu_create_binary_semaphore_pool(VkDevice vk_device, u32 size); 
void gpu_destroy_semaphore_pool(VkDevice vk_device, Gpu_Binary_Semaphore_Pool *pool);

VkSemaphore* gpu_get_binary_semaphores(Gpu_Binary_Semaphore_Pool *pool, u32 count);
void gpu_reset_binary_semaphore_pool(Gpu_Binary_Semaphore_Pool *pool);
void gpu_cut_tail_binary_semaphores(Gpu_Binary_Semaphore_Pool *pool, u32 size);

// Descriptors - static, pool allocated
VkDescriptorPool create_vk_descriptor_pool(VkDevice vk_device, int max_set_count, int counts[11]);
VkResult reset_vk_descriptor_pool(VkDevice vk_device, VkDescriptorPool pool);
void destroy_vk_descriptor_pool(VkDevice vk_device, VkDescriptorPool pool);

struct Gpu_Allocate_Descriptor_Set_Info;
struct Gpu_Descriptor_Allocator {
    u16 sets_queued;
    u16 sets_allocated;
    u16 set_cap;

    VkDescriptorSetLayout *layouts;
    VkDescriptorSet       *sets;

    VkDescriptorPool pool;

    // @Todo @Note This is a trial style. These arrays make this struct verrrryy big, but also super
    // detailed so you can really see what is up with the pool. Probably better to enable this
    // functionality only in debug mode?? Or maybe this information can be useful even in release?
    // Tbf this struct isnt smtg which is an array that gets looped through all the time, for instance
    // it will only ever be being loaded once per function so the size honestly wont matter 
    // (I dont think...)
    u16 cap[11];
    u16 counts[11]; // tracks individual descriptor allocations
};
// 'count' arg corresponds to the number of descriptors for each of the first 11 descriptor types
Gpu_Descriptor_Allocator 
gpu_create_descriptor_allocator(VkDevice vk_device, int max_sets, int counts[11]);
void gpu_destroy_descriptor_allocator(VkDevice vk_device, Gpu_Descriptor_Allocator *allocator);
void gpu_reset_descriptor_allocator(VkDevice vk_device, Gpu_Descriptor_Allocator *allocator);

struct Gpu_Descriptor_List;
struct Gpu_Queue_Descriptor_Set_Allocation_Info {
    int layout_count;
    VkDescriptorSetLayout *layouts;
    int *descriptor_counts; // array of len 11
};
VkDescriptorSet* gpu_queue_descriptor_set_allocation(
    Gpu_Descriptor_Allocator *allocator, Gpu_Queue_Descriptor_Set_Allocation_Info *info, VkResult *result);

void gpu_allocate_descriptor_sets(VkDevice vk_device, Gpu_Descriptor_Allocator *allocator);

struct Create_Vk_Descriptor_Set_Layout_Info {
    int count;
    VkDescriptorSetLayoutBinding *bindings;
};
VkDescriptorSetLayout* create_vk_descriptor_set_layouts(VkDevice vk_device, int count, Create_Vk_Descriptor_Set_Layout_Info *infos);
void gpu_destroy_descriptor_set_layouts(VkDevice vk_device, int count, VkDescriptorSetLayout *layouts);
void gpu_destroy_descriptor_set_layouts(VkDevice vk_device, int count, Gpu_Allocate_Descriptor_Set_Info *layouts);

struct Gpu_Descriptor_List {
    int counts[11]; // count per descriptor type
};
Gpu_Descriptor_List gpu_make_descriptor_list(int count, Create_Vk_Descriptor_Set_Layout_Info *infos);


// Descriptors - buffer, dynamic

//Gpu_Allocate_Descriptor_Set_Info* create_vk_descriptor_set_layouts(VkDevice vk_device, int count, Create_Vk_Descriptor_Set_Layout_Info *binding_info);
//void destroy_vk_descriptor_set_layouts(VkDevice vk_device, int count, VkDescriptorSetLayout *layouts);

// Pipeline Setup
// `ShaderStages
struct Create_Vk_Pipeline_Shader_Stage_Info {
    u64 code_size;
    const u32 *shader_code;
    VkShaderStageFlagBits stage;
    VkSpecializationInfo *spec_info = NULL;
};
VkPipelineShaderStageCreateInfo* create_vk_pipeline_shader_stages(VkDevice vk_device, u32 count, Create_Vk_Pipeline_Shader_Stage_Info *infos);
void destroy_vk_pipeline_shader_stages(VkDevice vk_device, u32 count, VkPipelineShaderStageCreateInfo *stages);

// `VertexInputState
// @StructPacking pack struct
struct Create_Vk_Vertex_Input_Binding_Description_Info {
    u32 binding;
    u32 stride;
    // @Todo support instance input rate
};
VkVertexInputBindingDescription create_vk_vertex_binding_description(Create_Vk_Vertex_Input_Binding_Description_Info *info);
enum Vec_Type {
    VEC_TYPE_1 = VK_FORMAT_R32_SFLOAT,
    VEC_TYPE_2 = VK_FORMAT_R32G32_SFLOAT,
    VEC_TYPE_3 = VK_FORMAT_R32G32B32_SFLOAT,
    VEC_TYPE_4 = VK_FORMAT_R32G32B32A32_SFLOAT,
};
struct Create_Vk_Vertex_Input_Attribute_Description_Info {
    u32 location;
    u32 binding;
    VkFormat format;
    u32 offset;
};
VkVertexInputAttributeDescription create_vk_vertex_attribute_description(Create_Vk_Vertex_Input_Attribute_Description_Info *info);

struct Create_Vk_Pipeline_Vertex_Input_State_Info {
    u32 binding_count;
    u32 attribute_count;
    VkVertexInputBindingDescription *binding_descriptions;
    VkVertexInputAttributeDescription *attribute_descriptions;
};
VkPipelineVertexInputStateCreateInfo create_vk_pipeline_vertex_input_states(Create_Vk_Pipeline_Vertex_Input_State_Info *info);

// `InputAssemblyState
VkPipelineInputAssemblyStateCreateInfo create_vk_pipeline_input_assembly_state(VkPrimitiveTopology topology, VkBool32 primitive_restart);

// `TessellationState
// @Todo support Tessellation

// Viewport
VkPipelineViewportStateCreateInfo create_vk_pipeline_viewport_state(Window *window);

// `RasterizationState
VkPipelineRasterizationStateCreateInfo create_vk_pipeline_rasterization_state(VkPolygonMode polygon_mode, VkCullModeFlags cull_mode, VkFrontFace front_face);
void vkCmdSetDepthClampEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClampEnable);
void vkCmdSetPolygonModeEXT(VkCommandBuffer commandBuffer, VkPolygonMode polygonMode);
// `MultisampleState // @Todo support setting multisampling functions
//struct Create_Vk_Pipeline_Multisample_State_Info {};
VkPipelineMultisampleStateCreateInfo create_vk_pipeline_multisample_state(VkSampleCountFlagBits sample_count);

// `DepthStencilState
struct Create_Vk_Pipeline_Depth_Stencil_State_Info {
    VkBool32 depth_test_enable;
    VkBool32 depth_write_enable;
    VkBool32 depth_bounds_test_enable;
    VkCompareOp depth_compare_op;
    float min_depth_bounds;
    float max_depth_bounds;
};
VkPipelineDepthStencilStateCreateInfo create_vk_pipeline_depth_stencil_state(Create_Vk_Pipeline_Depth_Stencil_State_Info *info);

// `BlendState
void vkCmdSetLogicOpEnableEXT(VkCommandBuffer commandBuffer, VkBool32 logicOpEnable);
void vkCmdSetColorBlendEnableEXT(VkCommandBuffer commandBuffer, u32 firstAttachment,
        u32 attachmentCount, VkBool32 *pColorBlendEnables);
void vkCmdSetColorBlendEquationEXT(VkCommandBuffer commandBuffer, u32 firstAttachment, 
        u32 attachmentCount, const VkColorBlendEquationEXT* pColorBlendEquations);
void vkCmdSetColorWriteMaskEXT(VkCommandBuffer commandBuffer, u32 firstAttachment, 
        u32 attachmentCount, const VkColorComponentFlags* pColorWriteMasks);

// `BlendState
struct Create_Vk_Pipeline_Color_Blend_State_Info {
    u32 attachment_count;
    VkPipelineColorBlendAttachmentState *attachment_states;
};
VkPipelineColorBlendStateCreateInfo create_vk_pipeline_color_blend_state(Create_Vk_Pipeline_Color_Blend_State_Info *info);

// `DynamicState
VkPipelineDynamicStateCreateInfo create_vk_pipeline_dyn_state();

// `PipelineLayout
struct Create_Vk_Pipeline_Layout_Info {
    u32 descriptor_set_layout_count;
    VkDescriptorSetLayout *descriptor_set_layouts;
    u32 push_constant_count;
    VkPushConstantRange *push_constant_ranges;
};
VkPipelineLayout create_vk_pipeline_layout(VkDevice vk_device, Create_Vk_Pipeline_Layout_Info *info);
void destroy_vk_pipeline_layout(VkDevice vk_device, VkPipelineLayout pl_layout);

// PipelineRenderingInfo
struct Create_Vk_Pipeline_Rendering_Info_Info {
    u32 view_mask;
    u32 color_attachment_count;
    VkFormat *color_attachment_formats;
    VkFormat  depth_attachment_format;
    VkFormat  stencil_attachment_format;
};
VkPipelineRenderingCreateInfo create_vk_pipeline_rendering_info(Create_Vk_Pipeline_Rendering_Info_Info *info);

/** `Pipeline Final -- static / descriptor pools **/

// Pl_Stage_1
struct Gpu_Vertex_Input_State {
    VkPrimitiveTopology topology;
    int input_binding_description_count;
    int input_attribute_description_count;

    // binding description info
    int *binding_description_bindings;
    int *binding_description_strides;

    // attribute description info
    int *attribute_description_locations;
    int *attribute_description_bindings;
    VkFormat *formats;
};

// Pl_Stage_2
enum Gpu_Polygon_Mode_Flag_Bits {
    GPU_POLYGON_MODE_FILL_BIT  = 0x01,
    GPU_POLYGON_MODE_LINE_BIT  = 0x02,
    GPU_POLYGON_MODE_POINT_BIT = 0x04,
};
typedef u8 Gpu_Polygon_Mode_Flags;

struct Gpu_Rasterization_State {
    int polygon_mode_count;
    VkPolygonMode polygon_modes[3];
    VkCullModeFlags cull_mode;
    VkFrontFace front_face;
};

// Pl_Stage_3
enum Gpu_Fragment_Shader_Flag_Bits {
    GPU_FRAGMENT_SHADER_DEPTH_TEST_ENABLE_BIT         = 0x01,
    GPU_FRAGMENT_SHADER_DEPTH_WRITE_ENABLE_BIT        = 0x02,
    GPU_FRAGMENT_SHADER_DEPTH_BOUNDS_TEST_ENABLE_BIT  = 0x04,
};
typedef u8 Gpu_Fragment_Shader_Flags;

struct Gpu_Fragment_Shader_State {
    u32 flags;
    VkCompareOp depth_compare_op;
    VkSampleCountFlagBits sample_count;
    float min_depth_bounds;
    float max_depth_bounds;
};

// Pl_Stage_4
enum Gpu_Blend_Setting {
    GPU_BLEND_SETTING_OPAQUE_FULL_COLOR = 0,
};
struct Gpu_Fragment_Output_State {
    VkPipelineColorBlendAttachmentState blend_state;
};

struct Create_Vk_Pipeline_Info {
    int subpass;

    int shader_stage_count;
    VkPipelineShaderStageCreateInfo *shader_stages;

    Gpu_Vertex_Input_State    *vertex_input_state;
    Gpu_Rasterization_State   *rasterization_state;
    Gpu_Fragment_Shader_State *fragment_shader_state;
    Gpu_Fragment_Output_State  *fragment_output_state;

    VkPipelineLayout layout;
    VkRenderPass renderpass;
};
void create_vk_graphics_pipelines(VkDevice vk_device, VkPipelineCache, int count, Create_Vk_Pipeline_Info *infos, VkPipeline *pipelines);
void gpu_destroy_pipeline(VkDevice vk_device, VkPipeline pipeline);

// `Pipeline Final -- dynamic
VkPipeline* create_vk_graphics_pipelines_heap(VkDevice vk_device, VkPipelineCache cache, 
        u32 count, VkGraphicsPipelineCreateInfo *create_infos);
void destroy_vk_pipelines_heap(VkDevice vk_device, u32 count, VkPipeline *pipelines); // Also frees memory associated with the 'pipelines' pointer

// `Static Rendering (framebuffers, renderpass, subpasses)

/* Begin Better Automate Rendering */ 

enum Gpu_Image_Layout {
    GPU_IMAGE_LAYOUT_COLOR = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    GPU_IMAGE_LAYOUT_DEPTH_STENCIL = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
};
enum Gpu_Resolve_Bits {
    GPU_RESOLVE_COLOR_BIT = 0x01,
    GPU_RESOLVE_DEPTH_STENCIL_BIT = 0x02,
};
typedef u8 Gpu_Resolve_Flags;
struct Gpu_Renderpass_Info {
    Gpu_Resolve_Flags resolve_flags;
    Gpu_Resolve_Flags input_flags; // if input_attachment count > 0 && flags == 0x0, assume color input
    int sample_count;
    int input_attachment_count;
    int color_attachment_count;
    bool32 no_depth_attachment;
    Gpu_Image_Layout *resolve_layouts; // Ignored unless flags is both depth and color
    Gpu_Image_Layout *input_layouts; // Ignored unless flags is both depth and color
};

VkRenderPass gpu_create_single_renderpass_graphics();

// @Unimplemented
VkRenderPass gpu_create_first_renderpass_graphics();
VkRenderPass gpu_create_subsequent_renderpass_graphics();
VkRenderPass gpu_create_final_renderpass_graphics();
VkRenderPass gpu_create_renderpass_deferred();

/* End Better Automate Rendering */

enum Gpu_Attachment_Description_Setting {
    GPU_ATTACHMENT_DESCRIPTION_SETTING_COLOR_LOAD_UNDEFINED_STORE,
    GPU_ATTACHMENT_DESCRIPTION_SETTING_COLOR_LOAD_OPTIMAL_STORE,
    GPU_ATTACHMENT_DESCRIPTION_SETTING_DEPTH_LOAD_UNDEFINED_STORE,
    GPU_ATTACHMENT_DESCRIPTION_SETTING_DEPTH_LOAD_OPTIMAL_STORE,
};
struct Gpu_Attachment_Description_Info {
    VkFormat format;
    Gpu_Attachment_Description_Setting setting;
};
VkAttachmentDescription gpu_get_attachment_description(Gpu_Attachment_Description_Info *info);

struct Create_Vk_Subpass_Description_Info {
    u32 input_attachment_count;
    VkAttachmentReference *input_attachments;
    u32 color_attachment_count;
    VkAttachmentReference *color_attachments;
    VkAttachmentReference *resolve_attachments;
    VkAttachmentReference *depth_stencil_attachment;

    // @Todo preserve attachments
};
VkSubpassDescription create_vk_graphics_subpass_description(Create_Vk_Subpass_Description_Info *info);

enum Gpu_Subpass_Dependency_Setting {
    GPU_SUBPASS_DEPENDENCY_SETTING_ACQUIRE_TO_RENDER_TARGET_BASIC,
    GPU_SUBPASS_DEPENDENCY_SETTING_COLOR_DEPTH_BASIC_DRAW,
    GPU_SUBPASS_DEPENDENCY_SETTING_WRITE_READ_COLOR_FRAGMENT,
    GPU_SUBPASS_DEPENDENCY_SETTING_WRITE_READ_DEPTH_FRAGMENT,
};
struct Create_Vk_Subpass_Dependency_Info {
    Gpu_Subpass_Dependency_Setting access_rules;
    u32 src_subpass;
    u32 dst_subpass;
};
VkSubpassDependency create_vk_subpass_dependency(Create_Vk_Subpass_Dependency_Info *info); 

struct Create_Vk_Renderpass_Info {
    u32 attachment_count;
    u32 subpass_count;
    u32 dependency_count;
    VkAttachmentDescription *attachments;
    VkSubpassDescription    *subpasses;
    VkSubpassDependency     *dependencies;
};
VkRenderPass create_vk_renderpass(VkDevice vk_device, Create_Vk_Renderpass_Info *info);
void destroy_vk_renderpass(VkDevice vk_device, VkRenderPass renderpass);

struct Gpu_Framebuffer_Info {
    VkRenderPass renderpass;
    int attachment_count;
    VkImageView *attachments;
    int width;
    int height;
};
VkFramebuffer gpu_create_framebuffer(VkDevice vk_device, Gpu_Framebuffer_Info *info);
void gpu_destroy_framebuffer(VkDevice vk_device, VkFramebuffer framebuffer);


// `Rendering Dyn
struct Create_Vk_Rendering_Attachment_Info_Info {
    VkImageView image_view;
    // Pretty sure this is left null for now (no multisampling yet...)
    VkResolveModeFlagBits resolve_mode;
    VkImageView resolve_image_view;
    VkImageLayout resolve_image_layout;
    float *clear_color;
};
VkRenderingAttachmentInfo create_vk_rendering_attachment_info(Create_Vk_Rendering_Attachment_Info_Info *info);

struct Create_Vk_Rendering_Info_Info {
    // @Todo support:
    //     flags for secondary command buffers,
    //     layered attachments (layer count and view mask)
    VkRect2D render_area; 
    u32 color_attachment_count;
    VkRenderingAttachmentInfo *color_attachment_infos;
    VkRenderingAttachmentInfo *depth_attachment_info;
    VkRenderingAttachmentInfo *stencil_attachment_info;

    u32 view_mask;
    u32 layer_count = 1; // I think 1 is correct??...
};
// @Note the names on these sorts of functions might be misleading, as create implies the need to destroy...
VkRenderingInfo create_vk_rendering_info(Create_Vk_Rendering_Info_Info *info);

// Inline Cmds - (Going to be heavily culled / edited in near future... - sol 8 oct, 2023)
static inline void cmd_vk_set_scissor(VkCommandBuffer vk_command_buffer) {
    VkSwapchainCreateInfoKHR info = get_window_instance()->info;
    VkRect2D scissor = {
        0, 0, // x, y
        info.imageExtent.width,
        info.imageExtent.height,
    };
    vkCmdSetScissorWithCount(vk_command_buffer, 1, &scissor);
}
static inline void cmd_vk_set_viewport(VkCommandBuffer vk_command_buffer) {
    VkSwapchainCreateInfoKHR info = get_window_instance()->info;
    VkViewport viewport = {
        0.0f, 0.0f, // x, y
        (float)info.imageExtent.width,
        (float)info.imageExtent.height,
        0.0f, 1.0f, // mindepth, maxdepth
    };
    vkCmdSetViewportWithCount(vk_command_buffer, 1, &viewport);
}

static inline void cmd_vk_enable_depth_clamp(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthClampEnableEXT(vk_command_buffer, VK_TRUE);
}
static inline void cmd_vk_disable_depth_clamp(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthClampEnableEXT(vk_command_buffer, VK_FALSE);
}

static inline void cmd_vk_set_rasterizer_discard_enabled(VkCommandBuffer vk_command_buffer) {
    vkCmdSetRasterizerDiscardEnable(vk_command_buffer, VK_TRUE);
}
static inline void cmd_vk_set_rasterizer_discard_disabled(VkCommandBuffer vk_command_buffer) {
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

static inline void cmd_vk_set_cull_mode_none(VkCommandBuffer vk_command_buffer) {
    vkCmdSetCullMode(vk_command_buffer, VK_CULL_MODE_NONE);
}
static inline void cmd_vk_set_cull_mode_front(VkCommandBuffer vk_command_buffer) {
    vkCmdSetCullMode(vk_command_buffer, VK_CULL_MODE_FRONT_BIT);
}
static inline void cmd_vk_set_cull_mode_back(VkCommandBuffer vk_command_buffer) {
    vkCmdSetCullMode(vk_command_buffer, VK_CULL_MODE_BACK_BIT);
}
static inline void cmd_vk_set_cull_mode_front_and_back(VkCommandBuffer vk_command_buffer) {
    vkCmdSetCullMode(vk_command_buffer, VK_CULL_MODE_FRONT_AND_BACK);
}

static inline void cmd_vk_set_front_face_clockwise(VkCommandBuffer vk_command_buffer) {
    vkCmdSetFrontFace(vk_command_buffer, VK_FRONT_FACE_CLOCKWISE);
}
static inline void cmd_vk_set_front_face_counter_clockwise(VkCommandBuffer vk_command_buffer) {
    vkCmdSetFrontFace(vk_command_buffer, VK_FRONT_FACE_COUNTER_CLOCKWISE);
}

static inline void cmd_vk_set_depth_bias_enabled(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthBiasEnable(vk_command_buffer, VK_TRUE);
}
static inline void cmd_vk_set_depth_bias_disabled(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthBiasEnable(vk_command_buffer, VK_FALSE);
}

static inline void cmd_vk_line_width(VkCommandBuffer vk_command_buffer, float width) {
    vkCmdSetLineWidth(vk_command_buffer, width);
}

static inline void cmd_vk_set_depth_test_enabled(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthTestEnable(vk_command_buffer, VK_TRUE);
}
static inline void cmd_vk_set_depth_test_disabled(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthTestEnable(vk_command_buffer, VK_FALSE);
}
static inline void cmd_vk_set_depth_write_enabled(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthWriteEnable(vk_command_buffer, VK_TRUE);
}
static inline void cmd_vk_set_depth_write_disabled(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthWriteEnable(vk_command_buffer, VK_FALSE);
}

static inline void cmd_vk_set_depth_compare_op_never(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthCompareOp(vk_command_buffer, VK_COMPARE_OP_NEVER);
}
static inline void cmd_vk_set_depth_compare_op_less(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthCompareOp(vk_command_buffer, VK_COMPARE_OP_LESS);
}
static inline void cmd_vk_set_depth_compare_op_equal(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthCompareOp(vk_command_buffer, VK_COMPARE_OP_EQUAL);
}
static inline void cmd_vk_set_depth_compare_op_less_or_equal(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthCompareOp(vk_command_buffer, VK_COMPARE_OP_LESS_OR_EQUAL);
}
static inline void cmd_set_vk_set_depth_compare_op_greater(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthCompareOp(vk_command_buffer, VK_COMPARE_OP_GREATER);
}
static inline void cmd_set_vk_depth_compare_op_not_equal(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthCompareOp(vk_command_buffer, VK_COMPARE_OP_NOT_EQUAL);
}
static inline void cmd_set_vk_depth_compare_op_greater_or_equal(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthCompareOp(vk_command_buffer, VK_COMPARE_OP_GREATER_OR_EQUAL);
}
static inline void cmd_set_vk_depth_compare_op_always(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthCompareOp(vk_command_buffer, VK_COMPARE_OP_ALWAYS);
}

static inline void cmd_vk_set_depth_bounds_test_enabled(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthBoundsTestEnable(vk_command_buffer, VK_TRUE);
}
static inline void cmd_vk_set_depth_bounds_test_disabled(VkCommandBuffer vk_command_buffer) {
    vkCmdSetDepthBoundsTestEnable(vk_command_buffer, VK_FALSE);
}

static inline void cmd_vk_set_stencil_test_enabled(VkCommandBuffer vk_command_buffer) {
    vkCmdSetStencilTestEnable(vk_command_buffer, VK_TRUE);
}
static inline void cmd_vk_set_stencil_test_disabled(VkCommandBuffer vk_command_buffer) {
    vkCmdSetStencilTestEnable(vk_command_buffer, VK_FALSE);
}
static inline void cmd_vk_set_stencil_op(
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

static inline void cmd_vk_set_topology_triangle_list(VkCommandBuffer vk_command_buffer) {
    vkCmdSetPrimitiveTopology(vk_command_buffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
}
static inline void cmd_vk_set_topology_line_list(VkCommandBuffer vk_command_buffer) {
    vkCmdSetPrimitiveTopology(vk_command_buffer, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
}
static inline void cmd_vk_set_primitive_restart_enabled(VkCommandBuffer vk_command_buffer) {
    vkCmdSetPrimitiveRestartEnable(vk_command_buffer, VK_TRUE);
}
static inline void cmd_vk_set_primitive_restart_disabled(VkCommandBuffer vk_command_buffer) {
    vkCmdSetPrimitiveRestartEnable(vk_command_buffer, VK_FALSE);
}

void set_default_draw_state(VkCommandBuffer vk_command_buffer); // calls lots of state cmds

struct Dyn_Vertex_Bind_Info {
    u32 first_binding;
    u32 binding_count;
    VkBuffer *buffers;
    VkDeviceSize *offsets;
    VkDeviceSize *sizes;
    VkDeviceSize *strides;
};
static inline void cmd_vk_bind_vertex_buffers2(VkCommandBuffer vk_command_buffer, Dyn_Vertex_Bind_Info *info) {
    vkCmdBindVertexBuffers2(
            vk_command_buffer, 
            info->first_binding,
            info->binding_count, 
            info->buffers,
            info->offsets,
            info->sizes,
            info->strides);
}

// Resources
struct Gpu_Buffer {
    VkBuffer vk_buffer;
    VmaAllocation vma_allocation;
};

static inline void* get_vma_mapped_ptr(VmaAllocator vma_allocator, Gpu_Buffer *gpu_buffer) {
    VmaAllocationInfo info;
    vmaGetAllocationInfo(vma_allocator, gpu_buffer->vma_allocation, &info);
    return info.pMappedData;
}
void destroy_vma_buffer(VmaAllocator vma_allocator, Gpu_Buffer *gpu_buffer);

// Buffer Allocators - @Todo Different code paths for unified vs discrete memory
struct Gpu_Linear_Allocator {
    u64 offset;
    u64 used;
    u64 cap;
    VkBuffer buffer;
    VmaAllocation vma_allocation;
    void *mapped_ptr;
};
enum Gpu_Allocator_Type {
    GPU_ALLOCATOR_TYPE_BUFFER_GENERAL_SRC = 
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | 
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT  |
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    GPU_ALLOCATOR_TYPE_BUFFER_GENERAL_DST = 
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | 
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT  |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,

    GPU_ALLOCATOR_TYPE_VERTEX       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    GPU_ALLOCATOR_TYPE_INDEX        = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    GPU_ALLOCATOR_TYPE_UNIFORM      = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    GPU_ALLOCATOR_TYPE_STORAGE      = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    GPU_ALLOCATOR_TYPE_TRANSFER_SRC = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    GPU_ALLOCATOR_TYPE_TRANSFER_DST = VK_BUFFER_USAGE_TRANSFER_DST_BIT,

    GPU_ALLOCATOR_TYPE_VERTEX_TRANSFER_SRC = 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    GPU_ALLOCATOR_TYPE_VERTEX_TRANSFER_DST = 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,

    GPU_ALLOCATOR_TYPE_INDEX_TRANSFER_DST = 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    GPU_ALLOCATOR_TYPE_INDEX_TRANSFER_SRC = 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,

    GPU_ALLOCATOR_TYPE_UNIFORM_TRANSFER_DST = 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    GPU_ALLOCATOR_TYPE_UNIFORM_TRANSFER_SRC = 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,

    GPU_ALLOCATOR_TYPE_STORAGE_TRANSFER_DST = 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    GPU_ALLOCATOR_TYPE_STORAGE_TRANSFER_SRC = 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
};
// Memory is close to the host
Gpu_Linear_Allocator gpu_create_linear_allocator_host(
    VmaAllocator vma_allocator, u64 size, Gpu_Allocator_Type usage_type);
// Memory is close to the device
Gpu_Linear_Allocator gpu_create_linear_allocator_device(
    VmaAllocator vma_allocator, u64 size, Gpu_Allocator_Type usage_type);
// Shutdown an allocator
void gpu_destroy_linear_allocator(
    VmaAllocator vma_allocator, Gpu_Linear_Allocator *linear_allocator);
// Return mapped ptr + offset into allocation; returns actual offset in 'offset'
void* gpu_make_linear_allocation(Gpu_Linear_Allocator *allocator, u64 size, u64 *offset);
// Reduce used by size
void gpu_cut_tail_linear_allocator(Gpu_Linear_Allocator *allocator, u64 size);
// Set used to zero
void gpu_reset_linear_allocator(Gpu_Linear_Allocator *allocator);
// Make allocation in 'to' allocator; Return copy information
VkBufferCopy gpu_linear_allocator_setup_copy(
    Gpu_Linear_Allocator *to_allocator, u64 offset_into_src_buffer, u64 size);

// Get mapped pointer to beginning of gpu linear allocator
inline static void* gpu_linear_allocator_get_ptr(VmaAllocator vma_allocator, Gpu_Linear_Allocator *linear_allocator) {
    VmaAllocationInfo info;
    vmaGetAllocationInfo(vma_allocator, linear_allocator->vma_allocation, &info);
    return info.pMappedData;
}

enum Gpu_Resource_Setting {
    GPU_RESOURCE_SETTING_HOST_LOCAL_VERTEX   = 0,
    GPU_RESOURCE_SETTING_DEVICE_LOCAL_VERTEX = 1,
};
// @Unimplemented
Gpu_Buffer gpu_create_buffer(VmaAllocator vma_allocator, Gpu_Resource_Setting setting, u64 size);
Gpu_Buffer gpu_create_image(VmaAllocator vma_allocator, Gpu_Resource_Setting setting, u64 size);

// Images
struct Gpu_Image {
    VkImage vk_image;
    VmaAllocation vma_allocation;
};
enum Gpu_Image_Type {
    GPU_IMAGE_TYPE_COLOR_ATTACHMENT,
    GPU_IMAGE_TYPE_DEPTH_ATTACHMENT,
    GPU_IMAGE_TYPE_TEXTURE,
};
Gpu_Image gpu_create_depth_attachment(VmaAllocator vma_allocator, int width, int height);
// For textures
Gpu_Image gpu_create_image(VmaAllocator vma_allocator, int width, int height, Gpu_Image_Type type);
void gpu_destroy_image(VmaAllocator vma_allocator, Gpu_Image *image);

VkImageView gpu_create_depth_attachment_view(VkDevice vk_device, VkImage vk_image);
void gpu_destroy_image_view(VkDevice vk_device, VkImageView view);


#if DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_messenger_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) 
{
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        println("\nValidation Layer: %c", pCallbackData->pMessage);
        //std::cout << "Validation Layer: " << pCallbackData->pMessage << "\n";

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
