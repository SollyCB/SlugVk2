#include "spirv.hpp"

// For shader resource interface, meaningful storage classes are:
//    Uniform, UniformConstant, StorageBuffer, or PushConstant.
//
// When considering only descriptor types, omit the PushConstant storage class.
//
// Sampler =              UniformConstant      , OpTypeSampler                               , No Decoration
// SampledImage =         UniformConstant      , OpTypeImage (sampled = 1)                   , No Decoration
// StorageImage =         UniformConstant      , OpTypeImage (sampled = 2)                   , No Decoration
// CombinedImageSampler = UniformConstant      , OpTypeSampledImage                          , No Decoration
// UniformTexelBuffer   = UniformConstant      , OpTypeImage (Dim = Buffer, Sampled = 1)     , No Decoration
// StorageTexelBuffer   = UniformConstant      , OpTypeImage (Dim = Buffer, Sampled = 2)     , No Decoration
// UniformBuffer        = Uniform              , OpTypeStruct                                , Block + Offset
// StorageBuffer        = Uniform/StorageBuffer, OpTypeStruct                                , Block/BufferBlock + Offset
// InputAttachment      = UniformConstant      , OpTypeImage (Dim = SubpassData, Sampled = 2), InputAttachmentIndex
// InlineUniformBlock   = UniformConstant      , OpTypeStruct <-- (confusing because it is identical to uniform buffer, I think the spirv spec it has decorations)

// u32s to prevent compiler warnings when assigning values by derefing spirv
struct Spirv_Type {
    u32 id;
    u32 result_id;
    u32 binding;
    u32 descriptor_set;
    u32 descriptor_count;
    VkDescriptorType descriptor_type;

    //u32 input_attachment_index; <- I dont think that this is actually useful...?
};

inline static Spirv_Type* spirv_find_type(Spirv_Type *types, int *type_count, int id) {
    // If there is an element in the array with a matching id, return it. 
    // Else, return the next empty element in the array.
    for(int i = 0; i < *type_count; ++i) {
        if (types[i].id == id)
            return &types[id];
    }
    Spirv_Type *type = &types[*type_count];
    type->id = id;
    type->descriptor_set = Max_u32; // to make finding types actually decorated with descriptor set easier...
    type->descriptor_count = Max_u32; // to make finding arrayed types easier...
    type->descriptor_type = (VkDescriptorType)Max_u32;
    type_count++;
    return type;
}

inline static Spirv_Type* spirv_find_descriptor(Spirv_Type *types, int type_count) {
    for(int i = 0; i < type_count; ++i) {
        if (types[i].descriptor_set != Max_u32)
            return &types[i];
    }
    return NULL;
}

VkDescriptorSetLayoutBinding* parse_spirv(u64 byte_count, const u32 *spirv, int *set_count) {
    if (spirv[0] != 0x07230203) { // Magic number equivalency
        println("Cannot parse spirv! (Incorrect Magic Number)");
        return NULL;
    }

    Spirv_Type types[64]; // Assume fewer that 64 type declarations
    int type_count = 0;

    u64 descriptor_mask = 0x0; // Assume fewer than 64 descriptor sets
    int descriptor_count = 0;

    byte_count >>= 2; // convert to word count
    u64 offset = 5; // point to first word in instruction stream
    u16 *instruction_size;
    u16 *op_code;
    u32 *instruction;
    Spirv_Type *type;
    VkShaderStageFlagBits stage;
    while(offset < byte_count) {
        op_code = (u16*)(spirv + offset);
        instruction_size = op_code + 1;
        instruction = (u32*)(instruction_size + 1);
        switch(op_code) {
        case 15: // OpEntryPoint
        {
            switch(instruction[0]) {
            case 0:
                stage = VK_SHADER_STAGE_VERTEX_BIT;
                break;
            case 1:
                stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
                break;
            case 2:
                stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
                break;
            case 3:
                stage = VK_SHADER_STAGE_GEOMETRY_BIT;
                break;
            case 4:
                stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                break;
            case 5:
                stage = VK_SHADER_STAGE_COMPUTE_BIT;
                break;
            }
            break;
        }
        case 71: // OpDecorate
        {
            switch(instruction[1]) { // switch on Decoration
            case 3: // BufferBlock
                type = spirv_find_type(types, &type_count, instruction[0]);
                type->descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                break;
            case 33: // Binding
                type = spirv_find_type(types, &type_count, instruction[0]);
                type->binding = instruction[2];
                break;
            case 34: // DescriptorSet
                type = spirv_find_type(types, &type_count, instruction[0]);
                type->descriptor_set = instruction[2];

                descriptor_count++;
                descriptor_mask |= 1 << instruction[2]; // register the potentially new descriptor set

                break;
            case 43: // InputAttachmentIndex
            {
                type = spirv_find_type(types, &type_count, instruction[0]);
                type->descriptor_type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
                //type->input_attachment_index = instruction[2]; <-  I dont think that this is actually useful
                break;
            }
            default:
                break;
            }
            break;
        }
        case 59: // OpVariable
        {
            type = spirv_find_type(types, &type_count, instruction[1]);
            type->type_id = instruction[0];
            switch(instruction[2]) { // switch on StorageClass
            /*case 9: // PushConstant
            {
                type->descriptor_type = SPIRV_SHADER_INTERFACE_PUSH_CONSTANT;
                break;
            }*/
            case 12:
            {
                type->descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // There can be two ways to define a storage buffer depending on the spec version...
                break;
            }
            default:
                break;
            }
        }
        case 32: // OpTypePointer
        {
            type = spirv_find_type(types, &type_count, instruction[0]);
            type->type_id = instruction[2]; // just connect the id chain to what it points to
            break;
        }
        case 28: // OpTypeArray
        {
            type = spirv_find_type(types, &type_count, instruction[0]);
            type->descriptor_count = instruction[2];
            type->type_id = instruction[1];
            break;
        }
        case 27: // OpTypeSampledImage
        {
            type = spirv_find_type(types, &type_count, instruction[0]);
            type->descriptor_type = Vk_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            break;
        }
        case 26: // OpTypeSampler
        {
            type = spirv_find_type(types, &type_count, instruction[0]);
            type->descriptor_type = Vk_DESCRIPTOR_TYPE_SAMPLER;
            break;
        }
        case 25: // OpTypeImage
        {
            type = spirv_find_type(types, &type_count, instruction[0]);
            switch(instruction[6]) { // switch Sampled
            case 1: // read only
            {
                switch(instruction[2]) { // switch Dim
                case 5: // Buffer
                    type->descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                    break;
                case 1: // 2D
                    type->descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                    break;
                }
                break;
            }
            case 2: // read + write
            {
                switch(instruction[2]) { // switch Dim
                case 5: // Buffer
                    type->descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                    break;
                case 1: // 2D
                    type->descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    break;
                }
                break;
            }
            default:
                break;
            } // switch Sampled
            break;
        }
        default:
            break;
        }
        offset += instruction_size;
    }

    VkDescriptorSetLayoutBinding *bindings =
        (VkDescriptorSetLayoutBinding*)memory_allocate_temp(sizeof(VkDescriptorSetLayoutBinding) * descriptor_count, 8);

    //
    // @TODO Current task. Probably need to sort bindings by their sets to make stuff clean. But I actually think there
    // is a simpler way, but it just does not jump to mind when I am so sleepy. Love u muah. GN
    //
    int descripot
    VkDescriptorSetLayout *descriptor_sets = (VkDescriptorSetLayout*)memory_allocate_temp(sizeof(VkDescriptorSetLayout) * (64 - pop

    // Here we just search the type list for the next descriptor in order to deal with its info.
    // Anything more complex, (like keeping a specific list just for descriptors and then looping that)
    // would be pointless because the list is so short...
    Spirv_Type *result_type;
    for(int i = 0; i < descriptor_count; ++i) {
        type = spirv_find_descriptor(types, type_count);
        type->result_type = spirv_find_type(types, &type_count, type->result_id)->result_id; // type has to be OpVariable, so deref its pointer to get the real type
        result_type = spirv_find_type(types, &type_count, type->result_id);

        bindings[i].stageFlags = (VkShaderStageFlags)stage;
        bindings[i].binding = type->binding;

        if (result_type->descriptor_count != Max_u32) {
            bindings[i].descriptorCount = result_type->descriptor_count;
            result_type = spirv_find_type(types, &type_count, result_type->result_id); // deref array to its element type
        }
        else {
            bindings[i].descriptorCount = 1;
        }

        if ((u32)type->descriptor_type != Max_u32)
            bindings[i].descriptorType = type->descriptor_type;
        else
            bindings[i].descriptorType = (VkDescriptorType)result_type.shader_interface;
    }

    return bindings;
}
