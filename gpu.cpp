#include "gpu.hpp"
#include "vulkan_errors.hpp"
#include "glfw.hpp"
#include "spirv.hpp"

#if DEBUG
static VkDebugUtilsMessengerEXT s_vk_debug_messenger;
VkDebugUtilsMessengerEXT* get_vk_debug_messenger_instance() { return &s_vk_debug_messenger; }
#endif

#define VULKAN_ALLOCATOR_IS_NULL true

#if VULKAN_ALLOCATOR_IS_NULL
#define ALLOCATION_CALLBACKS NULL
#endif

//
// @PipelineAllocation
// Best idea really is to calculate how big all the unchanging state settings are upfront, then make one 
// allocation at load time large enough to hold everything, and just allocate everything into that.
// Just need to find a good way to count all this size...
//

static Gpu *s_Gpu;
Gpu* get_gpu_instance() { return s_Gpu; }

static VkFormat COLOR_ATTACHMENT_FORMAT;

void init_gpu() {
    // keep struct data together (not pointing to random heap addresses)
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

    // creates queues and fills gpu struct with them
    // features and extensions lists defined in the function body
    gpu->vk_device = create_vk_device(gpu);
    gpu->vma_allocator = create_vma_allocator(gpu);
}
void kill_gpu(Gpu *gpu) {
    vmaDestroyAllocator(gpu->vma_allocator);
    vkDestroyDevice(gpu->vk_device, ALLOCATION_CALLBACKS);
#if DEBUG
    vkDestroyDebugUtilsMessengerEXT(gpu->vk_instance, *get_vk_debug_messenger_instance(), ALLOCATION_CALLBACKS);
#endif
    vkDestroyInstance(gpu->vk_instance, ALLOCATION_CALLBACKS);
    memory_free_heap(gpu);
}

// `Instance
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

    // Preprocessor always ugly
#if DEBUG
    u32 layer_count = 1;
    const char *layer_names[] = {
        "VK_LAYER_KHRONOS_validation"
    };
    u32 ext_count = 2;
    const char *ext_names[] = {
        "VK_EXT_validation_features",
        "VK_EXT_debug_utils",
    };
#else 
    u32 layer_count = 0;
    const char *layer_names[] = {};
    u32 ext_count = 0;
    const char *ext_names[] = {};
#endif

    u32 glfw_ext_count;
    const char **glfw_ext_names = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

    u8 char_index = 0;
    u8 name_index = 0;
    char ext_name_buffer[250]; // Assume char total in ext names < 250
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

// `Device ///////////
VkDevice create_vk_device(Gpu *gpu) { // returns logical device, silently fills in gpu.physical_device

    uint32_t ext_count = 2;
    const char *ext_names[] = {
        "VK_KHR_swapchain",
        "VK_EXT_descriptor_buffer",
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
        .descriptorBufferImageLayoutIgnored = VK_TRUE,
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

    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptor_buffer_features_unfilled =  descriptor_buffer_features;
    descriptor_buffer_features_unfilled.pNext = &vk13_features_unfilled;

    VkPhysicalDeviceFeatures2 features_full_unfilled = features_full;
    features_full_unfilled.pNext = &descriptor_buffer_features_unfilled;
 
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

        bool incompatible = false;
        if (descriptor_buffer_features.descriptorBuffer == VK_FALSE) {
            std::cerr << "Device Index " << i << " does not support descriptorBuffer\n";
            incompatible = true;
        }
        if (vk13_features.dynamicRendering == VK_FALSE) {
            std::cerr << "Device Index " << i << " does not support dynamicRendering\n";
            incompatible = true;
        }

        if (incompatible)
            continue;

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

    VkPhysicalDeviceProperties physical_device_properties;
    vkGetPhysicalDeviceProperties(gpu->vk_physical_device, &physical_device_properties);
    gpu->info->limits = physical_device_properties.limits;

    return vk_device;
} // func create_vk_device

// `Allocator
VmaAllocator create_vma_allocator(Gpu *gpu) {
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
     
    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorCreateInfo.physicalDevice = gpu->vk_physical_device;
    allocatorCreateInfo.device = gpu->vk_device;
    allocatorCreateInfo.instance = gpu->vk_instance;
    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
     
    VmaAllocator allocator;
    auto check = vmaCreateAllocator(&allocatorCreateInfo, &allocator);
    DEBUG_OBJ_CREATION(vmaCreateAllocator, check);

    return allocator;
}

// `Surface and `Swapchain
static Window *s_Window;
Window* get_window_instance() { return s_Window; }

void init_window(Gpu *gpu, Glfw *glfw) {
    VkSurfaceKHR surface = create_vk_surface(gpu->vk_instance, glfw);

    VkSwapchainKHR swapchain = create_vk_swapchain(gpu, surface);
    Window *window = get_window_instance();
    window->vk_swapchain = swapchain;
}
void kill_window(Gpu *gpu, Window *window) {
    destroy_vk_swapchain(gpu->vk_device, window);
    destroy_vk_surface(gpu->vk_instance, window->swapchain_info.surface);
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

VkSwapchainKHR recreate_vk_swapchain(Gpu *gpu, Window *window) {
    vkDestroyImageView(gpu->vk_device, window->vk_image_views[0], ALLOCATION_CALLBACKS);
    vkDestroyImageView(gpu->vk_device, window->vk_image_views[1], ALLOCATION_CALLBACKS);

    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu->vk_physical_device, window->swapchain_info.surface, &surface_capabilities);

    window->swapchain_info.imageExtent = surface_capabilities.currentExtent;
    window->swapchain_info.preTransform = surface_capabilities.currentTransform;

    // 
    // This might error with some stuff in the createinfo not properly define, 
    // I made the refactor while sleepy!
    //
    auto check = vkCreateSwapchainKHR(gpu->vk_device, &window->swapchain_info, ALLOCATION_CALLBACKS, &window->vk_swapchain);

    DEBUG_OBJ_CREATION(vkCreateSwapchainKHR, check);
    window->swapchain_info.oldSwapchain = window->vk_swapchain;

    // Image setup
    vkGetSwapchainImagesKHR(gpu->vk_device, window->vk_swapchain, &window->image_count, window->vk_images);

    VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; 
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format   = window->swapchain_info.imageFormat;

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
    check = vkCreateImageView(gpu->vk_device, &view_info, ALLOCATION_CALLBACKS, window->vk_image_views);
    DEBUG_OBJ_CREATION(vkCreateImageView, check);

    view_info.image = window->vk_images[1];
    check = vkCreateImageView(gpu->vk_device, &view_info, ALLOCATION_CALLBACKS, window->vk_image_views + 1);
    DEBUG_OBJ_CREATION(vkCreateImageView, check);

    return window->vk_swapchain;
}

VkSwapchainKHR create_vk_swapchain(Gpu *gpu, VkSurfaceKHR vk_surface) {
    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu->vk_physical_device, vk_surface, &surface_capabilities);

    VkSwapchainCreateInfoKHR swapchain_info = {};
    swapchain_info.surface = vk_surface;
    swapchain_info.imageExtent = surface_capabilities.currentExtent;
    swapchain_info.preTransform = surface_capabilities.currentTransform;

    u32 format_count;
    VkSurfaceFormatKHR *formats;
    u32 present_mode_count;
    VkPresentModeKHR *present_modes;

    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->vk_physical_device, swapchain_info.surface, &format_count, NULL);
    formats = (VkSurfaceFormatKHR*)memory_allocate_temp(sizeof(VkSurfaceFormatKHR) * format_count, 8);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu->vk_physical_device, swapchain_info.surface, &format_count, formats);

    swapchain_info.imageFormat = formats[0].format;
    COLOR_ATTACHMENT_FORMAT = swapchain_info.imageFormat;
    swapchain_info.imageColorSpace = formats[0].colorSpace;

    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu->vk_physical_device, swapchain_info.surface, &present_mode_count, NULL);
    present_modes = (VkPresentModeKHR*)memory_allocate_temp(sizeof(VkPresentModeKHR) * present_mode_count, 8);
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu->vk_physical_device, swapchain_info.surface, &present_mode_count, present_modes);

    for(int i = 0; i < present_mode_count; ++i) {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            // @Todo immediate presentation
            println("Mailbox Presentation Supported, but using FIFO (@Todo)...");
    }
    swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;

    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

    swapchain_info.minImageCount = surface_capabilities.minImageCount < 2 ? 2 : surface_capabilities.minImageCount;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.clipped = VK_TRUE;

    swapchain_info.queueFamilyIndexCount = 1;
    swapchain_info.pQueueFamilyIndices = &gpu->vk_queue_indices[1];

    VkSwapchainKHR swapchain;
    auto check = vkCreateSwapchainKHR(gpu->vk_device, &swapchain_info, ALLOCATION_CALLBACKS, &swapchain);
    DEBUG_OBJ_CREATION(vkCreateSwapchainKHR, check);

    // Image setup
    u32 image_count = surface_capabilities.minImageCount < 2 ? 2 : surface_capabilities.minImageCount;

    // keep struct data together (not pointing to random heap addresses)
    s_Window = (Window*)memory_allocate_heap(
            sizeof(Window)                       +
            (sizeof(VkImage)     * image_count)  +
            (sizeof(VkImageView) * image_count), 8);

    // Is this better than just continuing to use s_Window? who cares...
    Window *window = get_window_instance();
    window->vk_swapchain = swapchain;
    window->swapchain_info = swapchain_info;
    window->swapchain_info.oldSwapchain = swapchain;

    window->image_count = image_count;
    window->vk_images = (VkImage*)(window + 1);
    window->vk_image_views = (VkImageView*)(window->vk_images + image_count);

    vkGetSwapchainImagesKHR(gpu->vk_device, window->vk_swapchain, &image_count, window->vk_images);

    // Create Views
    VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; 
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format   = swapchain_info.imageFormat;

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
    check = vkCreateImageView(gpu->vk_device, &view_info, ALLOCATION_CALLBACKS, window->vk_image_views);
    DEBUG_OBJ_CREATION(vkCreateImageView, check);

    view_info.image = window->vk_images[1];
    check = vkCreateImageView(gpu->vk_device, &view_info, ALLOCATION_CALLBACKS, window->vk_image_views + 1);
    DEBUG_OBJ_CREATION(vkCreateImageView, check);

    reset_temp(); // end of basic initializations so clear temp
    return swapchain;
}
void destroy_vk_swapchain(VkDevice device, Window *window) {
    vkDestroyImageView(device, window->vk_image_views[0], ALLOCATION_CALLBACKS);
    vkDestroyImageView(device, window->vk_image_views[1], ALLOCATION_CALLBACKS);
    vkDestroySwapchainKHR(device, window->vk_swapchain, ALLOCATION_CALLBACKS);
}

// `Commands
Command_Group_Vk create_command_group_vk(VkDevice vk_device, u32 queue_family_index) {
    VkCommandPoolCreateInfo create_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    create_info.flags = 0x0;
    create_info.queueFamilyIndex = queue_family_index;

    // @AllocationType pools will likely be persistent
    VkCommandPool vk_command_pool;
    auto check = vkCreateCommandPool(vk_device, &create_info, ALLOCATION_CALLBACKS, &vk_command_pool);
    DEBUG_OBJ_CREATION(vkCreateCommandPool, check);

    Command_Group_Vk command_group;
    command_group.pool = vk_command_pool;
    INIT_DYN_ARRAY(command_group.buffers, 8);
    return command_group;
}
Command_Group_Vk create_command_group_vk_transient(VkDevice vk_device, u32 queue_family_index) {
    VkCommandPoolCreateInfo create_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    create_info.queueFamilyIndex = queue_family_index;

    // @AllocationType pools will likely be persistent
    VkCommandPool vk_command_pool;
    auto check = vkCreateCommandPool(vk_device, &create_info, ALLOCATION_CALLBACKS, &vk_command_pool);
    DEBUG_OBJ_CREATION(vkCreateCommandPool, check);

    Command_Group_Vk command_group;
    command_group.pool = vk_command_pool;
    INIT_DYN_ARRAY(command_group.buffers, 8);
    return command_group;
}
void destroy_command_groups_vk(VkDevice vk_device, u32 count, Command_Group_Vk *command_groups_vk) {
    // Assumes pools are already reset
    for(int i = 0; i < count; ++i) {
        vkDestroyCommandPool(vk_device, command_groups_vk[i].pool, ALLOCATION_CALLBACKS);
        KILL_DYN_ARRAY(command_groups_vk[i].buffers);
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

// @Todo use a free list and an in-use list and an in-use/free mask to avoid allocating new command buffers.
// Use a bucket array for this? Or just swap stuff around to keep free stuff contiguous (if its in use it swapped 
// with the first free implemented in a ring buffer?)
// ^^ also applies to sync objects...
VkCommandBuffer* allocate_vk_secondary_command_buffers(VkDevice vk_device, Command_Group_Vk *command_group, u32 count) {
    VkCommandBufferAllocateInfo allocate_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate_info.commandPool = command_group->pool;
    allocate_info.commandBufferCount = count;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;

    if (command_group->buffers.len + count > command_group->buffers.cap) {
        // This might be growing by too much. I do not yet know how many command buffers I will be needing,
        // But I assume this array will never be that large...
        GROW_DYN_ARRAY(command_group->buffers,/*Just in case cap is small*/count + (2 * command_group->buffers.cap)); 
    }

    // @Todo I need to handle pools returning out of memory. I dont know how often this happens. But it should
    // be easy: just make the pool field in Command_Group arrayed.
    auto check =
        vkAllocateCommandBuffers(vk_device, &allocate_info, command_group->buffers.data + command_group->buffers.len);
    DEBUG_OBJ_CREATION(vkAllocateCommandBuffers, check);

    command_group->buffers.len += count;
    return command_group->buffers.data + command_group->buffers.len - count;
}
VkCommandBuffer* allocate_vk_primary_command_buffers(VkDevice vk_device, Command_Group_Vk *command_group, u32 count) {
    VkCommandBufferAllocateInfo allocate_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate_info.commandPool = command_group->pool;
    allocate_info.commandBufferCount = count;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    if (command_group->buffers.len + count > command_group->buffers.cap) {
        // This might be growing by too much. I do not yet know how many command buffers I will be needing,
        // But I assume this array will never be that large...
        GROW_DYN_ARRAY(command_group->buffers,/*Just in case cap is small*/count + (2 * command_group->buffers.cap)); 
    }

    // @Todo I need to handle pools returning out of memory. I dont know how often this happens. But it should
    // be easy: just make the pool field in Command_Group arrayed.
    auto check =
        vkAllocateCommandBuffers(vk_device, &allocate_info, command_group->buffers.data + command_group->buffers.len);
    DEBUG_OBJ_CREATION(vkAllocateCommandBuffers, check);

    command_group->buffers.len += count;
    return command_group->buffers.data + command_group->buffers.len - count;
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

// `Sync
// @Todo for these sync pools, maybe implement a free mask and simd checking, so if there is a large enough 
// block of free objects in the middle of the pool, they can be used instead of appending. I have no idea if this 
// will be useful, as my intendedm use case for this system is to have a persistent pool and a temp pool. This 
// would eliminate the requirement for space saving. 
Fence_Pool create_fence_pool(VkDevice vk_device, u32 size) {
    Fence_Pool pool;
    pool.len = size;
    pool.in_use = 0;
    pool.fences = (VkFence*)memory_allocate_heap(sizeof(VkFence) * size, 8);

    VkFenceCreateInfo info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkResult check;
    for(int i = 0; i < size; ++i) {
        check = vkCreateFence(vk_device, &info, ALLOCATION_CALLBACKS, &pool.fences[i]);
        DEBUG_OBJ_CREATION(vkCreateFence, check);
    }

    return pool;
}
void destroy_fence_pool(VkDevice vk_device, Fence_Pool *pool) {
    ASSERT(pool->in_use, "Fences cannot be in use when pool is destroyed");
    for(int i = 0; i < pool->len; ++i) {
        vkDestroyFence(vk_device, pool->fences[i], ALLOCATION_CALLBACKS);
    }
    memory_free_heap((void*)pool->fences);
}
VkFence* get_fences(Fence_Pool *pool, u32 count) {
    if (pool->in_use + count == pool->len) {
        VkFence *old_fences = pool->fences;
        u32 old_len = pool->len;

        pool->len *= 2;
        pool->fences = (VkFence*)memory_allocate_heap(sizeof(VkFence) * pool->len, 8);

        memcpy(pool->fences, old_fences, old_len * sizeof(VkFence));
        memory_free_heap((void*)old_fences);
    }
    pool->in_use += count;
    return pool->fences + (pool->in_use - count);
}
void reset_fence_pool(Fence_Pool *pool) {
    pool->in_use = 0;
}
void cut_tail_fences(Fence_Pool *pool, u32 size) {
    pool->in_use -= size;
}

// Semaphores
Binary_Semaphore_Pool create_binary_semaphore_pool(VkDevice vk_device, u32 size) {
    Binary_Semaphore_Pool pool;
    pool.len = size;
    pool.in_use = 0;
    pool.semaphores = (VkSemaphore*)memory_allocate_heap(sizeof(VkSemaphore) * size, 8);

    VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkResult check;
    for(int i = 0; i < size; ++i) {
        check = vkCreateSemaphore(vk_device, &info, ALLOCATION_CALLBACKS, &pool.semaphores[i]);
        DEBUG_OBJ_CREATION(vkCreateSemaphore, check);
    }

    return pool;
}
void destroy_binary_semaphore_pool(VkDevice vk_device, Binary_Semaphore_Pool *pool) {
    ASSERT(pool->in_use, "Semaphores cannot be in use when pool is destroyed");
    for(int i = 0; i < pool->len; ++i) {
        vkDestroySemaphore(vk_device, pool->semaphores[i], ALLOCATION_CALLBACKS);
    }
    memory_free_heap((void*)pool->semaphores);
}
VkSemaphore* get_binary_semaphores(Binary_Semaphore_Pool *pool, u32 count) {
    if (pool->in_use + count == pool->len) {
        VkSemaphore *old_semaphores = pool->semaphores;
        u32 old_len = pool->len;

        pool->len *= 2;
        pool->semaphores = (VkSemaphore*)memory_allocate_heap(sizeof(VkSemaphore) * pool->len, 8);

        memcpy(pool->semaphores, old_semaphores, old_len * sizeof(VkSemaphore));
        memory_free_heap((void*)old_semaphores);
    }
    pool->in_use += count;
    return pool->semaphores + (pool->in_use - count);
}
// poopoo <- do not delete this comment - Sol & Jenny, Sept 16 2023
void reset_binary_semaphore_pool(Binary_Semaphore_Pool *pool) {
    pool->in_use = 0;
}
void cut_tail_binary_semaphores(Binary_Semaphore_Pool *pool, u32 size) {
    pool->in_use -= size;
}

// `Descriptors -- static / pool allocated
// @Note I would like to have a pool for each type of descriptor that I will use to help with
// fragmentation, and understanding what allocations are where... idk if this possible. But I
// should think so, since there are so few descriptor types. The over head of having more
// pools seems unimportant as nowhere does anyone say "Do not allocate too many pools", its
// more about managing the pools that you have effectively...
static constexpr u32 DESCRIPTOR_POOL_SAMPLER_MAX_SET_COUNT    = 64;
static constexpr u32 DESCRIPTOR_POOL_BUFFER_MAX_SET_COUNT     = 64;

static constexpr u32 DESCRIPTOR_POOL_SAMPLER_ALLOCATION_COUNT = 64;
static constexpr u32 DESCRIPTOR_POOL_BUFFER_ALLOCATION_COUNT  = 64;

VkDescriptorPool create_vk_descriptor_pool(VkDevice vk_device, Descriptor_Pool_Type type) {
    VkDescriptorPoolSize pool_size = {VK_DESCRIPTOR_TYPE_SAMPLER};
    VkDescriptorPoolCreateInfo create_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};

    switch(type) {
    case DESCRIPTOR_POOL_TYPE_SAMPLER:
        create_info.maxSets       = DESCRIPTOR_POOL_SAMPLER_MAX_SET_COUNT;
        pool_size.descriptorCount = DESCRIPTOR_POOL_SAMPLER_ALLOCATION_COUNT;
        break;
    case DESCRIPTOR_POOL_TYPE_UNIFORM_BUFFER:
        create_info.maxSets       = DESCRIPTOR_POOL_BUFFER_MAX_SET_COUNT;
        pool_size.descriptorCount = DESCRIPTOR_POOL_BUFFER_ALLOCATION_COUNT;
        break;
    default:
        ASSERT(false, "Not a valid DescriptorPool type");
        break;
    }
    create_info.poolSizeCount = 1;
    create_info.pPoolSizes = &pool_size;

    VkDescriptorPool pool;
    auto check = vkCreateDescriptorPool(vk_device, &create_info, ALLOCATION_CALLBACKS, &pool);
    DEBUG_OBJ_CREATION(vkCreateDescriptorPool, check);
    return pool;
}
VkResult reset_vk_descriptor_pool(VkDevice vk_device, VkDescriptorPool pool) {
    return vkResetDescriptorPool(vk_device, pool, 0x0);
}
void destroy_vk_descriptor_pool(VkDevice vk_device, VkDescriptorPool pool) {
    vkDestroyDescriptorPool(vk_device, pool, ALLOCATION_CALLBACKS);
} 

Gpu_Descriptor_Allocator gpu_create_descriptor_allocator(VkDevice vk_device, int sampler_size, int buffer_size) {
    Gpu_Descriptor_Allocator allocator = {};
    sampler_size = align(sampler_size, 8);
    buffer_size  = align(buffer_size, 8);

    ASSERT(sampler_size <= DESCRIPTOR_POOL_SAMPLER_MAX_SET_COUNT, "Sampler Pool Create Size Too Large");
    ASSERT(sampler_size <= DESCRIPTOR_POOL_BUFFER_MAX_SET_COUNT,  "Sampler Pool Create Size Too Large");

    allocator.sampler_pool    = create_vk_descriptor_pool(vk_device, GPU_DESCRIPTOR_POOL_TYPE_SAMPLER);
    allocator.buffer_pool     = create_vk_descriptor_pool(vk_device, GPU_DESCRIPTOR_POOL_TYPE_BUFFER);

    u8 *memory_block = memory_allocate_heap(
       (sizeof(VkDescriptorSetLayout) * sampler_size) +
       (sizeof(VkDescriptorSetLayout) * buffer_size ) +
       (     sizeof(VkDescriptorSet)  * sampler_size) +
       (     sizeof(VkDescriptorSet)  * buffer_size ), 8);

    allocator.sampler_layouts = (VkDescriptorSetLayout*)memory_block;
    allocator.buffer_layouts  = allocator.sampler_layouts + sampler_size;
    allocator.sampler_sets    = (VkDescriptorSet*)(allocator.buffer_layouts + buffer_size);
    allocator.buffer_sets     = allocator.sampler_sets + sampler_size;

    return allocator;
}

// @Todo I dont know what is the best allocation/pool pattern for these descriptors yet...
Gpu_Descriptor_Pool_Type gpu_get_pool_type(VkDescriptorType type) {
    switch(type) {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return GPU_DESCRIPTOR_POOL_TYPE_SAMPLER;

    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return GPU_DESCRIPTOR_POOL_TYPE_BUFFER;

    default:
        ASSERT(false, "Invalid VkDescriptorType");
        break;
    }
    return GPU_DESCRIPTOR_POOL_TYPE_BUFFER;
}
//
// @Goal For descriptor allocation, I would like all descriptors (for a thread) to allocated in one go:
//     When smtg wants to get a descriptor set, it gets put in a buffer, and then this buffer is at
//     whatever fill level and all the sets are allocated and returned... - sol 4 oct 2023
//
// -- (Really, I dont know why you cant just create all the descriptors necessary for shaders in one
// go store them for the lifetime of the program, tbf big programs can have A LOT of shaders, but many
// sets should be usable across many shaders and pipelines etc. I feel like with proper planning,
// this would be possible... idk maybe I am miles off) -- - sol 4 oct 2023
//
void allocate_vk_descriptor_sets(VkDevice vk_device, int pool_count, VkDescriptorPool *pools, int set_count, const VkDescriptorSetLayout *layouts, VkDescriptorSet *sets) {
     VkDescriptorSetAllocateInfo allocate_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
     allocate_info.descriptorPool     = pool;
     allocate_info.descriptorSetCount = set_count;
     allocate_info.pSetLayouts        = layouts;
     // @Todo handle allocation failure
     auto check = vkAllocateDescriptorSets(vk_device, &allocate_info, sets);
     DEBUG_OBJ_CREATION(vkAllocateDescriptorSets, check);
}

VkDescriptorSetLayout* create_vk_descriptor_set_layouts(VkDevice vk_device, int count, Create_Vk_Descriptor_Set_Layout_Info *infos) {
    VkDescriptorSetLayout *layouts = 
        (VkDescriptorSetLayout*)memory_allocate_temp(sizeof(VkDescriptorSetLayout) * count, 8);
    VkDescriptorSetLayoutCreateInfo create_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    VkResult check;
    for(int i = 0; i < count; ++i) {
        create_info.bindingCount = infos[i].count;
        create_info.pBindings    = infos[i].bindings;
        check = vkCreateDescriptorSetLayout(vk_device, &create_info, ALLOCATION_CALLBACKS, &layouts[i]);
    }
    return layouts;
}

// `Descriptors -- buffers / dynamic
//VkDescriptorSetLayout* create_vk_descriptor_set_layout();

void destroy_vk_descriptor_set_layouts(VkDevice vk_device, u32 count, VkDescriptorSetLayout *layouts) {
    for(int i = 0; i < count; ++i)
        vkDestroyDescriptorSetLayout(vk_device, layouts[i], ALLOCATION_CALLBACKS);
    memory_free_heap((void*)layouts);
}

// `PipelineSetup
// `ShaderStages
VkPipelineShaderStageCreateInfo* create_vk_pipeline_shader_stages(VkDevice vk_device, u32 count, Create_Vk_Pipeline_Shader_Stage_Info *infos) {
    // @Todo like with other aspects of pipeline creation, I think that shader stage infos can all be allocated 
    // and loaded at startup and the  not freed for the duration of the program as these are not changing state
    u8 *memory_block = memory_allocate_heap(sizeof(VkPipelineShaderStageCreateInfo) * count, 8);

    VkResult check;
    VkShaderModuleCreateInfo          module_info;
    VkPipelineShaderStageCreateInfo  *stage_info;
    for(int i = 0; i < count; ++i) {

        module_info = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, 
            NULL,
            0x0,
            infos[i].code_size,
            infos[i].shader_code,
        };

        VkShaderModule mod;
        check = vkCreateShaderModule(vk_device, &module_info, ALLOCATION_CALLBACKS, &mod);
        DEBUG_OBJ_CREATION(vkCreateShaderModule, check);

        stage_info = (VkPipelineShaderStageCreateInfo*)(memory_block + (sizeof(VkPipelineShaderStageCreateInfo) * i));

       *stage_info = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};

        stage_info->pNext  = NULL;
        stage_info->stage  = infos[i].stage;
        stage_info->module = mod;
        stage_info->pName  = "main";

        // @Todo add specialization support
        stage_info->pSpecializationInfo = NULL;
    }
    return (VkPipelineShaderStageCreateInfo*)memory_block;
}

void destroy_vk_pipeline_shader_stages(VkDevice vk_device, u32 count, VkPipelineShaderStageCreateInfo *stages) {
    for(int i = 0; i < count; ++i) {
        vkDestroyShaderModule(vk_device, stages[i].module, ALLOCATION_CALLBACKS);
    }
    memory_free_heap((void*)stages);
}

// `VertexInputState
VkVertexInputBindingDescription create_vk_vertex_binding_description(Create_Vk_Vertex_Input_Binding_Description_Info *info) {
    VkVertexInputBindingDescription binding_description = { info->binding, info->stride }; 
    return binding_description;
}

VkVertexInputAttributeDescription create_vk_vertex_attribute_description(Create_Vk_Vertex_Input_Attribute_Description_Info *info) {
    VkVertexInputAttributeDescription attribute_description = {
        info->location,
        info->binding,
        info->format,
        info->offset,
    };
    return attribute_description;
}
VkPipelineVertexInputStateCreateInfo create_vk_pipeline_vertex_input_states(Create_Vk_Pipeline_Vertex_Input_State_Info *info) {
    // @Todo @PipelineAllocation same as above ^^
    VkPipelineVertexInputStateCreateInfo input_state_info = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    input_state_info.vertexBindingDescriptionCount   = info->binding_count;
    input_state_info.pVertexBindingDescriptions      = info->binding_descriptions;
    input_state_info.vertexAttributeDescriptionCount = info->attribute_count;
    input_state_info.pVertexAttributeDescriptions    = info->attribute_descriptions;

    return input_state_info;
}

// `InputAssemblyState
VkPipelineInputAssemblyStateCreateInfo create_vk_pipeline_input_assembly_state(VkPrimitiveTopology topology, VkBool32 primitive_restart) {
    // @Todo @PipelineAllocation same as above ^^
    // ^^ not sure about these todos now that I have gltf use cases ... - sol, sept 30 2023

    VkPipelineInputAssemblyStateCreateInfo assembly_state_info = {};

    assembly_state_info = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly_state_info.topology = topology;
    assembly_state_info.primitiveRestartEnable = primitive_restart;

    return assembly_state_info;
}

// `TessellationState
// @Todo support Tessellation

// `Viewport
VkPipelineViewportStateCreateInfo create_vk_pipeline_viewport_state(Window *window) {
    VkViewport viewport = {
        0.0f, 0.0f, // x, y
        (float)window->swapchain_info.imageExtent.width,
        (float)window->swapchain_info.imageExtent.height,
        0.0f, 1.0f, // mindepth, maxdepth
    };
    VkRect2D scissor = {
        {0, 0}, // offsets
        {
            window->swapchain_info.imageExtent.width,
            window->swapchain_info.imageExtent.height,
        },
    };
    VkPipelineViewportStateCreateInfo viewport_info = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport_info.viewportCount = 0; // must be zero if dyn state set viewport with count is set
    /*viewport_info.pViewports = &viewport;
    viewport_info.scissorCount = 1;
    viewport_info.pScissors = &scissor;*/

    return viewport_info;
}

// `RasterizationState
VkPipelineRasterizationStateCreateInfo create_vk_pipeline_rasterization_state(VkPolygonMode polygon_mode, VkCullModeFlags cull_mode, VkFrontFace front_face) {
    VkPipelineRasterizationStateCreateInfo state = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    state.polygonMode = polygon_mode;
    state.cullMode    = cull_mode;
    state.frontFace   = front_face;
    return state;
}
void vkCmdSetDepthClampEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClampEnable) {
    VkDevice device = get_gpu_instance()->vk_device;
    auto func = (PFN_vkCmdSetDepthClampEnableEXT) vkGetDeviceProcAddr(device, "vkCmdSetDepthClampEnableEXT");

    ASSERT(func != nullptr, "Depth Clamp Enable Cmd not found");
    return func(commandBuffer, depthClampEnable);
}
void vkCmdSetPolygonModeEXT(VkCommandBuffer commandBuffer, VkPolygonMode polygonMode) {
    VkDevice device = get_gpu_instance()->vk_device;
    auto func = (PFN_vkCmdSetPolygonModeEXT) vkGetDeviceProcAddr(device, "vkCmdSetPolygonModeEXT");

    ASSERT(func != nullptr, "Polygon Mode Cmd not found");
    return func(commandBuffer, polygonMode);
}

// `MultisampleState // @Todo actually support setting multisampling functions
VkPipelineMultisampleStateCreateInfo create_vk_pipeline_multisample_state() {
    VkPipelineMultisampleStateCreateInfo state = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; 
    state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; 
    state.sampleShadingEnable = VK_FALSE;
    return state;
}

// `DepthStencilState
VkPipelineDepthStencilStateCreateInfo create_vk_pipeline_depth_stencil_state(Create_Vk_Pipeline_Depth_Stencil_State_Info *info) {
    VkPipelineDepthStencilStateCreateInfo state = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    state.depthTestEnable       = info->depth_test_enable;
    state.depthWriteEnable      = info->depth_write_enable;
    state.depthBoundsTestEnable = info->depth_bounds_test_enable;
    state.depthCompareOp        = info->depth_compare_op;
    state.minDepthBounds        = info->min_depth_bounds;
    state.maxDepthBounds        = info->max_depth_bounds;
    return state;
}

// `BlendState - Lots of inlined dyn states
VkPipelineColorBlendStateCreateInfo create_vk_pipeline_color_blend_state(Create_Vk_Pipeline_Color_Blend_State_Info *info) {
    // @PipelineAllocations I think this state is one that contrasts to the others, these will likely be super 
    // ephemeral. I do not know to what extent I can effect color blending. Not very much I assume without 
    // extended dyn state 3... so I think this will require recompilations or state explosion...
    VkPipelineColorBlendStateCreateInfo blend_state = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend_state.attachmentCount = info->attachment_count;
    blend_state.pAttachments    = info->attachment_states;

    // just set to whatever cos this can be set dyn
    blend_state.blendConstants[0] = 1;
    blend_state.blendConstants[1] = 1;
    blend_state.blendConstants[2] = 1;
    blend_state.blendConstants[3] = 1;

    return blend_state;
}

void vkCmdSetLogicOpEnableEXT(VkCommandBuffer commandBuffer, VkBool32 logicOpEnable) {
    VkDevice device = get_gpu_instance()->vk_device;
    auto func = (PFN_vkCmdSetLogicOpEnableEXT) vkGetDeviceProcAddr(device, "vkCmdSetLogicOpEnableEXT");

    ASSERT(func != nullptr, "Logic Op Enable Cmd not found");
    return func(commandBuffer, logicOpEnable);
}
void vkCmdSetColorBlendEnableEXT(VkCommandBuffer commandBuffer, u32 firstAttachment, u32 attachmentCount, VkBool32 *pColorBlendEnables) {
    VkDevice device = get_gpu_instance()->vk_device;
    auto func = (PFN_vkCmdSetColorBlendEnableEXT) vkGetDeviceProcAddr(device, "vkCmdSetColorBlendEnableEXT");

    ASSERT(func != nullptr, "Color Blend Enable Cmd not found");
    return func(commandBuffer, firstAttachment, attachmentCount, pColorBlendEnables);
}
void vkCmdSetColorBlendEquationEXT(VkCommandBuffer commandBuffer, u32 firstAttachment, u32 attachmentCount, const VkColorBlendEquationEXT* pColorBlendEquations) {
    VkDevice device = get_gpu_instance()->vk_device;
    auto func = (PFN_vkCmdSetColorBlendEquationEXT) vkGetDeviceProcAddr(device, "vkCmdSetColorBlendEquationEXT");

    ASSERT(func != nullptr, "Color Blend Equation Cmd not found");
    return func(commandBuffer, firstAttachment, attachmentCount, pColorBlendEquations);
}
void vkCmdSetColorWriteMaskEXT(VkCommandBuffer commandBuffer, u32 firstAttachment, u32 attachmentCount, const VkColorComponentFlags* pColorWriteMasks) {
    VkDevice device = get_gpu_instance()->vk_device;
    auto func = (PFN_vkCmdSetColorWriteMaskEXT) vkGetDeviceProcAddr(device, "vkCmdSetColorWriteMaskEXT");

    ASSERT(func != nullptr, "Color Write Mask Cmd not found");
    return func(commandBuffer, firstAttachment, attachmentCount, pColorWriteMasks);
}
// `DynamicState
const VkDynamicState dyn_state_list[] = {
    VK_DYNAMIC_STATE_LINE_WIDTH,
    VK_DYNAMIC_STATE_DEPTH_BIAS,
    VK_DYNAMIC_STATE_BLEND_CONSTANTS,
    VK_DYNAMIC_STATE_DEPTH_BOUNDS,
    VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
    VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
    VK_DYNAMIC_STATE_STENCIL_REFERENCE,

    // Provided by VK_VERSION_1_3 - all below
    VK_DYNAMIC_STATE_CULL_MODE,
    VK_DYNAMIC_STATE_FRONT_FACE,
    VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
    VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
    VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
    VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE,
    VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
    VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
    VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
    VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE,
    VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
    VK_DYNAMIC_STATE_STENCIL_OP,
    VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE,
    VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,
    VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,
}; 
const u32 dyn_state_count = 22; //  This list is 23 last time I counted
    // @Todo @DynState list of possible other dyn states
    //      vertex input
    //      multisampling
VkPipelineDynamicStateCreateInfo create_vk_pipeline_dyn_state() {
    VkPipelineDynamicStateCreateInfo dyn_state = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        NULL,
        0x0,
        dyn_state_count,
        dyn_state_list,
    };
    return dyn_state;
}

// `PipelineLayout
VkPipelineLayout create_vk_pipeline_layout(VkDevice vk_device, Create_Vk_Pipeline_Layout_Info *info) {
    VkPipelineLayoutCreateInfo create_info = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

    VkResult check;
    create_info.setLayoutCount         = info->descriptor_set_layout_count;
    create_info.pSetLayouts            = info->descriptor_set_layouts;
    create_info.pushConstantRangeCount = info->push_constant_count;
    create_info.pPushConstantRanges    = info->push_constant_ranges;

    VkPipelineLayout layout;
    check = vkCreatePipelineLayout(vk_device, &create_info, ALLOCATION_CALLBACKS, &layout);
    DEBUG_OBJ_CREATION(vkCreatePipelineLayout, check);
    return layout;
}
void destroy_vk_pipeline_layouts(VkDevice vk_device, u32 count, VkPipelineLayout *pl_layouts) {
    for(int i = 0; i < count; ++i) {
        vkDestroyPipelineLayout(vk_device, pl_layouts[i], ALLOCATION_CALLBACKS);
    }
    memory_free_heap((void*)pl_layouts);
}

// PipelineRenderingInfo
VkPipelineRenderingCreateInfo create_vk_pipeline_rendering_info(Create_Vk_Pipeline_Rendering_Info_Info *info) {
    VkPipelineRenderingCreateInfo create_info = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    create_info.viewMask = info->view_mask;
    create_info.colorAttachmentCount    = info->color_attachment_count;
    create_info.pColorAttachmentFormats = info->color_attachment_formats;
    create_info.depthAttachmentFormat   = info->depth_attachment_format;
    create_info.stencilAttachmentFormat = info->stencil_attachment_format;
    return create_info;
}

// `Pipeline Final - static / descriptor pools
void create_vk_graphics_pipelines(VkDevice vk_device, VkPipelineCache cache, int count, Create_Vk_Pipeline_Info *info, VkPipeline *pipelines) {

    // @Todo wrap this state shit in a loop (multiple pipeline creation)
    
    // `Vertex Input Stage 1
    VkVertexInputBindingDescription *vertex_binding_descriptions = 
        (VkVertexInputBindingDescription*)memory_allocate_temp(
            sizeof(VkVertexInputBindingDescription) * info->vertex_input_state->input_binding_description_count, 8);

    Create_Vk_Vertex_Input_Binding_Description_Info binding_info;
    for(int i = 0; i < info->vertex_input_state->input_binding_description_count; ++i) {
        binding_info = {
            (u32)info->vertex_input_state->binding_description_bindings[i],
            (u32)info->vertex_input_state->binding_description_strides[i],
        };
        vertex_binding_descriptions[i] = create_vk_vertex_binding_description(&binding_info);
    }

    VkVertexInputAttributeDescription *vertex_attribute_descriptions = 
        (VkVertexInputAttributeDescription*)memory_allocate_temp(
            sizeof(VkVertexInputAttributeDescription) * info->vertex_input_state->input_attribute_description_count, 8);

    Create_Vk_Vertex_Input_Attribute_Description_Info attribute_info;
    for(int i = 0; i < info->vertex_input_state->input_attribute_description_count; ++i) {
        attribute_info = {
            (u32)info->vertex_input_state->attribute_description_locations[i],
            (u32)info->vertex_input_state->attribute_description_bindings[i],
            0, // offset corrected at draw time
            info->vertex_input_state->formats[i],
        };
        vertex_attribute_descriptions[i] = create_vk_vertex_attribute_description(&attribute_info);
    }

    Create_Vk_Pipeline_Vertex_Input_State_Info create_input_state_info = {
        (u32)info->vertex_input_state->input_binding_description_count,
        (u32)info->vertex_input_state->input_attribute_description_count,
        vertex_binding_descriptions,
        vertex_attribute_descriptions,
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = create_vk_pipeline_vertex_input_states(&create_input_state_info);

    VkPipelineInputAssemblyStateCreateInfo input_assembly = create_vk_pipeline_input_assembly_state(info->vertex_input_state->topology, VK_FALSE);


    // `Rasterization Stage 2
    Window *window = get_window_instance();
    VkPipelineViewportStateCreateInfo viewport = create_vk_pipeline_viewport_state(window);
    
    // @Todo setup multiple pipeline compilation for differing topology state
    VkPipelineRasterizationStateCreateInfo rasterization = create_vk_pipeline_rasterization_state(info->rasterization_state->polygon_modes[0], info->rasterization_state->cull_mode, info->rasterization_state->front_face);


    // `Fragment Shader Stage 3
    VkPipelineMultisampleStateCreateInfo multisample = create_vk_pipeline_multisample_state();

    Create_Vk_Pipeline_Depth_Stencil_State_Info depth_stencil_info = {
        (VkBool32)(info->fragment_shader_state->flags & GPU_FRAGMENT_SHADER_DEPTH_TEST_ENABLE_BIT),
        (VkBool32)(info->fragment_shader_state->flags & GPU_FRAGMENT_SHADER_DEPTH_WRITE_ENABLE_BIT >> 1),
        (VkBool32)(info->fragment_shader_state->flags & GPU_FRAGMENT_SHADER_DEPTH_WRITE_ENABLE_BIT >> 2),
        info->fragment_shader_state->depth_compare_op,
        info->fragment_shader_state->min_depth_bounds,
        info->fragment_shader_state->max_depth_bounds,
    };
    VkPipelineDepthStencilStateCreateInfo depth_stencil = create_vk_pipeline_depth_stencil_state(&depth_stencil_info);

    // `Output Stage 4
    Create_Vk_Pipeline_Color_Blend_State_Info blend_info = {
        1,
        &info->fragment_output_state->blend_state,
    };
    VkPipelineColorBlendStateCreateInfo blending = create_vk_pipeline_color_blend_state(&blend_info);

    // `Dynamic
    VkDynamicState dyn_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
        VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT
    };
    VkPipelineDynamicStateCreateInfo dynamic = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates    = dyn_states;

    VkGraphicsPipelineCreateInfo pl_create_info = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pl_create_info.stageCount = info->shader_stage_count;
    pl_create_info.pStages    = info->shader_stages;
    pl_create_info.pVertexInputState = &vertex_input;
    pl_create_info.pInputAssemblyState = &input_assembly;
    pl_create_info.pViewportState = &viewport;
    pl_create_info.pRasterizationState = &rasterization;
    pl_create_info.pDepthStencilState = &depth_stencil;
    pl_create_info.pColorBlendState = &blending;
    pl_create_info.pDynamicState = &dynamic;
    pl_create_info.layout = info->layout;
    pl_create_info.renderPass = info->renderpass;
    pl_create_info.subpass = 0;

    auto check = vkCreateGraphicsPipelines(vk_device, cache, 1, &pl_create_info, ALLOCATION_CALLBACKS, pipelines);
    DEBUG_OBJ_CREATION(vkCreateGraphicsPipelines, check);
}

// @Todo pipeline: increase possible use of dyn states, eg. vertex input, raster states etc.
// `Pipeline Final - dynamic + descriptor buffers
VkPipeline* create_vk_graphics_pipelines_heap(VkDevice vk_device, VkPipelineCache cache, u32 count, VkGraphicsPipelineCreateInfo *create_infos) {
    for(int i = 0; i < count; ++i) {
        create_infos[i].sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        create_infos[i].flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    }

    // @Todo @Pipeline @Allocation @Speed in future I need a separate creation function for temp and heap allocated pipelines:
    // (a better explanation is do not assume all pipelines should be allocated like this)
    // I can imagine that some pipelines will be more persistent than others and therefore might want to be put on the heap 
    // instead. 
    // Potential Options:
    //     1. make a large heap allocation where all the persistent pipelines are allocated, this keeps more room in temp allocation 
    //        to do what it was made for: short lifetime allocations
    //     2. allocate persistent pipelines first in temp allocation and reset up to this mark, this is a faster allocation, but as it 
    //        would only happen once in option 1, not worth an unshifting lump in the linear allocator?
    //     3. Unless I find something, option 1 looks best
    VkPipeline *pipelines = (VkPipeline*)memory_allocate_heap(sizeof(VkPipeline) * count, 8);
    auto check = vkCreateGraphicsPipelines(vk_device, cache, count, create_infos, ALLOCATION_CALLBACKS, pipelines);

    DEBUG_OBJ_CREATION(vkCreateGraphicsPipelines, check);
    return pipelines;
}

void destroy_vk_pipelines_heap(VkDevice vk_device, u32 count, VkPipeline *pipelines) {
    for(int i = 0; i < count; ++i) {
        vkDestroyPipeline(vk_device, pipelines[i], ALLOCATION_CALLBACKS);
    }
    memory_free_heap(pipelines);
}

// `Static Rendering (passes, subpassed, framebuffer)
VkAttachmentDescription create_vk_attachment_description(Create_Vk_Attachment_Description_Info * info) {
    VkAttachmentDescription description = {};
    description.format  = info->image_format;
    description.samples = info->sample_count;

    switch(info->color_depth_setting) {
        case GPU_ATTACHMENT_DESCRIPTION_SETTING_DONT_CARE_DONT_CARE:
            description.loadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            description.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            break;
        case GPU_ATTACHMENT_DESCRIPTION_SETTING_CLEAR_AND_STORE:
            description.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
            description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            break;
        default:
            ASSERT(false, "Invalid setting");
    }

    switch(info->stencil_setting) {
        case GPU_ATTACHMENT_DESCRIPTION_SETTING_DONT_CARE_DONT_CARE:
            description.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            break;
        case GPU_ATTACHMENT_DESCRIPTION_SETTING_CLEAR_AND_STORE:
            description.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
            description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
            break;
        default:
            ASSERT(false, "Invalid setting");
    }

    switch(info->layout_setting) {
        case GPU_ATTACHMENT_DESCRIPTION_SETTING_UNDEFINED_TO_COLOR:
            description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            description.finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        case GPU_ATTACHMENT_DESCRIPTION_SETTING_UNDEFINED_TO_PRESENT:
            description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            description.finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            break;
        default:
            ASSERT(false, "Invalid setting");
    }

    return description;
}

VkSubpassDescription create_vk_graphics_subpass_description(Create_Vk_Subpass_Description_Info *info) {
    VkSubpassDescription description    = {};
    description.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    description.inputAttachmentCount    = info->input_attachment_count;
    description.colorAttachmentCount    = info->color_attachment_count;
    description.pInputAttachments       = info->input_attachments;
    description.pColorAttachments       = info->color_attachments;
    description.pResolveAttachments     = info->resolve_attachments;
    description.pDepthStencilAttachment = info->depth_stencil_attachment;

    // @Todo preserve attachments
    return description;
}

VkSubpassDependency create_vk_subpass_dependency(Create_Vk_Subpass_Dependency_Info *info) {
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = info->src_subpass;
    dependency.dstSubpass = info->dst_subpass;

    switch(info->access_rules) {
    // @Todo These mainly come from the vulkan sync wiki. The todo part is to implement more of them
    case GPU_SUBPASS_DEPENDENCY_SETTING_ACQUIRE_TO_RENDER_TARGET_BASIC:
        // This one is just super basic for now...
        dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        break;
    case GPU_SUBPASS_DEPENDENCY_SETTING_WRITE_READ_COLOR_FRAGMENT:
        dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        break;
    case GPU_SUBPASS_DEPENDENCY_SETTING_WRITE_READ_DEPTH_FRAGMENT:
        dependency.srcStageMask  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        break;
    }
    return dependency;
}

VkRenderPass create_vk_renderpass(VkDevice vk_device, Create_Vk_Renderpass_Info *info) {
    VkRenderPassCreateInfo create_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    create_info.attachmentCount = info->attachment_count;
    create_info.pAttachments = info->attachments;
    create_info.subpassCount = info->subpass_count;
    create_info.pSubpasses = info->subpasses;
    create_info.dependencyCount = info->dependency_count;
    create_info.pDependencies = info->dependencies;
    VkRenderPass renderpass;
    auto check = vkCreateRenderPass(vk_device, &create_info, ALLOCATION_CALLBACKS, &renderpass);
    DEBUG_OBJ_CREATION(vkCreateRenderPass, check);
    return renderpass;
}
void destroy_vk_renderpass(VkDevice vk_device, VkRenderPass renderpass) {
    vkDestroyRenderPass(vk_device, renderpass, ALLOCATION_CALLBACKS);
}

// `Drawing and `Rendering
// @Note the names on these sorts of functions might be misleading, as create implies the need to destroy...
VkRenderingAttachmentInfo create_vk_rendering_attachment_info(Create_Vk_Rendering_Attachment_Info_Info *info) {
    VkRenderingAttachmentInfo attachment_info = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    attachment_info.imageView   = info->image_view;
    attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    attachment_info.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment_info.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;

    attachment_info.clearValue.color.float32[0] = info->clear_color[0];
    attachment_info.clearValue.color.float32[1] = info->clear_color[1];
    attachment_info.clearValue.color.float32[2] = info->clear_color[2];
    attachment_info.clearValue.color.float32[3] = info->clear_color[3];

    return attachment_info;
}
VkRenderingInfo create_vk_rendering_info(Create_Vk_Rendering_Info_Info *info) {
    VkRenderingInfo rendering_info = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    rendering_info.renderArea           = info->render_area;
    rendering_info.colorAttachmentCount = info->color_attachment_count;
    rendering_info.pColorAttachments    = info->color_attachment_infos;
    rendering_info.layerCount = 1;

    return rendering_info;
}

void set_default_draw_state(VkCommandBuffer vk_command_buffer) {
    cmd_vk_set_depth_test_disabled(vk_command_buffer);
    cmd_vk_set_depth_write_disabled(vk_command_buffer);
    cmd_vk_set_stencil_test_disabled(vk_command_buffer);
    cmd_vk_set_cull_mode_none(vk_command_buffer);
    cmd_vk_set_front_face_counter_clockwise(vk_command_buffer);
    cmd_vk_set_topology_triangle_list(vk_command_buffer);
    cmd_vk_set_depth_compare_op_never(vk_command_buffer);
    cmd_vk_set_depth_bounds_test_disabled(vk_command_buffer);
    cmd_vk_set_stencil_op(vk_command_buffer);
    cmd_vk_set_rasterizer_discard_disabled(vk_command_buffer);
    cmd_vk_set_depth_bias_disabled(vk_command_buffer);
    cmd_vk_set_viewport(vk_command_buffer);
    cmd_vk_set_scissor(vk_command_buffer);
    cmd_vk_set_primitive_restart_disabled(vk_command_buffer);
}

// `Memory Dependencies
VkImageMemoryBarrier2 fill_image_barrier_transfer_cao_to_present(MemDep_Queue_Transfer_Info_Image *info) {
    VkImageSubresourceRange view  = {};
    view.aspectMask               = VK_IMAGE_ASPECT_COLOR_BIT;
    view.baseMipLevel             = 0;
    view.levelCount               = 1;
    view.baseArrayLayer           = 0;
    view.layerCount               = 1;

    VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.srcStageMask          = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask         = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask          = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    barrier.dstAccessMask         = 0x0;
    barrier.oldLayout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout             = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcQueueFamilyIndex   = info->release_queue_index;
    barrier.dstQueueFamilyIndex   = info->acquire_queue_index;
    barrier.image                 = info->image;
    barrier.subresourceRange      = view;

    return barrier;
}
VkImageMemoryBarrier2 fill_image_barrier_transition_undefined_to_cao(MemDep_Queue_Transition_Info_Image *info) {
    VkImageSubresourceRange view  = {};
    view.aspectMask               = VK_IMAGE_ASPECT_COLOR_BIT;
    view.baseMipLevel             = 0;
    view.levelCount               = 1;
    view.baseArrayLayer           = 0;
    view.layerCount               = 1;

    VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.srcStageMask          = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask         = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask          = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccessMask         = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout             = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout             = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                 = info->image;
    barrier.subresourceRange      = info->view;

    return barrier;
}
VkImageMemoryBarrier2 fill_image_barrier_transition_dst_to_cao(MemDep_Queue_Transition_Info_Image *info) {
    VkImageSubresourceRange view  = {};
    view.aspectMask               = VK_IMAGE_ASPECT_COLOR_BIT;
    view.baseMipLevel             = 0;
    view.levelCount               = 1;
    view.baseArrayLayer           = 0;
    view.layerCount               = 1;

    VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.srcStageMask          = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask         = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask          = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccessMask         = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout             = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout             = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                 = info->image;
    barrier.subresourceRange      = info->view;

    return barrier;
}
VkImageMemoryBarrier2 fill_image_barrier_transition_undefined_to_dst(MemDep_Queue_Transition_Info_Image *info) {
    VkImageSubresourceRange view  = {};
    view.aspectMask               = VK_IMAGE_ASPECT_COLOR_BIT;
    view.baseMipLevel             = 0;
    view.levelCount               = 1;
    view.baseArrayLayer           = 0;
    view.layerCount               = 1;

    VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.srcStageMask          = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask         = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask          = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccessMask         = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout             = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout             = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                 = info->image;
    barrier.subresourceRange      = info->view;

    return barrier;
}

VkBufferMemoryBarrier2 fill_buffer_barrier_transfer(MemDep_Queue_Transfer_Info_Buffer *info) {
    VkBufferMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2}; 
    barrier.offset              = info->offset;
    barrier.size                = info->size;
    barrier.buffer              = info->vk_buffer;

    barrier.srcStageMask        = VK_PIPELINE_STAGE_TRANSFER_BIT;
    barrier.dstStageMask        = VK_PIPELINE_STAGE_TRANSFER_BIT;
    barrier.srcAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;

    barrier.srcQueueFamilyIndex = info->release_queue_index;
    barrier.dstQueueFamilyIndex = info->acquire_queue_index;
    
    return barrier;
}

VkDependencyInfo fill_vk_dependency_buffer(VkBufferMemoryBarrier2 *buffer_barrier) {
    VkDependencyInfo dependency         = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency.bufferMemoryBarrierCount = 1;
    dependency.pBufferMemoryBarriers    = buffer_barrier;
    return dependency;
}

VkDependencyInfo fill_vk_dependency(Fill_Vk_Dependency_Info *info) {
    VkDependencyInfo dependency_info = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};

    dependency_info.memoryBarrierCount       = info->memory_barrier_count;
    dependency_info.bufferMemoryBarrierCount = info->buffer_barrier_count;
    dependency_info.imageMemoryBarrierCount  = info->image_barrier_count;

    dependency_info.pMemoryBarriers          = info->memory_barriers;
    dependency_info.pBufferMemoryBarriers    = info->buffer_barriers;
    dependency_info.pImageMemoryBarriers     = info->image_barriers;

    return dependency_info;
}

// `Resources

// `GpuLinearAllocator - Buffer optimisation
struct Gpu_Linear_Allocator {
    u64 used;
    u64 cap;
    VkBuffer buffer;
    VmaAllocation vma_allocation;
};
// returns offset into the buffer;
u64 make_gpu_allocation(Gpu_Linear_Allocator *allocator, u64 size, u8 alignment) {
    u8 alignment_mask = allocator->used & (alignment - 1);
    allocator->used = (allocator->used + alignment_mask) & ~alignment_mask;

    u64 ret = allocator->used;
    allocator->used += size;
    return ret;
}
void empty_gpu_allocator(Gpu_Linear_Allocator *allocator) {
    allocator->used = 0;
}
void cut_tail_gpu_allocator(Gpu_Linear_Allocator *allocator, u64 size) {
    allocator->used -= size;
}

// Allocate the perframe and persistent buffers
Gpu_Linear_Allocator create_gpu_linear_allocator_host(VmaAllocator vma_allocator, u64 size) {
    VkBufferCreateInfo buffer_create_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_create_info.size  = size;
    buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    Gpu_Linear_Allocator gpu_linear_allocator;
    auto check = vmaCreateBuffer(vma_allocator, &buffer_create_info, &allocation_create_info, &gpu_linear_allocator.buffer, &gpu_linear_allocator.vma_allocation, NULL);
    DEBUG_OBJ_CREATION(vmaCreateBuffer, check);

    return gpu_linear_allocator;
}

Gpu_Linear_Allocator create_gpu_linear_allocator_device(VmaAllocator vma_allocator, u64 size) {
    VkBufferCreateInfo buffer_create_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_create_info.size  = size;
    buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    Gpu_Linear_Allocator gpu_linear_allocator;
    auto check = vmaCreateBuffer(vma_allocator, &buffer_create_info, &allocation_create_info, &gpu_linear_allocator.buffer, &gpu_linear_allocator.vma_allocation, NULL);
    DEBUG_OBJ_CREATION(vmaCreateBuffer, check);

    return gpu_linear_allocator;
}

// `Buffers
void create_src_dst_vertex_buffer_pair(VmaAllocator vma_allocator, u64 size, Gpu_Buffer *src, Gpu_Buffer *dst) {
    VkBufferCreateInfo buffer_create_info_dst = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_create_info_dst.size  = size;
    buffer_create_info_dst.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo allocation_create_info_dst = {};
    allocation_create_info_dst.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    vmaCreateBuffer(vma_allocator, &buffer_create_info_dst, &allocation_create_info_dst, &dst->vk_buffer, &dst->vma_allocation, NULL);

    VkBufferCreateInfo buffer_create_info_src = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_create_info_src.size  = size;
    buffer_create_info_src.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo allocation_create_info_src = {};
    allocation_create_info_src.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_create_info_src.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    Gpu_Buffer gpu_buffer;
    vmaCreateBuffer(vma_allocator, &buffer_create_info_src, &allocation_create_info_src, &gpu_buffer.vk_buffer, &gpu_buffer.vma_allocation, NULL);

}
Gpu_Buffer create_dst_vertex_buffer(VmaAllocator vma_allocator, u64 size) {
    VkBufferCreateInfo buffer_create_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_create_info.size  = size;
    buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    Gpu_Buffer gpu_buffer;
    VmaAllocationInfo allocation_info;
    vmaCreateBuffer(vma_allocator, &buffer_create_info, &allocation_create_info, &gpu_buffer.vk_buffer, &gpu_buffer.vma_allocation, NULL);
    return gpu_buffer;
}
Gpu_Buffer create_src_buffer(VmaAllocator vma_allocator, u64 size) {
    VkBufferCreateInfo buffer_create_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_create_info.size  = size;
    buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    Gpu_Buffer gpu_buffer;
    vmaCreateBuffer(vma_allocator, &buffer_create_info, &allocation_create_info, &gpu_buffer.vk_buffer, &gpu_buffer.vma_allocation, NULL);
    return gpu_buffer;
}

// `Images
Gpu_Image create_vma_color_image(VmaAllocator vma_allocator, u32 width, u32 height) {
    VkImageCreateInfo image_create_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    image_create_info.imageType         = VK_IMAGE_TYPE_2D;
    image_create_info.extent.width      = width;
    image_create_info.extent.height     = height;
    image_create_info.extent.depth      = 1;
    image_create_info.mipLevels         = 1;
    image_create_info.arrayLayers       = 1;

    // Idk what format I should use?
    image_create_info.format            = VK_FORMAT_R8G8B8A8_UNORM;
    image_create_info.tiling            = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.samples           = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.usage             = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                          VK_IMAGE_USAGE_SAMPLED_BIT;
     
    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_create_info.flags = 0x0;
    allocation_create_info.priority = 1.0f;
     
    Gpu_Image gpu_image;
    auto check = vmaCreateImage(vma_allocator, &image_create_info, &allocation_create_info, 
                                &gpu_image.vk_image, &gpu_image.vma_allocation, nullptr);
    DEBUG_OBJ_CREATION(vmaCreateImage, check);

    return gpu_image;
}
Gpu_Image create_vma_color_attachment(VmaAllocator vma_allocator, u32 width, u32 height) {
    VkImageCreateInfo image_create_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    image_create_info.imageType         = VK_IMAGE_TYPE_2D;
    image_create_info.extent.width      = width;
    image_create_info.extent.height     = height;
    image_create_info.extent.depth      = 1;
    image_create_info.mipLevels         = 1;
    image_create_info.arrayLayers       = 1;

    // Idk what format I should use?
    image_create_info.format            = COLOR_ATTACHMENT_FORMAT;
    image_create_info.tiling            = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.samples           = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.usage             = VK_IMAGE_USAGE_SAMPLED_BIT |
                                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
     
    VmaAllocationCreateInfo allocation_create_info = {};
    allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_create_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocation_create_info.priority = 1.0f;
     
    Gpu_Image gpu_image;
    auto check = vmaCreateImage(vma_allocator, &image_create_info, &allocation_create_info, 
                                &gpu_image.vk_image, &gpu_image.vma_allocation, nullptr);
    DEBUG_OBJ_CREATION(vmaCreateImage, check);

    return gpu_image;
}

VkSampler create_vk_sampler(VkDevice vk_device, Create_Vk_Sampler_Info *info) {
    VkSamplerCreateInfo create_info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

    create_info.magFilter = VK_FILTER_LINEAR;
    create_info.minFilter = VK_FILTER_LINEAR;

    create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    create_info.anisotropyEnable = VK_TRUE;
    create_info.maxAnisotropy = info->max_anisotropy;

    create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    create_info.mipLodBias = 0.0f;
    create_info.minLod = 0.0f;
    create_info.maxLod = 0.0f;

    // @Todo percentage closer filtering shadow maps
    create_info.compareEnable = VK_FALSE;
    //create_info.compareOp = 

    VkSampler sampler;
    auto check = vkCreateSampler(vk_device, &create_info, ALLOCATION_CALLBACKS, &sampler);
    DEBUG_OBJ_CREATION(vkCreateSampler, check);

    return sampler;
}
void destroy_vk_sampler(VkDevice vk_device, VkSampler sampler) {
    vkDestroySampler(vk_device, sampler, ALLOCATION_CALLBACKS);
}

// `Resource Commands
void cmd_vk_copy_buffer(VkCommandBuffer vk_command_buffer, VkBuffer from, VkBuffer to, u64 size) {
    VkBufferCopy2 region = {VK_STRUCTURE_TYPE_BUFFER_COPY_2};
    region.srcOffset     = 0;
    region.dstOffset     = 0;
    region.size          = size;

    VkCopyBufferInfo2 copy_info = {VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2};
    copy_info.srcBuffer         = from;
    copy_info.dstBuffer         = to;
    copy_info.regionCount       = 1;
    copy_info.pRegions          = &region;

    vkCmdCopyBuffer2(vk_command_buffer, &copy_info);
}

// @Note could be inlined cos so small? who cares...
void destroy_vma_buffer(VmaAllocator vma_allocator, Gpu_Buffer *gpu_buffer) {
    vmaDestroyBuffer(vma_allocator, gpu_buffer->vk_buffer, gpu_buffer->vma_allocation);
}
void destroy_vma_image(VmaAllocator vma_allocator, Gpu_Image *gpu_image) {
    vmaDestroyImage(vma_allocator, gpu_image->vk_image, gpu_image->vma_allocation);
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
