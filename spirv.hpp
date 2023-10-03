#ifndef SOL_SPIRV_HPP_INCLUDE_GUARD_
#define SOL_SPIRV_HPP_INCLUDE_GUARD_

#include "basic.h"
#include <vulkan/vulkan_core.h>

struct Spirv_Binding {
    int set;
    int count;
    int binding;
    VkDescriptorType type;
};
struct Parsed_Spirv {
    VkShaderStageFlagBits stage;
    int binding_count;
    Spirv_Binding *bindings;
};
Parsed_Spirv parse_spirv(u64 byte_count, const u32 *spirv, int *count); 

#if TEST
void test_spirv();
#endif

#endif
