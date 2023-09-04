#include "external/stb_sprintf.h"

#include <iostream>

#define print(fmt, ...) \
    char PRINT_STB_PRINT_BUF[127]; \
    stbsp_sprintf(PRINT_STB_PRINT_BUF, fmt, __VA_ARGS__); \
    std::cout << PRINT_STB_PRINT_BUF;

#define println(fmt, ...) { \
    char PRINT_STB_PRINT_BUF[127]; \
    stbsp_sprintf(PRINT_STB_PRINT_BUF, fmt, __VA_ARGS__); \
    std::cout << PRINT_STB_PRINT_BUF << '\n'; }
