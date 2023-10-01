#include "spirv.hpp"

#if TEST
#include "file.hpp"
#include "test.hpp"
#endif

enum Decoration {
    SPEC_ID = 1,
    BLOCK = 2,
    BUFFER_BLOCK = 3,
    BUILTIN = 11,
    UNIFORM = 26,
    LOCATION = 30,
    BINDING = 33,
    DESCRIPTOR_SET = 34,
};
enum Decoration_Flags {
    SPEC_ID_BIT = 0x01,
    BLOCK_BIT = 0x02,
    BUFFER_BLOCK_BIT = 0x04,
    BUILTIN_BIT = 0x08,
    UNIFORM_BIT = 0x10,
    LOCATION_BIT = 0x20,
    BINDING_BIT = 0x40,
    DESCRIPTOR_SET_BIT = 0x80,
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
    IMAGE_SAMPLED,
    IMAGE_STORAGE,
    TEXEL_UNIFORM,
    TEXEL_STORAGE,
    IMAGE_INPUT,
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
    u16 binding;
    u16 descriptor_set;
    //u16 location; i dont need this currently i dont think

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
    memset(ids, 0, sizeof(Id) * id_bound);

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
            case 1:
            {
                ret.stage = SHADER_STAGE_TESSELLATION_CONTROL_BIT;
                break;
            }
            case 2:
            {
                ret.stage = SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
                break;
            }
            case 3:
            {
                ret.stage = SHADER_STAGE_GEOMETRY_BIT;
                break;
            }
            case 4:
            {
                ret.stage = SHADER_STAGE_FRAGMENT_BIT;
                break;
            }
            case 5:
            {
                ret.stage = SHADER_STAGE_COMPUTE_BIT;
                break;
            }
            } // switch execution model
            break;
        }
        // OpVariable
        case 59:
        {
            id = ids + instr[1];
            id->result_id = instr[0];
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
            case BUFFER_BLOCK:
            {
                id->decoration_flags |= (u8)BUFFER_BLOCK_BIT;
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
                //id->location = (u16)instr[2];
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
                    set_count = id->descriptor_set + 1;

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
            // translates to combined image sampler
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
        // OpConstant
        case 43:
        {
            ids[instr[1]].length = instr[2];
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

    ret.layout_infos = (Descriptor_Set_Layout_Info*)memory_allocate_temp(sizeof(Descriptor_Set_Layout_Info) * set_count, 8);

    Descriptor_Set_Layout_Binding_Info* binding_infos = (Descriptor_Set_Layout_Binding_Info*)memory_allocate_temp(sizeof(Descriptor_Set_Layout_Binding_Info) * binding_count, 8);

    // *1 note on sorting below
    sort_vars((int*)var_ids, ids, 0, var_count - 1);

    u16 binding_index = 0;
    u16 previous_binding_index = 0;
    Descriptor_Set_Layout_Binding_Info *binding_info = &binding_infos[binding_index];

    u16 layout_index = 0;
    ret.layout_infos[layout_index].binding_infos = binding_info;

    // @Todo redo this shit. This is pretty ugly code actually (specifically the group/index counting...

    Id *var;
    Id *result_type;
    u32 hack_count = 0;
    u32 zero_group = 0;
    for(int i = 0; i < var_count; ++i) {
        zero_groups = 1;

        var = ids + var_ids[i];
        binding_info = &binding_infos[binding_index];

        if (layout_index != var->descriptor_set) {
            ret.layout_infos[layout_index].binding_count = binding_index - previous_binding_index;

            layout_index++;
            ret.layout_infos[layout_index].binding_infos = binding_info;

            previous_binding_index = binding_index;
        }

        binding_info->binding = var->binding;
        result_type = ids + var->result_id;
        ASSERT(result_type->op_type == OP_TYPE_POINTER, "vars must reference pointers");
        result_type = ids + result_type->result_id;

        // @HeightMaps Assumes vertex shaders do not access samplers (more info below)
        // Assumes fragma shaders do not access buffers
        binding_info->access_flags = SHADER_STAGE_VERTEX_BIT;

        if (result_type->op_type == OP_TYPE_ARRAY) {
            binding_info->descriptor_count = ids[result_type->length].length;
            result_type = ids + result_type->result_id;
        } else {
            binding_info->descriptor_count = 1;
        }

        if (result_type->decoration_flags & BLOCK_BIT) {
            if (var->storage_type == STORAGE_STORAGE_BUFFER)
                binding_info->descriptor_type = DESCRIPTOR_TYPE_STORAGE_BUFFER;
            else 
                binding_info->descriptor_type = DESCRIPTOR_TYPE_UNIFORM_BUFFER;

            goto increment_binding_index;
        }
        if (result_type->decoration_flags & BUFFER_BLOCK_BIT) {
            binding_info->descriptor_type = DESCRIPTOR_TYPE_STORAGE_BUFFER;
            goto increment_binding_index;
        }

    begin_switch: // goto label

        // resource must be an image, so assume that the access stage is the fragment shader.
        // this needs refining: I need a way to change or override this behaviour for shit like 
        // height maps and the like
        binding_info->access_flags = SHADER_STAGE_FRAGMENT_BIT;

        switch(result_type->op_type) {
        case OP_TYPE_POINTER:
        {
            ASSERT(false, "pointer to pointer? array of pointers?");
            result_type = &ids[result_type->result_id];
            goto begin_switch;
        }
        case OP_TYPE_ARRAY:
        {
            ASSERT(false, "array to array?");
            result_type = &ids[result_type->result_id];
            goto begin_switch;
        }
        case OP_TYPE_SAMPLER:
        {
            binding_info->descriptor_type = DESCRIPTOR_TYPE_SAMPLER;
            break;
        }
        case OP_TYPE_SAMPLED_IMAGE:
        {
            binding_info->descriptor_type = DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            break;
        }
        // It is annoying that image layout types are so opaque up front
        case OP_TYPE_IMAGE:
        {
            switch(result_type->image_type) {
            case UNKNOWN:
            {
                ASSERT(false, "Unusable Image Descriptor");
            }
            case IMAGE_SAMPLED:
            {
                binding_info->descriptor_type = DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                break;
            }
            case IMAGE_STORAGE:
            {
                binding_info->descriptor_type = DESCRIPTOR_TYPE_STORAGE_IMAGE;
                break;
            }
            case TEXEL_UNIFORM:
            {
                binding_info->descriptor_type = DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                break;
            }
            case TEXEL_STORAGE:
            {
                binding_info->descriptor_type = DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                break;
            }
            case IMAGE_INPUT:
            {
                binding_info->descriptor_type = DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
                break;
            }
            } // switch image type
            break;
        }
        } // switch result optype

    increment_binding_index: // goto label
        binding_index++;
    }
    // The last layout info in the list will not have its binding count set
    ret.layout_infos[layout_index].binding_count = binding_index - previous_binding_index;

    // @Hack This is a hack, but a slightly less ugly one than the 'if (..)' that came before it..
    ret.group_count = layout_index + zero_groups;

    cut_diff_temp(mark);
    return ret;
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

#if TEST 
void test_spirv() {
    BEGIN_TEST_MODULE("Spirv", true, false);
    u64 byte_count;
    const u32 *spirv = (const u32*)file_read_bin_temp("spirv_2.vert.spv", &byte_count);
    Parsed_Spirv p = parse_spirv(byte_count, spirv);

    Descriptor_Set_Layout_Info info;

    info = p.layout_infos[0];
    for(int j = 0; j < info.binding_count; ++j) {
        Descriptor_Set_Layout_Binding_Info bind = info.binding_infos[j];
        int count = bind.descriptor_count;
        int type = bind.descriptor_type;
        switch(bind.binding) {
        case 0:
        {
            TEST_EQ("Set_0_Binding_0", count, 2, false);
            TEST_EQ("Set_0_Binding_0", type, 6, false);
            break;
        }
        case 1:
        {
            TEST_EQ("Set_0_Binding_1", count, 4, false);
            TEST_EQ("Set_0_Binding_1", type, 6, false);
            break;
        }
        }
    }
    info = p.layout_infos[1];
    for(int j = 0; j < info.binding_count; ++j) {
        Descriptor_Set_Layout_Binding_Info bind = info.binding_infos[j];
        int count = bind.descriptor_count;
        int type = bind.descriptor_type;
        switch(bind.binding) {
        case 0:
        {
            TEST_EQ("Set_1_Binding_0", count, 3, false);
            TEST_EQ("Set_1_Binding_0", type, 6, false);
            break;
        }
        case 1:
        {
            TEST_EQ("Set_1_Binding_1", count, 1, false);
            TEST_EQ("Set_1_Binding_1", type, 6, false);
            break;
        }
        }
    }

    info = p.layout_infos[2];
    for(int j = 0; j < info.binding_count; ++j) {
        Descriptor_Set_Layout_Binding_Info bind = info.binding_infos[j];
        int count = bind.descriptor_count;
        int type = bind.descriptor_type;
        switch(bind.binding) {
        case 0:
        {
            TEST_EQ("Set_2_Binding_0", count, 7, false);
            TEST_EQ("Set_2_Binding_0", type, 6, false);
            break;
        }
        case 1:
        {
            TEST_EQ("Set_2_Binding_1", count, 1, false);
            TEST_EQ("Set_2_Binding_1", type, 6, false);
            break;
        }
        case 2:
        {
            TEST_EQ("Set_2_Binding_2", count, 2, false);
            TEST_EQ("Set_2_Binding_2", type, 6, false);
            break;
        }
        }
    }

    info = p.layout_infos[3];
    for(int j = 0; j < info.binding_count; ++j) {
        Descriptor_Set_Layout_Binding_Info bind = info.binding_infos[j];
        int count = bind.descriptor_count;
        int type = bind.descriptor_type;
        switch(bind.binding) {
        case 0:
        {
            TEST_EQ("Set_3_Binding_0", count, 4, false);
            TEST_EQ("Set_3_Binding_0", type, 1, false);
            break;
        }
        case 1:
        {
            TEST_EQ("Set_3_Binding_1", count, 3, false);
            TEST_EQ("Set_3_Binding_1", type, 1, false);
            break;
        }
        case 2:
        {
            TEST_EQ("Set_3_Binding_2", count, 2, false);
            TEST_EQ("Set_3_Binding_2", type, 1, false);
            break;
        }
        case 3:
        {
            TEST_EQ("Set_3_Binding_3", count, 1, false);
            TEST_EQ("Set_3_Binding_3", type, 1, false);
            break;
        }
        }
    }

    END_TEST_MODULE();
}
#endif // #if TEST
