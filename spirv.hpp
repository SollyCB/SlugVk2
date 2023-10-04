#ifndef SOL_SPIRV_HPP_INCLUDE_GUARD_
#define SOL_SPIRV_HPP_INCLUDE_GUARD_

#include "basic.h"
#include <vulkan/vulkan_core.h>

struct Spirv_Descriptor {
    int set;
    int count;
    int binding;
    VkDescriptorType type;
};
struct Parsed_Spirv {
    VkShaderStageFlagBits stage;
    int binding_count;
    Spirv_Descriptor *bindings; // should be renamed descriptors, but they are synonymous pretty much so?
};
Parsed_Spirv parse_spirv(u64 byte_count, const u32 *spirv, int *descriptor_set_count);

//     ** DO NOT BIND A DESCRIPTOR TO A NON CONTIGUOUS BINDING **
// This function relies on bindings being incremented the same way as descriptor sets, 
// (it is more efficient to parse and easier to understand and debug)
struct Spirv_Set {
    int binding_count;
    VkDescriptorSetLayoutBinding *bindings;
};
Spirv_Set* group_spirv(int count, Parsed_Spirv *parsed_spirv, int *returned_set_count);

#if TEST
void test_spirv();
#endif

#endif
