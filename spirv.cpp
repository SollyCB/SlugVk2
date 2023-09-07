#include "spirv.hpp"

enum Shader_Stage_Flags {
    SHADER_STAGE_VERTEX_BIT = 0x00000001,
    SHADER_STAGE_TESSELLATION_CONTROL_BIT = 0x00000002,
    SHADER_STAGE_TESSELLATION_EVALUATION_BIT = 0x00000004,
    SHADER_STAGE_GEOMETRY_BIT = 0x00000008,
    SHADER_STAGE_FRAGMENT_BIT = 0x00000010,
    SHADER_STAGE_COMPUTE_BIT = 0x00000020,
    SHADER_STAGE_ALL_GRAPHICS = 0x0000001F,
    SHADER_STAGE_ALL = 0x7FFFFFFF,
    // Provided by VK_KHR_ray_tracing_pipeline
    SHADER_STAGE_RAYGEN_BIT_KHR = 0x00000100,
    // Provided by VK_KHR_ray_tracing_pipeline
    SHADER_STAGE_ANY_HIT_BIT_KHR = 0x00000200,
    // Provided by VK_KHR_ray_tracing_pipeline
    SHADER_STAGE_CLOSEST_HIT_BIT_KHR = 0x00000400,
    // Provided by VK_KHR_ray_tracing_pipeline
    SHADER_STAGE_MISS_BIT_KHR = 0x00000800,
    // Provided by VK_KHR_ray_tracing_pipeline
    SHADER_STAGE_INTERSECTION_BIT_KHR = 0x00001000,
    // Provided by VK_KHR_ray_tracing_pipeline
    SHADER_STAGE_CALLABLE_BIT_KHR = 0x00002000,
    // Provided by VK_EXT_mesh_shader
    SHADER_STAGE_TASK_BIT_EXT = 0x00000040,
    // Provided by VK_EXT_mesh_shader
    SHADER_STAGE_MESH_BIT_EXT = 0x00000080,
    // Provided by VK_HUAWEI_subpass_shading
    SHADER_STAGE_SUBPASS_SHADING_BIT_HUAWEI = 0x00004000,
    // Provided by VK_HUAWEI_cluster_culling_shader
    SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI = 0x00080000,
    // Provided by VK_NV_ray_tracing
    SHADER_STAGE_RAYGEN_BIT_NV = SHADER_STAGE_RAYGEN_BIT_KHR,
    // Provided by VK_NV_ray_tracing
    SHADER_STAGE_ANY_HIT_BIT_NV = SHADER_STAGE_ANY_HIT_BIT_KHR,
    // Provided by VK_NV_ray_tracing
    SHADER_STAGE_CLOSEST_HIT_BIT_NV = SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
    // Provided by VK_NV_ray_tracing
    SHADER_STAGE_MISS_BIT_NV = SHADER_STAGE_MISS_BIT_KHR,
    // Provided by VK_NV_ray_tracing
    SHADER_STAGE_INTERSECTION_BIT_NV = SHADER_STAGE_INTERSECTION_BIT_KHR,
    // Provided by VK_NV_ray_tracing
    SHADER_STAGE_CALLABLE_BIT_NV = SHADER_STAGE_CALLABLE_BIT_KHR,
    // Provided by VK_NV_mesh_shader
    SHADER_STAGE_TASK_BIT_NV = SHADER_STAGE_TASK_BIT_EXT,
    // Provided by VK_NV_mesh_shader
    SHADER_STAGE_MESH_BIT_NV = SHADER_STAGE_MESH_BIT_EXT,
};

enum Descriptor_Type {
    DESCRIPTOR_TYPE_SAMPLER = 0,
    DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 1,
    DESCRIPTOR_TYPE_SAMPLED_IMAGE = 2,
    DESCRIPTOR_TYPE_STORAGE_IMAGE = 3,
    DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER = 4,
    DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER = 5,
    DESCRIPTOR_TYPE_UNIFORM_BUFFER = 6,
    DESCRIPTOR_TYPE_STORAGE_BUFFER = 7,
    DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC = 8,
    DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC = 9,
    DESCRIPTOR_TYPE_INPUT_ATTACHMENT = 10,
    // Provided by VK_VERSION_1_3
    DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK = 1000138000,
    // Provided by VK_KHR_acceleration_structure
    DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR = 1000150000,
    // Provided by VK_NV_ray_tracing
    DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV = 1000165000,
    // Provided by VK_QCOM_image_processing
    DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM = 1000440000,
    // Provided by VK_QCOM_image_processing
    DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM = 1000440001,
    // Provided by VK_EXT_mutable_descriptor_type
    DESCRIPTOR_TYPE_MUTABLE_EXT = 1000351000,
    // Provided by VK_EXT_inline_uniform_block
    DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT =
    DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
    // Provided by VK_VALVE_mutable_descriptor_type
    DESCRIPTOR_TYPE_MUTABLE_VALVE = DESCRIPTOR_TYPE_MUTABLE_EXT,
};

enum Decoration {
    SPEC_ID = 1,
    BLOCK = 2,
    BUILTIN = 11,
    UNIFORM = 26,
    LOCATION = 30,
    BINDING = 33,
    DESCRIPTOR_SET = 34,
};
enum Decoration_Flags {
    SPEC_ID_BIT = 1,
    BLOCK_BIT = 2,
    BUILTIN_BIT = 11,
    UNIFORM_BIT = 26,
    LOCATION_BIT = 30,
    BINDING_BIT = 33,
    DESCRIPTOR_SET_BIT = 34,
};
enum Storage_Type {
    STORAGE_UNIFORM_CONSTANT = 0,
    STORAGE_INPUT = 1,
    STORAGE_UNIFORM = 2,
    STORAGE_OUTPUT = 3,
    STORAGE_PUSH_CONSTANT = 9,
    STORAGE_IMAGE = 11,
    STORAGE_STORAGE_BUFFER = 12,
};
enum Image_Type {
    UNKNOWN,
    TEXEL_UNIFORM,
    TEXEL_STORAGE,
    IMAGE_INPUT,
    IMAGE_STORAGE,
    IMAGE_SAMPLED,
};
enum Op_Type {
    OP_TYPE_SAMPLER,
    OP_TYPE_SAMPLED_IMAGE,
    OP_TYPE_IMAGE,
    OP_TYPE_ARRAY,
    OP_TYPE_POINTER,
};

struct Id {
    Op_Type op_type;

    u8 decoration_flags;
    u16 location;
    u16 binding;
    u16 descriptor_set;

    Image_Type image_type; 
    Storage_Type storage_type;
    u16 length;
    u16 result_id;
};

// @Todo also track the offset of a descriptor according to its binding
Parsed_Spirv parse_spirv(u64 byte_count, const u32 *spirv) {

    const u32 magic_number = 0x07230203;
    if (byte_count & 3 != 0) {
        ASSERT(false, "Spirv Byte Count is not a multiple of four");
        return {};
    }
    if (magic_number != spirv[0]) {
        ASSERT(false, "Spirv no magic word");
        return {};
    }

    u64 mark = get_mark_temp();
    u32 id_bound = spirv[3];
    Id *ids = (Id*)memory_allocate_temp(sizeof(Id) * id_bound, 8);
    u32 *var_ids = (u32*)memory_allocate_temp(sizeof(u32) * id_bound, 8);
    u32 var_index = 0;

    Id *id;
    u16 *instr_size;
    u16 *opcode;
    u32 *instr;

    u16 set_index = 0;
    u16 binding_count = 0;

    u64 offset = 5;
    u64 word_count = byte_count >> 2;
    while(offset < word_count) {

        opcode = (u16*)(spirv + offset);
        instr_size = opcode + 1;
        instr = (u32*)(opcode + 2);

        switch(*opcode) {
#if DEBUG
        case 5:
        {
            //std::cout << "Id: " << instr[0] << ", Name: " << (const char*)(instr + 1) << '\n';
            break;
        }
#endif

        // OpVariable
        case 59:
        {
            id = ids + instr[0];
            id->result_id = instr[1];
            id->storage_type = (Storage_Type)instr[2];

            var_ids[var_index] = instr[0];
            var_index++;

            break;
        }
        // OpDecorate
        case 71:
        {
            id = ids + instr[0];

            switch((Decoration)(instr[1])) {
            case SPEC_ID:
            {
                id->decoration_flags |= (u8)SPEC_ID_BIT;
                break;
            }
            case BLOCK:
            {
                id->decoration_flags |= (u8)BLOCK_BIT;
                break;
            }
            case BUILTIN:
            {
                id->decoration_flags |= (u8)BUILTIN_BIT;
                break;
            }
            case UNIFORM:
            {
                id->decoration_flags |= (u8)UNIFORM_BIT;
                break;
            }
            case LOCATION:
            {
                id->decoration_flags |= (u8)LOCATION_BIT;
                id->location = (u16)instr[2];
                break;
            }
            case BINDING:
            {
                id->decoration_flags |= (u8)BINDING_BIT;
                id->binding = (u16)instr[2];

                binding_count++;
                break;
            }
            case DESCRIPTOR_SET:
            {
                id->decoration_flags |= (u8)DESCRIPTOR_SET_BIT;
                id->descriptor_set = (u16)instr[2];

                if (id->descriptor_set >= set_index)
                    set_index = id->descriptor_set;

                break;
            }
            } // switch decoration

            break; // case OpDecorate
        }
        // OpTypeSampler
        case 26:
        {
            ids[*instr].op_type = OP_TYPE_SAMPLER;
            break;
        }
        // OpTypeSampledImage
        case 27:
        {
            ids[*instr].op_type = OP_TYPE_SAMPLED_IMAGE;
            ids[*instr].result_id = instr[1];
            break;
        }
        // OpTypeImage
        case 25:
        {
            id = ids + instr[0];
            id->op_type = OP_TYPE_IMAGE;
            
            // switch sampled
            ASSERT(instr[6] < 3, "");
            switch(instr[6]) {
            case 2:
            {
                // switch dimensionality
                switch(instr[2]) {
                case 6:
                    id->image_type = IMAGE_INPUT;
                    break;
                case 5:
                    id->image_type = TEXEL_STORAGE;
                    break;
                default:
                    id->image_type = IMAGE_STORAGE;
                    break;
                } // switch dimensionality
                break;
            }
            case 1:
            {
                if (instr[2] == 5) {
                    id->image_type = TEXEL_UNIFORM;
                    break;
                } else {
                    id->image_type = IMAGE_SAMPLED;
                }
            }
            case 0:
            {
                id->image_type = UNKNOWN;
                break;
            }
            } // switch sampled
        }
        // OpTypeArray
        case 28:
        {
            id = ids + instr[0];
            id->op_type = OP_TYPE_ARRAY;
            id->result_id = instr[1];
            id->length = instr[2];
            break;
        }
        // OpTypePointer
        case 32:
        {
            id = ids + instr[0];
            id->op_type = OP_TYPE_POINTER;
            id->result_id = instr[2];
            break;
        }
        default:
            break;
        } // switch opcode

        offset += *instr_size;
    }

    Create_Vk_Descriptor_Set_Layout_Info *set_infos = (Create_Vk_Descriptor_Set_Layout_Info*)memory_allocate_temp(
            (sizeof(Create_Vk_Descriptor_Set_Layout_Info) * set_index) +
            ((sizeof(u32))  + // size of binding count
             (sizeof(u64))  + // size of descriptor type, without extensions this can be smaller
             (sizeof(u32))) * // size of shader stage flags (this size may need to be increased at some point...
             binding_count    // the number of bindings referenced in the shader
            , 8);

    // @Speed see the revision below
    //
    // @Reminder looping through vars finding common descriptor set indices in order to know how much memory 
    // needs to be allocated to each set BEFORE the loop which groups data into the 'set_infos'
    // without this I cannot allocate like data sequentially: I want the pointers in 'set_infos' to point to memory
    // allocated directly after the struct, so that access to each struct does not cache miss, for example:
    //
    //              set_infos[i] end    v set_infos[i].descriptor_types start   etc, etc 
    //                  v  
    //  0000000000000000000000000000000000000...000 <- begin set_infos[i + 1]
    //  ^
    // set_infos[i] start |  ^ set_infos[i].bindings start
    //
    // Revision: This is one way of doing it, but as i just dont know the access pattern for the data that vulkan will    // use, this will just have to be tested later. I will implement the simple way, but this is something small 
    // to come back to. The simple way being make temp allocations in the loop for each pointer in the struct. 
    // this is probably just as good depending on the access pattern, as it makes the structs packed... but the 
    // data that the structs reference becomes further away...
    /*for(int i = 0; i < var_index; ++i) {
        if (ids[var_ids[i]].decoration_flags & DESCRIPTOR_SET_BIT)
    }*/

    Id *var;
    Id *result_type;
    for(int i = 0; i < var_index; ++i) {
        
        var = ids + var_ids[i];
        result_type = ids + var->result_id;
        ASSERT(result_type->op_type == OP_TYPE_POINTER, "Vars should reference pointers");
        result_type = ids[result_type->result_id]; // deref pointer

        if (var->decoration_flags & (u8)DESCRIPTOR_SET_BIT)
            set_index = var->descriptor_set;

        set_infos[set_index].bindings;

        switch(result_type->op_type) {
        case OP_TYPE_ARRAY:
        {
            break;
        }
        case OP_TYPE_SAMPLER:
        {
            break;
        }
        case OP_TYPE_SAMPLED_IMAGE:
        {
            break;
        }
        case OP_TYPE_IMAGE:
        {
            break;
        }
        case OP_TYPE_POINTER:
        {
            break;
        }
        case OP_TYPE_FORWARD_POINTER:
        {
            break;
        }
        } // switch result optype
    }

    cut_diff_temp(mark);
    return {};
}
