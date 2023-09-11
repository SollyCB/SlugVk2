#include "external/stb_sprintf.h"

#include <iostream>

#if (_WIN32)
#define print(fmt, ...) \
    char PRINT_STB_PRINT_BUF[127]; \
    stbsp_sprintf(PRINT_STB_PRINT_BUF, fmt, __VA_ARGS__); \
    std::cout << PRINT_STB_PRINT_BUF;

#define println(fmt, ...) { \
    char PRINT_STB_PRINT_BUF[127]; \
    stbsp_sprintf(PRINT_STB_PRINT_BUF, fmt, __VA_ARGS__); \
    std::cout << PRINT_STB_PRINT_BUF << '\n'; }
#else
#define print(fmt, ...) \
    char PRINT_STB_PRINT_BUF[127]; \
    stbsp_sprintf(PRINT_STB_PRINT_BUF, fmt); \
    std::cout << PRINT_STB_PRINT_BUF;

#define println(fmt, ...) { \
    char PRINT_STB_PRINT_BUF[127]; \
    stbsp_sprintf(PRINT_STB_PRINT_BUF, fmt); \
    std::cout << PRINT_STB_PRINT_BUF << '\n'; }
#endif
