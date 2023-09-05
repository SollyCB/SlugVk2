#include "gpu.hpp"
#include "vulkan_errors.hpp"
#include "glfw.hpp"

#if DEBUG
static VkDebugUtilsMessengerEXT s_vk_debug_messenger;
VkDebugUtilsMessengerEXT* get_vk_debug_messenger_instance() { return &s_vk_debug_messenger; }
#endif

#define VULKAN_ALLOCATOR_IS_NULL true

#if VULKAN_ALLOCATOR_IS_NULL
#define ALLOCATION_CALLBACKS NULL
#endif

static Gpu *s_Gpu;
Gpu* get_gpu_instance() { return s_Gpu; }

void init_gpu() {
    // This is ugly but sometimes ugly is best
    s_Gpu = (Gpu*)memory_allocate_heap(
            sizeof(Gpu) +
            sizeof(GpuInfo) +
            (sizeof(VkQueue) * 3) +
            (sizeof(u32) * 3),
            8);

    Gpu *gpu = get_gpu_instance();
    gpu->vk_queues = (VkQueue*)(gpu + 1);
    gpu->vk_queue_indices = (u32*)(gpu->vk_queues + 3);
    gpu->info = (GpuInfo*)(gpu->vk_queue_indices + 3);

    Create_Vk_Instance_Info create_instance_info = {};
    gpu->vk_instance = create_vk_instance(&create_instance_info);

#if DEBUG
    Create_Vk_Debug_Messenger_Info create_debug_messenger_info = {gpu->vk_instance};
    *get_vk_debug_messenger_instance() = create_debug_messenger(&create_debug_messenger_info);
#endif

    // Info for this function is set in the function body, as lots of feature structs have to be specified awkwardly
    // This function also creates the device queues and adds them to the gpu struct
    gpu->vk_device = create_vk_device(gpu);

}
void kill_gpu(Gpu *gpu) {
    vkDestroyDevice(gpu->vk_device, ALLOCATION_CALLBACKS);
#if DEBUG
    vkDestroyDebugUtilsMessengerEXT(gpu->vk_instance, *get_vk_debug_messenger_instance(), ALLOCATION_CALLBACKS);
#endif
    vkDestroyInstance(gpu->vk_instance, ALLOCATION_CALLBACKS);
    memory_free_heap(gpu);
}

// *Instance
VkInstance create_vk_instance(Create_Vk_Instance_Info *info) {
    VkInstanceCreateInfo instance_create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; 
#if DEBUG
    Create_Vk_Debug_Messenger_Info debug_messenger_info = {};
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = fill_vk_debug_messenger_info(&debug_messenger_info);
    instance_create_info.pNext = &debug_messenger_create_info;
#endif

    // App Info
    VkApplicationInfo application_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application_info.pApplicationName = info->application_name;
    application_info.applicationVersion = VK_MAKE_VERSION(info->application_version_major, info->application_version_middle, info->application_version_minor);
    application_info.pEngineName = info->application_name;
    application_info.engineVersion = VK_MAKE_VERSION(info->engine_version_major, info->engine_version_middle, info->engine_version_minor);
    application_info.apiVersion = info->vulkan_api_version;

    // assign app info to instance create info
    instance_create_info.pApplicationInfo = &application_info;

    // Ugly preprocessor stuff at setup:
    //      All it says is: 
    //       if DEBUG is defined true, layer count and extension count are decreased, corresponding name lists lose these names
#if DEBUG
    u32 layer_count = 1;
#else 
    u32 layer_count = 0;
#endif

    const char *layer_names[] = {
#if DEBUG
        "VK_LAYER_KHRONOS_validation"
#endif
    };

#if DEBUG
    u32 ext_count = 2;
#else
    u32 ext_count = 0; // Always -2 from DEBUG version
#endif

    const char *ext_names[] = {

#if DEBUG
        "VK_EXT_validation_features",
        "VK_EXT_debug_utils",
#endif
    };

    u32 glfw_ext_count;
    const char **glfw_ext_names = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

    u8 char_index = 0;
    u8 name_index = 0;
    char ext_name_buffer[250]; // Assume char total in ext names < 251
    char *ext_names_final[20]; // Assume fewer than 20 ext names

    char *name;
    for(int i = 0; i < glfw_ext_count; ++i) {
        name = strcpy(ext_name_buffer + char_index, glfw_ext_names[i]);
        ext_names_final[i] = name;
        char_index += strlen(name) + 1;
        name_index++;
    }
    for(int i = 0; i < ext_count; ++i) {
        name = strcpy(ext_name_buffer + char_index, ext_names[i]);
        ext_names_final[name_index] = name;
        char_index += strlen(name) + 1;
        name_index++;
    }

    instance_create_info.enabledExtensionCount = ext_count + glfw_ext_count;
    instance_create_info.ppEnabledExtensionNames = ext_names_final;

    instance_create_info.enabledLayerCount = layer_count;
    instance_create_info.ppEnabledLayerNames = layer_names;

    VkInstance vk_instance;
    auto check = vkCreateInstance(&instance_create_info, ALLOCATION_CALLBACKS, &vk_instance);
    DEBUG_OBJ_CREATION(vkCreateInstance, check);

    return vk_instance;
}

// *Device ///////////
VkDevice create_vk_device(Gpu *gpu) { // returns logical device, silently fills in gpu.physical_device

    uint32_t ext_count = 3;
    const char *ext_names[] = {
        "VK_KHR_swapchain",
        "VK_KHR_dynamic_rendering",
        "VK_EXT_descriptor_buffer",

        // @Todo stuff to look at later to ease pipelining (shader objects too new, graphics libs unsupported..
        //"VK_EXT_extended_dynamic_state",
        //"VK_EXT_extended_dynamic_state2",
        //"VK_EXT_extended_dynamic_state3",
        //"VK_EXT_vertex_input_dynamic_state",
    };

    VkPhysicalDeviceFeatures vk1_features = {
        .samplerAnisotropy = VK_TRUE,
    };
    VkPhysicalDeviceVulkan12Features vk12_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
    };
    VkPhysicalDeviceVulkan13Features vk13_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = (void*)&vk12_features,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };
    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptor_buffer_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
        .pNext = (void*)&vk13_features,
        .descriptorBuffer = VK_TRUE,
        .descriptorBufferCaptureReplay = VK_FALSE,
        .descriptorBufferImageLayoutIgnored = VK_FALSE,
        .descriptorBufferPushDescriptors = VK_TRUE,
    };
    VkPhysicalDeviceFeatures2 features_full = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &descriptor_buffer_features,
        .features = vk1_features,
    };

    VkPhysicalDeviceFeatures vk1_features_unfilled = vk1_features;
    VkPhysicalDeviceVulkan12Features vk12_features_unfilled = vk12_features;

    VkPhysicalDeviceVulkan13Features vk13_features_unfilled = vk13_features;
    vk13_features_unfilled.pNext = &vk12_features_unfilled;

    VkPhysicalDeviceFeatures2 features_full_unfilled = features_full;
    features_full_unfilled.pNext = &descriptor_buffer_features;
 
    features_full_unfilled.features = vk1_features_unfilled;

    // choose physical device
    u32 physical_device_count;
    vkEnumeratePhysicalDevices(gpu->vk_instance, &physical_device_count, NULL);
    VkPhysicalDevice *physical_devices =
        (VkPhysicalDevice*)memory_allocate_temp(sizeof(VkPhysicalDevice) * physical_device_count, 8);
    vkEnumeratePhysicalDevices(gpu->vk_instance, &physical_device_count, physical_devices);

    int graphics_queue_index;
    int transfer_queue_index;
    int presentation_queue_index;
    int physical_device_index = -1;

    int backup_graphics_queue_index = -1;
    int backup_presentation_queue_index = -1;
    int backup_physical_device_index = -1;

    // @Todo prefer certain gpus eg discrete
    for(int i = 0; i < physical_device_count; ++i) {

        vkGetPhysicalDeviceFeatures2(physical_devices[i], &features_full);

        if (descriptor_buffer_features.descriptorBuffer == VK_FALSE) {
            std::cerr << "Device Index " << i << " does not support descriptorBuffer, continuing\n";
            continue;
        }
        if (vk13_features.dynamicRendering == VK_FALSE) {
            std::cerr << "Device Index " << i << " does not support dynamicRendering, continuing\n";
            continue;
        }

        u32 queue_family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_count, NULL);

        VkQueueFamilyProperties *queue_family_props = 
            (VkQueueFamilyProperties*)memory_allocate_temp(sizeof(VkQueueFamilyProperties) * queue_family_count, 8);;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_count, queue_family_props);

        graphics_queue_index = -1;
        transfer_queue_index = -1;
        presentation_queue_index = -1;

        bool break_outer = false;
        for(int j = 0; j < queue_family_count;++j) {
            if (glfwGetPhysicalDevicePresentationSupport(gpu->vk_instance, physical_devices[i], j) &&
                presentation_queue_index == -1) 
            {
                presentation_queue_index = j;
            }
            if (queue_family_props[j].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
                queue_family_props[j].queueFlags & VK_QUEUE_TRANSFER_BIT &&
                graphics_queue_index == -1)
            {
                graphics_queue_index = j;
            }
            if (queue_family_props[j].queueFlags & VK_QUEUE_TRANSFER_BIT    &&
                !(queue_family_props[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                transfer_queue_index == -1)
            {
                transfer_queue_index = j;
            }
            if (transfer_queue_index != -1 && graphics_queue_index != -1 && presentation_queue_index != -1) {
                physical_device_index = i;
                break_outer = true;
                break;
            }
        }

        if (break_outer)
            break;

        if (backup_physical_device_index == -1 && graphics_queue_index != -1 && presentation_queue_index != -1) {
            backup_presentation_queue_index = presentation_queue_index;
            backup_graphics_queue_index = graphics_queue_index;
            backup_physical_device_index = i;
        }

        continue;
    }

    if (physical_device_index == -1) {
        if (backup_physical_device_index == -1) {
            std::cerr << "Failed to choose suitable device, aborting...\n";
            HALT_EXECUTION();
        }
        physical_device_index = backup_physical_device_index;
        graphics_queue_index = backup_graphics_queue_index;
        transfer_queue_index = graphics_queue_index;
    }

    // @Todo query and store the important information for the device in a GpuInfo struct
    // Do this later when the info is actually required
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical_devices[physical_device_index], &props); 

    VkDeviceQueueCreateInfo graphics_queue_create_info = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    graphics_queue_create_info.queueFamilyIndex = graphics_queue_index;

    // AMD article about multiple queues. Seems that one graphics queue and one async compute is a fine idea.
    // Using too many queues apparently sucks resources... This is smtg to come back to maybe. This article 
    // is 2016...
    // https://gpuopen.com/learn/concurrent-execution-asynchronous-queues/
    graphics_queue_create_info.queueCount = 1;

    float queue_priorities = 1.0f;
    graphics_queue_create_info.pQueuePriorities = &queue_priorities;

    u32 queue_info_count = 1;
    VkDeviceQueueCreateInfo transfer_queue_create_info;

    if (transfer_queue_index != graphics_queue_index) {
        std::cout << "Selected Device (Primary Choice) " << props.deviceName << '\n';

        queue_info_count++;
        transfer_queue_create_info = graphics_queue_create_info; 
        transfer_queue_create_info.queueFamilyIndex = transfer_queue_index; 
    } else {
        std::cout << "Selected Device (Backup) " << props.deviceName << '\n';
    }

    VkDeviceQueueCreateInfo queue_infos[] = { graphics_queue_create_info, transfer_queue_create_info };

    VkDeviceCreateInfo device_create_info      = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_create_info.pNext                   = &features_full_unfilled;
    device_create_info.queueCreateInfoCount    = queue_info_count;
    device_create_info.pQueueCreateInfos       = queue_infos;
    device_create_info.enabledExtensionCount   = ext_count;
    device_create_info.ppEnabledExtensionNames = ext_names;
    device_create_info.pEnabledFeatures        = NULL;

    VkDevice vk_device;
    auto check = vkCreateDevice(physical_devices[physical_device_index], &device_create_info, ALLOCATION_CALLBACKS, &vk_device);
    DEBUG_OBJ_CREATION(vkCreateDevice, check);

    gpu->vk_physical_device = physical_devices[physical_device_index];
    gpu->vk_queue_indices[0] = graphics_queue_index;
    gpu->vk_queue_indices[1] = presentation_queue_index;
    gpu->vk_queue_indices[2] = transfer_queue_index;

    vkGetDeviceQueue(vk_device, graphics_queue_index, 0, &gpu->vk_queues[0]);

    // if queue indices are equivalent, dont get twice
    if (presentation_queue_index != graphics_queue_index) {
        vkGetDeviceQueue(vk_device, presentation_queue_index, 0, &gpu->vk_queues[1]);
    } else {
        gpu->vk_queues[1] = gpu->vk_queues[0];
    }

    // if queue indices are equivalent, dont get twice
    if (transfer_queue_index != graphics_queue_index) {
        vkGetDeviceQueue(vk_device, transfer_queue_index, 0, &gpu->vk_queues[2]);
    } else {
        gpu->vk_queues[2] = gpu->vk_queues[0];
    }

    return vk_device;
} // func create_vk_device

// *Surface and *Swapchain
static Window *s_Window;
Window* get_window_instance() { return s_Window; }

void init_window(Gpu *gpu, Glfw *glfw) {
    // This is all ugly, but sometimes ugly is best
    s_Window = (Window*)memory_allocate_heap(
            sizeof(Window) + 
            sizeof(VkSwapchainCreateInfoKHR) +
            (sizeof(VkImage) * 2) +
            (sizeof(VkImageView) * 2), 8);

    Window *window = get_window_instance();
    window->swapchain_info = (VkSwapchainCreateInfoKHR*)(window + 1);
    window->vk_images      = (VkImage*)(window->swapchain_info + 1);
    window->vk_image_views = (VkImageView*)(window->vk_images + 2);

    *window->swapchain_info = {};
    window->swapchain_info->surface = create_vk_surface(gpu->vk_instance, glfw);;

    window->vk_swapchain = create_vk_swapchain(gpu, window);
}
void kill_window(Gpu *gpu, Window *window) {
    destroy_vk_swapchain(gpu->vk_device, window);
    destroy_vk_surface(gpu->vk_instance, window->swapchain_info->surface);
    memory_free_heap(window);
}

VkSurfaceKHR create_vk_surface(VkInstance vk_instance, Glfw *glfw) {
    VkSurfaceKHR vk_surface;
    auto check = glfwCreateWindowSurface(vk_instance, glfw->window, NULL, &vk_surface);

    DEBUG_OBJ_CREATION(glfwCreateWindowSurface, check);
    return vk_surface;
} 
void destroy_vk_surface(VkInstance vk_instance, VkSurfaceKHR vk_surface) {
    vkDestroySurfaceKHR(vk_instance, vk_surface, ALLOCATION_CALLBACKS);
}

VkSwapchainKHR create_vk_swapchain(Gpu *gpu, Window *window) {
    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu->vk_physical_device, window->swapchain_info->surface, &surface_capabilities);
    window->swapchain_info->imageExtent = surface_capabilities.currentExtent;
    window->swapchain_info->preTransform = surface_capabilities.currentTransform;

        u32 format_count;
        VkSurfaceFormatKHR *formats;
        u32 present_mode_count;
        VkPresentModeKHR *present_modes;
    if (window->swapchain_info->oldSwapchain == VK_NULL_HANDLE) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->vk_physical_device, window->swapchain_info->surface, &format_count, NULL);
        formats = (VkSurfaceFormatKHR*)memory_allocate_temp(sizeof(VkSurfaceFormatKHR) * format_count, 8);
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->vk_physical_device, window->swapchain_info->surface, &format_count, formats);

        window->swapchain_info->imageFormat = formats[0].format;
        window->swapchain_info->imageColorSpace = formats[0].colorSpace;

        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu->vk_physical_device, window->swapchain_info->surface, &present_mode_count, NULL);
        present_modes = (VkPresentModeKHR*)memory_allocate_temp(sizeof(VkPresentModeKHR) * present_mode_count, 8);
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu->vk_physical_device, window->swapchain_info->surface, &present_mode_count, present_modes);

        for(int i = 0; i < present_mode_count; ++i) {
            if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
                // @Todo immediate presentation
                println("Mailbox Presentation Supported, bu using FIFO...");
        }
        window->swapchain_info->presentMode = VK_PRESENT_MODE_FIFO_KHR;

        window->swapchain_info->sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

        window->swapchain_info->minImageCount = 2;
        window->swapchain_info->imageArrayLayers = 1;
        window->swapchain_info->imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        window->swapchain_info->imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

        window->swapchain_info->compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        window->swapchain_info->clipped = VK_TRUE;

        window->swapchain_info->queueFamilyIndexCount = 1;
        window->swapchain_info->pQueueFamilyIndices = &gpu->vk_queue_indices[1];

    } else { // else if window->swapchain_info->oldSwapchain != VK_NULL_HANDLE
        vkDestroyImageView(gpu->vk_device, window->vk_image_views[0], ALLOCATION_CALLBACKS);
        vkDestroyImageView(gpu->vk_device, window->vk_image_views[1], ALLOCATION_CALLBACKS);
    }

    VkSwapchainKHR vk_swapchain;
    auto check = vkCreateSwapchainKHR(gpu->vk_device, window->swapchain_info, ALLOCATION_CALLBACKS, &vk_swapchain);

    DEBUG_OBJ_CREATION(vkCreateSwapchainKHR, check);
    window->swapchain_info->oldSwapchain = vk_swapchain;

    // Image setup
    u32 image_count = 2;
    check = vkGetSwapchainImagesKHR(gpu->vk_device, vk_swapchain, &image_count, window->vk_images);

    ASSERT(image_count == 2, "Incorrect number of swapchain images returned");
    DEBUG_OBJ_CREATION(vkGetSwapchainImagesKHR, check);

    VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; 
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = window->swapchain_info->imageFormat;
    view_info.components = { 
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
    };
    view_info.subresourceRange = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, // base mip level
        1, // mip level count
        0, // base array layer
        1, // array layer count
    };

    view_info.image = window->vk_images[0];
    check = vkCreateImageView(gpu->vk_device, &view_info, ALLOCATION_CALLBACKS, &window->vk_image_views[0]);
    DEBUG_OBJ_CREATION(vkCreateImageView, check);

    view_info.image = window->vk_images[1];
    check = vkCreateImageView(gpu->vk_device, &view_info, ALLOCATION_CALLBACKS, &window->vk_image_views[1]);
    DEBUG_OBJ_CREATION(vkCreateImageView, check);

    reset_temp(); // end of initializations so clear temp
    return vk_swapchain;
}
void destroy_vk_swapchain(VkDevice device, Window *window) {
    vkDestroyImageView(device, window->vk_image_views[0], ALLOCATION_CALLBACKS);
    vkDestroyImageView(device, window->vk_image_views[1], ALLOCATION_CALLBACKS);
    vkDestroySwapchainKHR(device, window->vk_swapchain, ALLOCATION_CALLBACKS);
}

// *Commands
void create_vk_command_pools(VkDevice vk_device, Create_Vk_Command_Pool_Info *info, u32 count, VkCommandPool *vk_command_pools) {
    VkCommandPoolCreateInfo create_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};;

    create_info.flags |= info->transient ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : 0x0;
    create_info.flags |= info->reset_buffers ? VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : 0x0;
    create_info.queueFamilyIndex = info->queue_family_index;

    VkResult check;
    for(int i = 0; i < count; ++i) {
        check = vkCreateCommandPool(vk_device, &create_info, ALLOCATION_CALLBACKS, &vk_command_pools[i]);
        DEBUG_OBJ_CREATION(vkCreateCommandPool, check);
    }
}
void destroy_vk_command_pools(VkDevice vk_device, u32 count, VkCommandPool *vk_command_pools) {
    for(int i = 0; i < count; ++i) {
        vkDestroyCommandPool(vk_device, vk_command_pools[i], ALLOCATION_CALLBACKS);
    }
}
void reset_vk_command_pools(VkDevice vk_device, u32 count, VkCommandPool *vk_command_pools) {
    for(int i = 0; i < count; ++i) {
        vkResetCommandPool(vk_device, vk_command_pools[i], 0x0);
    }
}
void reset_vk_command_pools_and_release_resources(VkDevice vk_device, u32 count, VkCommandPool *vk_command_pools) {
    for(int i = 0; i < count; ++i) {
        vkResetCommandPool(vk_device, vk_command_pools[i], VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
    }
}

void allocate_vk_command_buffers(VkDevice vk_device, Allocate_Vk_Command_Buffer_Info *info, VkCommandBuffer *vk_command_buffers) {

    VkCommandBufferAllocateInfo allocate_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate_info.commandPool = info->pool;
    allocate_info.commandBufferCount = info->count;
    allocate_info.level = info->secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY :
                                             VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    auto check = vkAllocateCommandBuffers(vk_device, &allocate_info, vk_command_buffers);
    DEBUG_OBJ_CREATION(vkAllocateCommandBuffers, check);
}

void begin_vk_command_buffer_primary(VkCommandBuffer vk_command_buffer) {
    VkCommandBufferBeginInfo info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

    auto check = vkBeginCommandBuffer(vk_command_buffer, &info);
    DEBUG_OBJ_CREATION(vkBeginCommandBuffer, check);
}
void begin_vk_command_buffer_primary_onetime(VkCommandBuffer vk_command_buffer) {
    VkCommandBufferBeginInfo info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    auto check = vkBeginCommandBuffer(vk_command_buffer, &info);
    DEBUG_OBJ_CREATION(vkBeginCommandBuffer, check);
}
void end_vk_command_buffer(VkCommandBuffer vk_command_buffer) {
    auto check = vkEndCommandBuffer(vk_command_buffer);
    DEBUG_OBJ_CREATION(vkEndCommandBuffer, check);
}

void submit_vk_command_buffer(VkQueue vk_queue, VkFence vk_fence, u32 count, Submit_Vk_Command_Buffer_Info *infos) {
    u64 mark = get_mark_temp();
    VkSubmitInfo2 *submit_infos = (VkSubmitInfo2*)memory_allocate_temp(sizeof(VkSubmitInfo2) * count, 8);

    u32 command_buffer_total_count = 0;
    for(int i = 0; i < count; ++i) {
        command_buffer_total_count += infos[i].command_buffer_count;
    }
    VkCommandBufferSubmitInfo *command_buffer_infos = (VkCommandBufferSubmitInfo*)memory_allocate_temp(sizeof(VkCommandBufferSubmitInfo) * command_buffer_total_count, 8);

    u32 command_buffer_index = 0;
    for(int i = 0; i < count; ++i) {
        submit_infos[i] = {VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
        submit_infos[i].waitSemaphoreInfoCount   = infos[i].wait_semaphore_info_count;
        submit_infos[i].pWaitSemaphoreInfos      = infos[i].wait_semaphore_infos;
        submit_infos[i].signalSemaphoreInfoCount = infos[i].signal_semaphore_info_count;
        submit_infos[i].pSignalSemaphoreInfos    = infos[i].signal_semaphore_infos;
        submit_infos[i].commandBufferInfoCount   = infos[i].command_buffer_count;

        // @Bug I am basically certain that I will have make a logic error in here somewhere. Check this if submission results ever seem awry, although it does seem to work after a brief basic test so hopes are high
        for(int j = 0; j < infos[i].command_buffer_count; ++j) {
            command_buffer_infos[command_buffer_index] = {
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                NULL,
                infos[i].command_buffers[j],
                0,
            };
            command_buffer_index++;
        }
        submit_infos[i].pCommandBufferInfos = &command_buffer_infos[command_buffer_index - infos[i].command_buffer_count];
    }
    auto check = vkQueueSubmit2(vk_queue, count, submit_infos, vk_fence);
    DEBUG_OBJ_CREATION(vkQueueSubmit2, check);
    cut_diff_temp(mark);
}

// *Sync
void create_vk_fences_unsignalled(VkDevice vk_device, u32 count, VkFence *vk_fences) {
    VkFenceCreateInfo info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkResult check;
    for(int i = 0; i < count; ++i) {
        check = vkCreateFence(vk_device, &info, ALLOCATION_CALLBACKS, &vk_fences[i]); 
        DEBUG_OBJ_CREATION(vkCreateFence, check);
    }
}
void create_vk_fences_signalled(VkDevice vk_device, u32 count, VkFence *vk_fences) {
    VkFenceCreateInfo info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VkResult check;
    for(int i = 0; i < count; ++i) {
        check = vkCreateFence(vk_device, &info, ALLOCATION_CALLBACKS, &vk_fences[i]); 
        DEBUG_OBJ_CREATION(vkCreateFence, check);
    }
}
void destroy_vk_fences(VkDevice vk_device, u32 count, VkFence *vk_fences) {
    for(int i = 0; i < count; ++i) {
        vkDestroyFence(vk_device, vk_fences[i], ALLOCATION_CALLBACKS);
    }
}

void create_vk_semaphores_binary(VkDevice vk_device, u64 initial_value, u32 count, VkSemaphore *vk_semaphores) {
    VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    VkResult check;
    for(int i = 0; i < count; ++i) {
        check = vkCreateSemaphore(vk_device, &info, ALLOCATION_CALLBACKS, &vk_semaphores[i]);
        DEBUG_OBJ_CREATION(vkCreateSemaphore, check);
    }
}
void create_vk_semaphores_timeline(VkDevice vk_device, u64 initial_value, u32 count, VkSemaphore *vk_semaphores) {
    VkSemaphoreTypeCreateInfo type_info = {
        VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        NULL,
        VK_SEMAPHORE_TYPE_TIMELINE,
        initial_value,
    };

    VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    info.pNext = &type_info;

    VkResult check;
    for(int i = 0; i < count; ++i) {
        check = vkCreateSemaphore(vk_device, &info, ALLOCATION_CALLBACKS, &vk_semaphores[i]);
        DEBUG_OBJ_CREATION(vkCreateSemaphore, check);
    }
}
void destroy_vk_semaphores(VkDevice vk_device, u32 count, VkSemaphore *vk_semaphore) {
    for(int i = 0; i < count; ++i) {
        vkDestroySemaphore(vk_device, vk_semaphore[i], ALLOCATION_CALLBACKS);
    }
}

#if DEBUG
VkDebugUtilsMessengerEXT create_debug_messenger(Create_Vk_Debug_Messenger_Info *info) {
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = fill_vk_debug_messenger_info(info);

    VkDebugUtilsMessengerEXT debug_messenger;
    auto check = vkCreateDebugUtilsMessengerEXT(info->vk_instance, &debug_messenger_create_info, NULL, &debug_messenger);

    DEBUG_OBJ_CREATION(vkCreateDebugUtilsMessengerEXT, check)
    return debug_messenger;
}

VkResult vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) 
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}
void vkDestroyDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT messenger,
        const VkAllocationCallbacks *pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
        return func(instance, messenger, pAllocator);
} 
#endif
