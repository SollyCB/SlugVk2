#ifndef SOL_SPIRV_HPP_INCLUDE_GUARD_
#define SOL_SPIRV_HPP_INCLUDE_GUARD_

#include "basic.h"
#include <vulkan/vulkan_core.h>

VkDescriptorSetLayoutCreateInfo* parse_spirv(u64 byte_count, const u32 *spirv, int *count); 

#endif
