#ifndef SOL_SPV_HPP_INCLUDE_GUARD_
#define SOL_SPV_HPP_INCLUDE_GUARD_

#include "basic.h"


struct Create_Vk_Descriptor_Set_Layout_Info; // defined in gpu.hpp
struct Parsed_Spirv {
   u32 count;
   Create_Vk_Descriptor_Set_Layout_Info *set_layout_infos;
};

Parsed_Spirv parse_spirv(u64 byte_count, const u32 *spirv);

#endif // include guard
