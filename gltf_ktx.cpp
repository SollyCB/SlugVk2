#include "gltf_ktx.hpp"
#include "file.hpp"

u8 KTX_IDENT_LENGTH = 12;
static u8 KTX_IDENT_VER_2[] = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

static u8 KTX_IDENT_VER_1[] = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

static Ktx parse_ktx1(const u8 *ktx_data);
static Ktx parse_ktx2(const u8 *ktx_data);

Ktx load_ktx(const char *file_name) {
    u64 file_size;
    const u8 *file_contents = file_read_char_heap(file_name, &file_size);

    if (memcmp(file_contents, KTX_IDENT_VER_2, KTX_IDENT_LENGTH) == 0) {
        println("Ktx 2.0 image file found");
        return parse_ktx2(file_contents);
    }
    if (memcmp(file_contents, KTX_IDENT_VER_1, KTX_IDENT_LENGTH) == 0) {
        println("Ktx 1.0 image file found");
        ASSERT(false, "Piss off, why do I have to make an api call to OpenGl to know what the \
                       gl..Format value is??? Just list the enum values in the spec (which is awful). Holy \
                       shit vulkan spec is sooooo much better!!");
                    
        return parse_ktx1(file_contents);
    }

    println("This is not a valid ktx file");
    return {};
}

static Ktx parse_ktx1(const u8 *ktx_data) {
    Ktx ktx;
    return ktx;
}
static Ktx parse_ktx2(const u8 *ktx_data) {
    Ktx ktx;
    return ktx;
}
