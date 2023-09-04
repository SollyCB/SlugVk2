#include <iostream>

#include "Spv.hpp"
#include "Assert.hpp"
#include "HashMap.hpp"
#include "Array.hpp"
#include "Print.hpp"

#if TEST
#include <iostream>
#include "test/test.hpp"
#include "File.hpp"
#include "String.hpp"
#endif

namespace Sol {

static const u32 magic = 0x07230203;

namespace  {
enum class Deco : u32 {
    BLOCK = 2,
    ROW_MAJOR = 4,
    COL_MAJOR = 5,
    ARRAY_STRIDE = 6,
    MAT_STRIDE = 7,
    BUILTIN = 11,
    CONSTANT = 22,
    UNIFORM = 26,
    LOCATION = 30,
    COMPONENT = 31,
    BINDING = 33,
    DESC_SET = 34,
    OFFSET = 35,
};

static void match_deco(const u32 *instr, bool member, HashMap<u16, Spv::DecoInfo> *deco_infos) {
	u16 id = *(instr + 1);
	Spv::DecoInfo *deco_info = deco_infos->find_cpy(id);
	if (!deco_info) {
		deco_infos->insert_cpy(id, {});
		deco_info = deco_infos->find_cpy(id);
	}
    if (member) {
        ++instr;
        deco_info->member = *(instr + 1) | 0x80;
    }

    Deco deco = static_cast<Deco>(*(instr + 2));
    switch(deco) {
        case Deco::BLOCK:
        {
            deco_info->flags |= static_cast<u32>(Spv::DecoFlagBits::BLOCK);
            break;
        }
        case Deco::ROW_MAJOR:
        {
            deco_info->flags |= static_cast<u32>(Spv::DecoFlagBits::ROW_MAJOR);
            break;
        }
        case Deco::COL_MAJOR:
        {
            deco_info->flags |= static_cast<u32>(Spv::DecoFlagBits::COL_MAJOR);
            break;
        }
        case Deco::ARRAY_STRIDE:
        {
            deco_info->flags |= static_cast<u32>(Spv::DecoFlagBits::ARRAY_STRIDE);
            deco_info->array_stride = *(instr + 3);
            break;
        }
        case Deco::MAT_STRIDE:
        {
            deco_info->flags |= static_cast<u32>(Spv::DecoFlagBits::MAT_STRIDE);
            deco_info->mat_stride = *(instr + 3);
            break;
        }
        case Deco::BUILTIN:
        {
            deco_info->flags |=  static_cast<u32>(Spv::DecoFlagBits::BUILTIN);
            break;
        }
        case Deco::CONSTANT:
        {
            deco_info->flags |=  static_cast<u32>(Spv::DecoFlagBits::CONSTANT);
            break;
        }
        case Deco::UNIFORM:
        {
            deco_info->flags |= static_cast<u32>(Spv::DecoFlagBits::UNIFORM);
            break;
        }
        case Deco::LOCATION:
        {
            deco_info->flags |= static_cast<u32>(Spv::DecoFlagBits::LOCATION);
            deco_info->location = *(instr + 3);
            break;
        }
        case Deco::COMPONENT:
        {
            DEBUG_ASSERT(false, "Crashing as I want info on what this decoration means");
            deco_info->flags |= static_cast<u32>(Spv::DecoFlagBits::COMPONENT);
            break;
        }
        case Deco::BINDING:
        {
            deco_info->flags |= static_cast<u32>(Spv::DecoFlagBits::BINDING);
            deco_info->binding = *(instr + 3);
            break;
        }
        case Deco::DESC_SET:
        {
            deco_info->flags |= static_cast<u32>(Spv::DecoFlagBits::DESC_SET);
            deco_info->desc_set = *(instr + 3);
            break;
        }
        case Deco::OFFSET:
        {
            deco_info->flags |= static_cast<u32>(Spv::DecoFlagBits::OFFSET);
            deco_info->offset = *(instr + 3);
            break;
        }
        default:
            break;
    }
}
} // namespace

Spv Spv::parse(usize code_size, const u32 *spirv, bool *ok) {
    Spv ret = {};
	// wasting empty space in scratch memory doesnt matter here:
	// the plan is to parse shaders, serialize, and write the result to a file, 
	// freeing all these data structures after each parse (or more likely after a bunch of 
	// parses, writing to only one file and offsetting into it, but either way the scratch 
	// allocator is filled and freed in one "job" - parsing spv...)
	// plus by only using the linear allocator, I can easily track how large each parse is, 
	// and more easily create offsets into the files...

	ret.decorations.init(32, SCRATCH);
	ret.member_decorations.init(32, SCRATCH);
	ret.types.init(32, SCRATCH);
	ret.vars.init(32, SCRATCH);

#if DEBUG
    ret.debug.init(32, SCRATCH);
#endif

	// I have to zero, else compiler complains about constructor for some c++ reason I dont understand...
	Type type = {};
	Var var;
    u8 var_count = 0;
    ASSERT(spirv != nullptr, "do not pass nullptr to Spv::parse");
    if (spirv[0] != magic) {
        if (ok != nullptr)
            *ok = false;
        return ret;
    }

    u16 inc = 5;
    while (inc < code_size / 4) {
        const u16 *info = (u16*)(spirv + inc);
        const u32 *instr = spirv + inc;

        switch (info[0]) {
        case 15:
        {
            // @Incomplete Support other shader stages
            const u32 model = *(instr + 1);
            switch (model) {
            case 0:
                ret.stage = ShaderStage::VERTEX;
                break;
            case 4:
                ret.stage = ShaderStage::FRAGMENT;
                break;
            default:
                break;
            }
			// might compiler error
            ret.p_name = (cstr)(instr + 3);
            break;
        }
		// OpDecorate
        case 71:
        {
            match_deco(instr, false, &ret.decorations);
            break;
        }
		// OpMemberDecorate
        case 72:
        {
            match_deco(instr, true, &ret.member_decorations);
            break;
        }
        case 19:
        {
            type.name = Name::VOID;
			ret.types.insert_ptr((u16*)(instr + 1), &type);
            break;
        }
        case 20:
        {
            type.name = Name::BOOL;
			ret.types.insert_ptr((u16*)(instr + 1), &type);
            break;
        }
        case 21:
        {
            type.name = Name::INT;
            type.integer.width_sign = (*(instr + 2) | (*(instr + 3) << 7));
			ret.types.insert_ptr((u16*)(instr + 1), &type);
            break;
        }
        case 22:
        {
            type.name = Name::FLOAT;
            type.flp.width = *(instr + 2);
			ret.types.insert_ptr((u16*)(instr + 1), &type);
            break;
        }
        case 23:
        {
            
            type.name = Name::VEC;
			type.vec.type_id = *(instr + 2);
            type.vec.len = *(instr + 3);
			ret.types.insert_ptr((u16*)(instr + 1), &type);
            break;
        }
        case 24:
        {
            type.name = Name::MATRIX;
			type.matrix.type_id = *(instr + 2);
            type.matrix.column_count = *(instr + 3);
			ret.types.insert_ptr((u16*)(instr + 1), &type);
            break;
        }
        case 25:
        {
            type.name = Name::IMAGE;
			type.image.type_id = *(instr + 2);
            type.image.dim = static_cast<Image::Dim>(*(instr + 3));
            type.image.fmt = static_cast<Image::Format>(*(instr + 8));
			
            switch(*(instr + 4)) {
            case 0:
                type.image.flags |= static_cast<u8>(Image::Flags::NO_DEPTH);
                break;
            case 1:
                type.image.flags |= static_cast<u8>(Image::Flags::DEPTH);
                break;
            case 2:
                type.image.flags |= static_cast<u8>(Image::Flags::NEITHER);
                break;
            }

            if (*(instr + 5))
                type.image.flags |= static_cast<u8>(Image::Flags::ARRAYED);
            if (*(instr + 6))
                type.image.flags |= static_cast<u8>(Image::Flags::MULTI_SAMPLED);
            if (*(instr + 7))
                type.image.flags |= static_cast<u8>(Image::Flags::SAMPLED);

			ret.types.insert_ptr((u16*)(instr + 1), &type);
            break;
        }
        case 26:
        {
            type.name = Name::SAMPLER;
			ret.types.insert_ptr((u16*)(instr + 1), &type);
            break;
        }
        case 27:
        {
            type.name = Name::SAMPLED_IMAGE;
			type.sampled_image.type_id = *(instr + 2);
			ret.types.insert_ptr((u16*)(instr + 1), &type);
            break;
        }
        case 28:
        {
            type.name = Name::ARRAY;
			type.arr.type_id = *(instr + 2);
			type.arr.len = *(instr + 3);
			ret.types.insert_ptr((u16*)(instr + 1), &type);
            break;
        }
        case 29:
        {
            type.name = Name::RUNTIME_ARRAY;
			type.runtime_arr.type_id = *(instr + 2);
			ret.types.insert_ptr((u16*)(instr + 1), &type);
            break;
        }
        case 30:
        {
			type = {};
            type.structure.member_ids.init(info[1] - 2);
            for(int i = 0; i < info[1] - 2; ++i)
				type.structure.member_ids.append_cpy(static_cast<u16>(*(instr + 2 + i)));

            type.name = Name::STRUCT;
			ret.types.insert_ptr((u16*)(instr + 1), &type);
            break;
        }
        case 31:
        {
            type.name = Name::OPAQUE;
			ret.types.insert_ptr((u16*)(instr + 1), &type);
            break;
        }
        case 32:
        {
            type.name = Name::PTR;
			type.ptr.type_id = *(instr + 3);
			ret.types.insert_ptr((u16*)(instr + 1), &type);
        }
        case 39:
        {
			// Do not need to handle forward pointers, as any type referencing one would 
			// search in the HashMap and find the pointer that the forward pointer points to
            break;
        }
        case 59:
        {
			var.type_id = *(instr + 1);
			var.storage = static_cast<Storage>(*(instr + 3));
			ret.vars.insert_ptr((u16*)(instr + 2), &var);
            break;
        }

#if DEBUG
        // OpName
        case 5:
        {
            StringBuffer name = StringBuffer::get((const char*)(instr + 2));
			ret.debug.insert_ptr((u16*)(instr + 1), &name);

			//std::cout << "Id: " << *(instr + 1) << ", Name: " << name.as_cstr() << '\n';

            break;
        }
#endif
        }

        inc += info[1];
    }

	auto var_iter = ret.vars.iter();
	auto *next = var_iter.next();
	while(next) {
		// Replace Var's reference pointer id with the id of the type pointed to
		// (just to remove a level of indirection...)
		next->value.type_id = ret.types.find_cpy(next->value.type_id)->ptr.type_id;
		next = var_iter.next();
	}

    if (ok) *ok = true;
    return ret;
}

void Spv::kill() {
    SCRATCH->cut_diff(scratch_mark);
}

#if TEST
void test_ubo(Spv spv) {
	u16 ubo_id = 19;
	Spv::Var *ubo = spv.vars.find_cpy(ubo_id);
	TEST_EQ("ubo_type_eq_UBO", ubo->type_id, (u16)17, false);
	Spv::Type *UBO = spv.types.find_cpy(ubo->type_id);
	TEST_EQ("UBO_eq_structure",(u8)UBO->name, (u8)Spv::Name::STRUCT, false);

	for(int i = 0; i < UBO->structure.member_ids.len; ++i) {
		Spv::Type *member_type = spv.types.find_cpy(UBO->structure.member_ids[i]);
		TEST_EQ("UBO_members_type", 
				(u16)member_type->name, (u16)Spv::Name::MATRIX, false);
		TEST_EQ("UBO_members_col_count", 
				(int)member_type->matrix.column_count, 4, false);

		Spv::Type *vec = spv.types.find_cpy(member_type->matrix.type_id);
		TEST_EQ("UBO_members_vec_type", 
				(u16)vec->name, (u16)Spv::Name::VEC, false);
		TEST_EQ("UBO_members_vec_type", 
				(int)vec->vec.len, 4, false);

		Spv::Type *vec_float = spv.types.find_cpy(vec->vec.type_id);
		TEST_EQ("UBO_members_vec_float", 
				(u16)vec_float->name, (u16)Spv::Name::FLOAT, false);
		TEST_EQ("UBO_members_vec_float", 
				(int)vec_float->flp.width, 32, false);
	}

	Spv::DecoInfo ubo_decos = *spv.decorations.find_cpy(ubo_id);
	TEST_EQ("ubo_decorations_desc_set_flag", 
			(u32)ubo_decos.flags & (u32)Spv::DecoFlagBits::DESC_SET,
		   	(u32)Spv::DecoFlagBits::DESC_SET, false);
	TEST_EQ("ubo_decorations_binding_flag", 
			(u32)ubo_decos.flags & (u32)Spv::DecoFlagBits::BINDING,
		   	(u32)Spv::DecoFlagBits::BINDING, false);
	TEST_EQ("ubo_decorations_desc_set_value", 
			(int)ubo_decos.desc_set, 0, false);
	TEST_EQ("ubo_decorations_binding_value", 
			(int)ubo_decos.binding, 0, false);
}
void test_pos(Spv spv) {
	u16 pos_id = 33;
	Spv::Var *pos = spv.vars.find_cpy(pos_id);
	Spv::DecoInfo *pos_deco = spv.decorations.find_cpy(pos_id);
	TEST_EQ("pos_deco_flag", 
			(u32)pos_deco->flags & (u32)Spv::DecoFlagBits::LOCATION,
			(u32)Spv::DecoFlagBits::LOCATION, false);
	TEST_EQ("pos_location_value", (int)pos_deco->location, 0, false);

	Spv::Type *v2 = spv.types.find_cpy(pos->type_id);
	TEST_EQ("pos_type_vec", (u32)v2->name, (u32)Spv::Name::VEC, false);
	TEST_EQ("pos_type_vec_len", (int)v2->vec.len, 2, false);

	Spv::Type *v2_float = spv.types.find_cpy(v2->vec.type_id);
	TEST_EQ("v2_is_float", (u32)v2_float->name, (u32)Spv::Name::FLOAT, false);
}
void test_color(Spv spv) {
	u16 col_id = 47;
	Spv::Var *col = spv.vars.find_cpy(col_id);
	Spv::DecoInfo *col_deco = spv.decorations.find_cpy(col_id);
	TEST_EQ("col_deco_flag", 
			(u32)col_deco->flags & (u32)Spv::DecoFlagBits::LOCATION,
			(u32)Spv::DecoFlagBits::LOCATION, false);
	TEST_EQ("col_location_value", (int)col_deco->location, 1, false);

	Spv::Type *v3 = spv.types.find_cpy(col->type_id);
	TEST_EQ("col_type_vec", (u32)v3->name, (u32)Spv::Name::VEC, false);
	TEST_EQ("col_type_vec_len", (int)v3->vec.len, 3, false);

	Spv::Type *v3_float = spv.types.find_cpy(v3->vec.type_id);
	TEST_EQ("v3_is_float", (u32)v3_float->name, (u32)Spv::Name::FLOAT, false);
}
void test_sampler(Spv spv) {
	u16 tex_id = 52;
	Spv::DecoInfo tex_decos = *spv.decorations.find_cpy(tex_id);
	TEST_EQ("tex_decorations_desc_set_flag", 
			(u32)tex_decos.flags & (u32)Spv::DecoFlagBits::DESC_SET,
		   	(u32)Spv::DecoFlagBits::DESC_SET, false);
	TEST_EQ("tex_decorations_binding_flag", 
			(u32)tex_decos.flags & (u32)Spv::DecoFlagBits::BINDING,
		   	(u32)Spv::DecoFlagBits::BINDING, false);
	TEST_EQ("tex_decorations_desc_set_value", 
			(int)tex_decos.desc_set, 1, false);
	TEST_EQ("tex_decorations_binding_value", 
			(int)tex_decos.binding, 1, false);
}
void Spv::run_tests() {
    TEST_MODULE_BEGIN("SpvTestMod1", true, false);

    usize code_size;
    const u32 *spirv = File::read_spv(&code_size, "test/test_spirv.vert.spv", SCRATCH);

	DEBUG_ASSERT(spirv != nullptr, "read_spv failed");

    bool ok;
    Spv spv = Spv::parse(code_size, spirv, &ok);
	TEST_EQ("ParseOk", ok, true, false);
    TEST_EQ("Stage", static_cast<int>(spv.stage), 0x01, false);

	// Begin test functions
    test_ubo(spv);
	test_pos(spv);
	test_color(spv);
	test_sampler(spv);

	// End
    spv.kill();
    TEST_MODULE_END();
}
#endif

} // namespace Sol
