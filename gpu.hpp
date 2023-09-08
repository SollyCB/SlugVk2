#ifndef SOL_GPU_HPP_INCLUDE_GUARD_
#define SOL_GPU_HPP_INCLUDE_GUARD_

#include <vulkan/vulkan_core.h>

#include "basic.h"
#include "glfw.hpp"

struct GpuInfo {
};
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
    VkSwapchainKHR vk_swapchain;
    VkSwapchainCreateInfoKHR *swapchain_info;

    VkImage *vk_images; // 2 images for presentation
    VkImageView *vk_image_views;
};
Window* get_window_instance();

void init_window(Gpu *gpu, Glfw *glfw);
void kill_window(Gpu *gpu, Window *window);

VkSurfaceKHR create_vk_surface(VkInstance vk_instance, Glfw *glfw);
void destroy_vk_surface(VkInstance vk_instance, VkSurfaceKHR vk_surface);

VkSwapchainKHR create_vk_swapchain(Gpu *gpu, Window *window); // also creates images and views and adds them to window struct
void destroy_vk_swapchain(VkDevice vk_device, Window *window); // also destroys image views

// CommandPools and CommandBuffers
struct Create_Vk_Command_Pool_Info {
    u32 queue_family_index;
    // @BoolsInStructs
    bool transient = false;
    bool reset_buffers = false;
};
void create_vk_command_pools(VkDevice vk_device, Create_Vk_Command_Pool_Info *info, u32 count, VkCommandPool *vk_command_pools);
void destroy_vk_command_pools(VkDevice vk_device, u32 count, VkCommandPool *vk_command_pools);

void reset_vk_command_pools(VkDevice vk_device, u32 count, VkCommandPool *vk_command_pools);
void reset_vk_command_pools_and_release_resources(VkDevice vk_device, u32 count, VkCommandPool *vk_command_pools);

struct Allocate_Vk_Command_Buffer_Info {
    VkCommandPool pool;
    u32 count;
    bool secondary = false;
};
void allocate_vk_command_buffers(VkDevice vk_device, Allocate_Vk_Command_Buffer_Info *info, VkCommandBuffer *vk_command_buffers);

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
void create_vk_fences_unsignalled(VkDevice vk_device, u32 count, VkFence *vk_fences);
void create_vk_fences_signalled(VkDevice vk_device, u32 count, VkFence *vk_fences);
void destroy_vk_fences(VkDevice vk_device, u32 count, VkFence *vk_fences);

void create_vk_semaphores_binary(VkDevice vk_device, u64 initial_value, u32 count, VkSemaphore *vk_semaphores);
void create_vk_semaphores_timeline(VkDevice vk_device, u64 initial_value, u32 count, VkSemaphore *vk_semaphores);
void destroy_vk_semaphores(VkDevice vk_device, u32 count, VkSemaphore *vk_semaphore);

// Descriptors
struct Create_Vk_Descriptor_Set_Layout_Info {
    u32 count;
    VkDescriptorSetLayoutBinding *bindings;
};
// add some helper function to fill in set layout bindings
VkDescriptorSetLayout create_vk_descriptor_set_layout(VkDevice vk_device, Create_Vk_Descriptor_Set_Layout_Info *info);
void destroy_vk_descriptor_set_layouts(VkDevice vk_device, u32 count, VkDescriptorSetLayout *layouts);

// Pipeline

#if DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_messenger_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) 
{
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

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
