#include "file.hpp"

const u8* file_read_bin_temp_large(const char *file_name, u64 size) {
    FILE *file = fopen(file_name, "rb");
    ASSERT(file, "Could Not Open File");
    if (!file) {
        println("Failed to read file %c", file_name);
        return NULL;
    }
    u8 *ret = memory_allocate_temp(size, 8);
    size = fread(ret, 1, size, file);
    fclose(file);
    return ret;
}

const u8* file_read_bin_temp(const char *file_name, u64 *size) {
    FILE *file = fopen(file_name, "rb");

    if (!file) {
        println("FAILED TO READ FILE %c", file_name);
        return nullptr;
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    void *contents = memory_allocate_temp(*size, 8); // 8 byte aligned as contents of file may need to be aligned

    // idk if it is worth checking the returned size? It seems to be wrong on windows
    //fread(contents, 1, *size, file);
    size_t read = fread(contents, 1, *size, file);

    if (*size != read)
        println("Failed to read entire file");

    //ASSERT(read == *byte_count, "Failed to read entire file: read = %i, file_len = %i", read, *byte_count);
    fclose(file);

    return (u8*)contents;
}

const u8* file_read_bin_heap(const char *file_name, u64 *size) {
    FILE *file = fopen(file_name, "rb");

    if (!file) {
        println("FAILED TO READ FILE %c", file_name);
        return nullptr;
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    void *contents = memory_allocate_heap(*size, 8); // 8 byte aligned as contents of file may need to be aligned

    // idk if it is worth checking the returned size? It seems to be wrong on windows
    //fread(contents, 1, *size, file);
    size_t read = fread(contents, 1, *size, file);

    if (*size != read)
        println("Failed to read entire file, %c", file_name);

    //ASSERT(read == *byte_count, "Failed to read entire file: read = %i, file_len = %i", read, *byte_count);
    fclose(file);

    return (u8*)contents;
}
const u8* file_read_char_temp(const char *file_name, u64 *size) {
    FILE *file = fopen(file_name, "r");

    if (!file) {
        println("Failed to read file %c", file_name);
        return nullptr;
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    void *contents = memory_allocate_temp(*size, 8); // 8 byte aligned as contents of file may need to be aligned

    // idk if it is worth checking the returned size? It seems to be wrong on windows
    //fread(contents, 1, *size, file);
    size_t read = fread(contents, 1, *size, file);

    if (*size != read) {
        println("Failed to read entire file, %c", file_name);
        println("    File Size: %u, Size Read: %u", *size, read);
    }

    //ASSERT(read == *byte_count, "Failed to read entire file: read = %i, file_len = %i", read, *byte_count);
    fclose(file);

    return (u8*)contents;
}

const u8* file_read_char_heap(const char *file_name, u64 *size) {
    FILE *file = fopen(file_name, "r");

    if (!file) {
        println("FAILED TO READ FILE %c", file_name);
        return nullptr;
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    void *contents = memory_allocate_heap(*size, 8); // 8 byte aligned as contents of file may need to be aligned

    // idk if it is worth checking the returned size? It seems to be wrong on windows
    //fread(contents, 1, *size, file);
    size_t read = fread(contents, 1, *size, file);

    if (*size != read) {
        println("Failed to read entire file, %c", file_name);
        println("    File Size: %u, Size Read: %u", *size, read);
    }

    //ASSERT(read == *byte_count, "Failed to read entire file: read = %i, file_len = %i", read, *byte_count);
    fclose(file);

    return (u8*)contents;
}

const u8* file_read_char_heap_padded(const char *file_name, u64 *size, int pad_size) {
    FILE *file = fopen(file_name, "r");

    if (!file) {
        println("FAILED TO READ FILE %c", file_name);
        return nullptr;
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    void *contents = memory_allocate_heap(size[0] + pad_size, 8);

    // idk if it is worth checking the returned size? It seems to be wrong on windows
    //fread(contents, 1, *size, file);
    size_t read = fread(contents, 1, *size, file);

    if (*size != read) {
        println("Failed to read entire file, %c", file_name);
        println("    File Size: %u, Size Read: %u", *size, read);
    }

    //ASSERT(read == *byte_count, "Failed to read entire file: read = %i, file_len = %i", read, *byte_count);
    fclose(file);

    return (u8*)contents;
}
const u8* file_read_char_temp_padded(const char *file_name, u64 *size, int pad_size) {
    FILE *file = fopen(file_name, "r");

    if (!file) {
        println("FAILED TO READ FILE %c", file_name);
        return nullptr;
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    void *contents = memory_allocate_temp(size[0] + pad_size, 8);

    // idk if it is worth checking the returned size? It seems to be wrong on windows
    //fread(contents, 1, *size, file);
    size_t read = fread(contents, 1, *size, file);

    if (*size != read) {
        println("Failed to read entire file, %c", file_name);
        println("    File Size: %u, Size Read: %u", *size, read);
    }

    //ASSERT(read == *byte_count, "Failed to read entire file: read = %i, file_len = %i", read, *byte_count);
    fclose(file);

    return (u8*)contents;
}
