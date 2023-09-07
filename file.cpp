#include "file.hpp"

const u8* file_read_bin_temp(const char *file_name, u64 *size) {
    FILE *file = fopen(file_name, "rb");

    if (!file) {
        std::cerr << "FAILED TO READ FILE " << file_name << "!\n";
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
        println("Failed to read entire file: read %i, file_len %i", read, *size);

    //ASSERT(read == *byte_count, "Failed to read entire file: read = %i, file_len = %i", read, *byte_count);
    fclose(file);

    return (u8*)contents;
}
