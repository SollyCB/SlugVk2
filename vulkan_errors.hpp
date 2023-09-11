#ifndef SOL_VULKAN_ERRORS_HPP_INCLUDE_GUARD_
#define SOL_VULKAN_ERRORS_HPP_INCLUDE_GUARD_

#include <iostream>

enum Vulkan_Result {
    VULKAN_SUCCESS = 0,
    VULKAN_NOT_READY = 1,
    VULKAN_TIMEOUT = 2,
    VULKAN_EVENT_SET = 3,
    VULKAN_EVENT_RESET = 4,
    VULKAN_INCOMPLETE = 5,
    VULKAN_ERROR_OUT_OF_HOST_MEMORY = -1,
    VULKAN_ERROR_OUT_OF_DEVICE_MEMORY = -2,
    VULKAN_ERROR_INITIALIZATION_FAILED = -3,
    VULKAN_ERROR_DEVICE_LOST = -4,
    VULKAN_ERROR_MEMORY_MAP_FAILED = -5,
    VULKAN_ERROR_LAYER_NOT_PRESENT = -6,
    VULKAN_ERROR_EXTENSION_NOT_PRESENT = -7,
    VULKAN_ERROR_FEATURE_NOT_PRESENT = -8,
    VULKAN_ERROR_INCOMPATIBLE_DRIVER = -9,
    VULKAN_ERROR_TOO_MANY_OBJECTS = -10,
    VULKAN_ERROR_FORMAT_NOT_SUPPORTED = -11,
    VULKAN_ERROR_FRAGMENTED_POOL = -12,
    VULKAN_ERROR_UNKNOWN = -13,
    VULKAN_ERROR_OUT_OF_POOL_MEMORY = -1000069000,
    VULKAN_ERROR_INVALID_EXTERNAL_HANDLE = -1000072003,
    VULKAN_ERROR_FRAGMENTATION = -1000161000,
    VULKAN_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS = -1000257000,
    VULKAN_PIPELINE_COMPILE_REQUIRED = 1000297000,
    VULKAN_ERROR_SURFACE_LOST_KHR = -1000000000,
    VULKAN_ERROR_NATIVE_WINDOW_IN_USE_KHR = -1000000001,
    VULKAN_SUBOPTIMAL_KHR = 1000001003,
    VULKAN_ERROR_OUT_OF_DATE_KHR = -1000001004,
    VULKAN_ERROR_INCOMPATIBLE_DISPLAY_KHR = -1000003001,
    VULKAN_ERROR_VALIDATION_FAILED_EXT = -1000011001,
    VULKAN_ERROR_INVALID_SHADER_NV = -1000012000,
    VULKAN_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR = -1000023000,
    VULKAN_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR = -1000023001,
    VULKAN_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR = -1000023002,
    VULKAN_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR = -1000023003,
    VULKAN_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR = -1000023004,
    VULKAN_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR = -1000023005,
    VULKAN_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT = -1000158000,
    VULKAN_ERROR_NOT_PERMITTED_KHR = -1000174001,
    VULKAN_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT = -1000255000,
    VULKAN_THREAD_IDLE_KHR = 1000268000,
    VULKAN_THREAD_DONE_KHR = 1000268001,
    VULKAN_OPERATION_DEFERRED_KHR = 1000268002,
    VULKAN_OPERATION_NOT_DEFERRED_KHR = 1000268003,
    VULKAN_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR = -1000299000,
    VULKAN_ERROR_COMPRESSION_EXHAUSTED_EXT = -1000338000,
    VULKAN_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT = 1000482000,
    VULKAN_ERROR_OUT_OF_POOL_MEMORY_KHR = VULKAN_ERROR_OUT_OF_POOL_MEMORY,
    VULKAN_ERROR_INVALID_EXTERNAL_HANDLE_KHR = VULKAN_ERROR_INVALID_EXTERNAL_HANDLE,
    VULKAN_ERROR_FRAGMENTATION_EXT = VULKAN_ERROR_FRAGMENTATION,
    VULKAN_ERROR_NOT_PERMITTED_EXT = VULKAN_ERROR_NOT_PERMITTED_KHR,
    VULKAN_ERROR_INVALID_DEVICE_ADDRESS_EXT = VULKAN_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS,
    VULKAN_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR = VULKAN_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS,
    VULKAN_PIPELINE_COMPILE_REQUIRED_EXT = VULKAN_PIPELINE_COMPILE_REQUIRED,
    VULKAN_ERROR_PIPELINE_COMPILE_REQUIRED_EXT = VULKAN_PIPELINE_COMPILE_REQUIRED,
};

const char* match_vk_error(Vulkan_Result error);

#if DEBUG

#ifndef _WIN32
#define DEBUG_OBJ_CREATION(creation_func, err_code)  \
  if (err_code != (VkResult)VULKAN_SUCCESS) { \
    const char* err_msg = (match_vk_error((Vulkan_Result)err_code)); \
    std::cerr << "OBJ CREATION ERROR: " << #creation_func << \
    " returned " << err_msg << ", (" << __FILE__ << ", " << __LINE__ << ")\n";  \
    __builtin_trap(); \
  } 

#else

#define DEBUG_OBJ_CREATION(creation_func, err_code)  \
  if (err_code != (VkResult)VULKAN_SUCCESS) { \
    const char* err_msg = (match_vk_error((Vulkan_Result)err_code)); \
    std::cerr << "OBJ CREATION ERROR: " << #creation_func << \
    " returned " << err_msg << ", (" << __FILE__ << ", " << __LINE__ << ")\n";  \
    abort(); \
  }

#endif // WIN32

#else

#define DEBUG_ABORT_CREATION(obj_name, creation_func, err_code)

#endif // DEBUG

#endif // include guard
