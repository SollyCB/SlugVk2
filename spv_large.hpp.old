#pragma once

#include "Basic.hpp"

namespace Sol {

    // TODO: New rewrite with serialization in mind: 
    //      Types stay the same, except for the ones which store pointers, these will 
    //      instead store byte offsets into a buffer where each type till be copied. 
    //      The more basic the type, the earlier in the buffer that it appears.

/*
    TODO:(Sol): These structs are all probably poorly laid out. They can definitely be better 
    packed and aligned. But this section is not very performance critical. Eventually I want to 
    prebake and package all the state available here. Parsing shaders at runtime is blegh
*/

struct Spv {
	enum class ShaderStage : u8 {
		VERTEX   = 0x01,
		FRAGMENT = 0x10,
	};
    enum class Storage : u8 {
        UNIFORM_CONSTANT = 0,
        INPUT = 1,
        UNIFORM = 2,
        OUTPUT = 3,
        PUSH_CONSTANT = 9,
        IMAGE = 11,
        STORAGE_BUFFER = 12,
    };
    enum class Name : u8 {
        VOID = 19,
        BOOL = 20,
        INT = 21,
        FLOAT = 22,
        VEC = 23,
        MATRIX = 24,
        IMAGE = 25,
        SAMPLER = 26,
        SAMPLED_IMAGE = 27,
        ARRAY = 28,
        RUNTIME_ARRAY = 29,
        STRUCT = 30,
        OPAQUE = 31,
        PTR = 32,
		FWD_PTR = 39,
        VAR = 59,
    };
    enum class DecoFlagBits : u32 {
        BLOCK = 0x0001,
        ROW_MAJOR = 0x0002,
        COL_MAJOR = 0x0004,
        ARRAY_STRIDE = 0x0008,
        MAT_STRIDE = 0x0010,
        BUILTIN = 0x0020,
        CONSTANT = 0x0040,
        UNIFORM = 0x0080,
        LOCATION = 0x0100,
        COMPONENT = 0x0200,
        BINDING = 0x0400,
        DESC_SET = 0x0800,
        OFFSET = 0x1000,
    };
    struct DecoInfo {
        u32 flags = 0x0;
        u16 array_stride;
        u16 mat_stride;
        u16 offset;
        u8 desc_set;
        u8 component;
        u8 location;
        u8 binding; 
        u8 member;
        // member bit layout:
        //      Top bit: bool (is this OpMemberDecorate or OpDecorate), 
        //      Other 7: member index
    };
    struct Int {
        u8 width_sign;
    };
    struct Float {
        u8 width;
    };
    struct Deco {
        u16 id;
        DecoInfo deco_info; 
    };
    struct Vector {
		u16 type_id;
        u8 len;
    };
    struct Matrix {
        u16 type_id;
        u8 column_count;
    };
    struct Arr {
        u16 type_id;
        u8 len;
    };
    struct RuntimeArr {
        u16 type_id;
    };
    struct Struct {
        Array<u16> member_ids;
    };
    struct Ptr {
        u16 type_id;
    };
    struct Image {
        enum Dim : u8 {
            D1 = 0,
            D2 = 1,
            D3 = 2,
            CUBE = 3,
            RECT = 4,
            BUFFER = 5,
            SUBPASS_DATA = 6,
        };
        enum class Flags : u8 {
			NO_DEPTH = 0x01,
            DEPTH = 0x02,
            NEITHER = 0x04,
            RUN_TIME = 0x08,
            SAMPLED = 0x10,
            READ_WRITE = 0x20,
            ARRAYED  = 0x40,
            MULTI_SAMPLED = 0x80,
        };
        enum Format : u8 {
            Unknown = 0,
            Rgba32f = 1,
            Rgba16f = 2,
            R32f = 3,
            Rgba8 = 4,
            Rgba8Snorm = 5,
            Rg32f = 6,
            Rg16f = 7,
            R11fG11fB10f = 8,
            R16f = 9,
            Rgba16 = 10,
            Rgb10A2 = 11,
            Rg16 = 12,
            Rg8 = 13,
            R16 = 14,
            R8 = 15,
            Rgba16Snorm = 16,
            Rg16Snorm = 17,
            Rg8Snorm = 18,
            R16Snorm = 19,
            R8Snorm = 20,
            Rgba32i = 21,
            Rgba16i = 22,
            Rgba8i = 23,
            R32i = 24,
            Rg32i = 25,
            Rg16i = 26,
            Rg8i = 27,
            R16i = 28,
            R8i = 29,
            Rgba32ui = 30,
            Rgba16ui = 31,
            Rgba8ui = 32,
            R32ui = 33,
            Rgb10a2ui = 34,
            Rg32ui = 35,
            Rg16ui = 36,
            Rg8ui = 37,
            R16ui = 38,
            R8ui = 39,
            R64ui = 40,
            R64i = 41,
        };
        Dim dim;
        Format fmt;
		u16 type_id;
        u8 flags;
    };
    struct SampledImage {
        u16 type_id;
    };
    struct Type {
        Name name;
        union {
            Int integer;
            Float flp;
            Vector vec;
            Matrix matrix;
            Image image;
            SampledImage sampled_image;
            Arr arr;
            RuntimeArr runtime_arr;
            Struct structure;
            Ptr ptr;
        };
    };
	struct Var {
		Storage storage;
		u16 type_id;
	};
    cstr p_name;
    ShaderStage stage;
	HashMap<u16, Var> vars;
	HashMap<u16, Type> types;
	HashMap<u16, DecoInfo> decorations;
	HashMap<u16, DecoInfo> member_decorations;

    static Spv parse(usize code_size, const u32 *spirv, bool *ok);

	struct Serialized {
		// Heavily simplify Spv:
		//	report only the information necessary for creating the 
		//	Pipeline layout / desc buffers, e.g. memory type, is it arrayed...
	};
	// Store the info in manner better facilitating file io, store data size for offsets
	Serialized serialize();
	static void write(cstr filename, u32 count, Serialized *infos);

	// @Dangerous @PoorName
	// This function should really never be called, as it will be exclusively 
	// used in pipeline state creation, and that can do the scratch deallocation 
	void kill();

private:
    size_t scratch_mark = 0;

#if DEBUG
public:
    HashMap<u16, StringBuffer> debug;
#endif

#if TEST
    static void run_tests();
#endif
};


} // namespace Sol
