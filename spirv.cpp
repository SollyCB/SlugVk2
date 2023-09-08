#include "spirv.hpp"

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

static void sort_vars(int *var_ids, Id *ids, int low, int high) {
    if (low < high) {
        int lower = low - 1;
        int c;
        for(int i = low;  i < high; ++i) {
            if (ids[var_ids[i]].descriptor_set < ids[var_ids[high]].descriptor_set) {
                ++lower;
                c = var_ids[i];
                var_ids[i] = var_ids[lower];
                var_ids[lower] = c;
            }
        }
        ++lower;
        c = var_ids[lower];
        var_ids[lower] = var_ids[high];
        var_ids[high] = c;

        sort_vars(var_ids, ids, low, lower - 1);
        sort_vars(var_ids, ids, lower + 1, high);
    }
}

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
    u32 var_count = 0;

    Id *id;
    u16 *instr_size;
    u16 *opcode;
    u32 *instr;

    u16 set_count = 0;
    u16 binding_count = 0;

    u64 offset = 5;
    u64 word_count = byte_count >> 2;
    Parsed_Spirv ret;
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
        // OpEntryPoint
        case 15:
        {
            switch(instr[0]) { // switch execution model
            case 0:
            {
                ret.stage = SHADER_STAGE_VERTEX_BIT;
                break;
            }
            case 4:
            {
                ret.stage = SHADER_STAGE_FRAGMENT_BIT;
                break;
            }
            } // switch execution model
            break;
        }
        // OpVariable
        case 59:
        {
            id = ids + instr[0];
            id->result_id = instr[1];
            id->storage_type = (Storage_Type)instr[2];

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

                if (id->descriptor_set >= set_count)
                    set_count = id->descriptor_set;

                var_ids[var_count] = instr[0];
                var_count++;

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
            
            ASSERT(instr[6] < 3, "");
            switch(instr[6]) { // switch sampled
            case 2:
            {
                switch(instr[2]) { // switch dimensionality
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

    u64 layout_info_mem_size = sizeof(Descriptor_Set_Layout_Info) * set_count;
    Descriptor_Set_Layout_Info *layout_infos = (Descriptor_Set_Layout_Info*)memory_allocate_temp(layout_info_mem_size, 8);
    u64 binding_info_mem_size = sizeof(Descriptor_Set_Layout_Binding_Info) * binding_count;

    Descriptor_Set_Layout_Binding_Info* binding_infos = (Descriptor_Set_Layout_Binding_Info*)memory_allocate_temp(binding_info_mem_size, 8);
    memset(layout_infos, 0, layout_info_mem_size + binding_info_mem_size * set_count);

    // *1 note on sorting below
    sort_vars((int*)var_ids, ids, 0, var_count - 1);

    u16 layout_index = 0;
    u16 binding_index = 0;
    Id *var;
    Id *result_type;

    Descriptor_Set_Layout_Info *layout_info;
    Descriptor_Set_Layout_Binding_Info *binding_info;

    for(int i = 0; i < var_count; ++i) {
        var = ids + var_ids[i];
        if (layout_index != var->descriptor_set) {
            layout_index++;
            binding_index = 0;
        }

         layout_info = layout_infos + layout_index;
        binding_info = layout_info + binding_index;

        binding_info->binding = var->binding;
        result_type = ids + var->result_id;
        ASSERT(result_type->op_type == op_type_pointer, "vars must reference pointers");
        result_type = ids + result_type->result_id;

        if (OP_TYPE_ARRAY:
        {
            binding_info->descriptor_count = result_type->len;
            result_type = ids + result_type->result_id;
            break;
        }

        // Continue stripping back layers of references like above with the array.
        // Also add parsing for the storage type of the var. This should be done before parsing 
        // the image information, as certain storage types would mean that we immediately know the descriptor
        // type and we 'continue' here

        switch(result_type->op_type) {
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

        case OP_TYPE_ARRAY:
            ASSERT(false, "array of arrays");
        case OP_TYPE_POINTER:
            ASSERT(false, "Pointer to pointer?");

        } // switch result optype
    }

    cut_diff_temp(mark);
    return {};
}

// *1: I sort the descriptor set indices so that when i am parsing the opvars i can fill the memory block allocated 
// for the binding infos sequentially and packed e.g.:
// 
// fill memory like this, i do not need to leave gaps if sets' bindings are filled in order...
// ---->---->---->---->
// 000000000000000000000000
//
// ...Otherwise i have to do this
// ---->           ---->         ---->
// 00000000000000000000000000000000000000
// I have to allocate more space and leave big gaps to ensure that i have space for the possible number of bindings 
// in each descriptor set. If i sort them, i know that as soon as i see a novel set index, i can just continue filling
// knowing that i will not need to come back and add bindings to a previous set index
