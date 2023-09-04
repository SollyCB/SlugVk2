#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace Sol::Test {

static const char* RED = "\u001b[1m\u001b[31;1m";
static const char* NC = "\033[0m";
static const char* GREEN = "\u001b[32;1m";
static const char* YELLOW = "\033[1;33m";
static const char* BLUE = "\u001b[35;1m";
static const char* CYAN = "\u001b[34;1m";

template<typename T>
void test_fail(
        const char* test_name, T val1, T val2, const char* name1, 
        const char* name2, const char* op, const char* file_name, const char* function_name) 
{
    std::cout << RED << "    TEST FAIL! " << NC << "[ FunctionName: " << function_name << " ], TestName \"" << test_name << "\":\n        " \
    << name1 << ' ' << op << ' ' << name2 << ", " << name1 << " = " << val1 \
    << ", " << name2 << " = " << val2 << '\n';
}
void test_skipped(const char* test_name);

struct TestClass {
    virtual void run() = 0;
};
struct StringBuffer {
    char* data = nullptr;
    static StringBuffer get(const char* str);
    void kill();
    const char* cstr();
};
struct Module {
    StringBuffer module_name;
    StringBuffer file_name;
    bool skippable = true;
    bool skip_module = false;
    bool skipped = true;
    bool ok = true;
    uint32_t test_index = 0;

    static void begin(const char* name, const char* file_name, const char* function_name, bool skippable = true, bool skip_module = false);
    static void end();
    void kill();

    template<typename T>
    void test_eq(const char* test_name, T arg1, T arg2, const char* arg1_name, const char* arg2_name, const char* file_name, const char* function_name, bool skip) {
        if (skip_module)
            return;

        test_name = strcmp(test_name, "") == 0 ? "unnamed" : test_name;
        ++test_index;
        if (skippable && skip) {
            test_skipped(test_name);
            return;
        } else 
            skipped = false;

        if (arg1 == arg2)
            return;
        ok = false;
        test_fail(test_name, arg1, arg2, arg1_name, arg2_name, "!=", file_name, function_name);
    }
    template<typename T>
    void test_neq(const char* test_name, T arg1, T arg2, const char* arg1_name, const char* arg2_name, const char* file_name, const char* function_name, bool skip) {
        if (skip_module)
            return;

        test_name = strcmp(test_name, "") == 0 ? "unnamed" : test_name;
        ++test_index;
        if (skippable && skip) {
            test_skipped(test_name);
            return;
        } else 
            skipped = false;

        if (arg1 != arg2)
            return;
        ok = false;
        test_fail(test_name, arg1, arg2, arg1_name, arg2_name, "==", file_name, function_name);
    }
    template<typename T>
    void test_str_eq(const char* test_name, T arg1, T arg2, const char* arg1_name, const char* arg2_name, const char* file_name, const char* function_name, bool skip) {
        if (skip_module)
            return;

        test_name = strcmp(test_name, "") == 0 ? "unnamed" : test_name;
        ++test_index;
        if (skippable && skip) {
            test_skipped(test_name);
            return;
        } else 
            skipped = false;

        if (strcmp(arg1, arg2) == 0)
            return;
        ok = false;
        test_fail(test_name, arg1, arg2, arg1_name, arg2_name, "==", file_name, function_name);
    }

};
struct List {
    size_t len = 0;
    size_t cap = 0;
    Module* data = nullptr;
    void init();
    void kill();
    void push(Module &module);
    void grow();
    Module* new_last();
    Module* last();
};
struct Suite {
    List modules;
    bool enable_skips = false;
    static Suite* instance();
    void init(bool skippable);
    void kill();
};
} // namespace Sol::Test
#define TEST_MODULE_BEGIN(name, skippable, skip_module) \
    Sol::Test::Module::begin(name, __FILE__, __FUNCTION__, skippable, skip_module);
#define TEST_MODULE_END() \
    Sol::Test::Module::end();
#define TEST_EQ(test_name, arg1, arg2, skip) \
    Sol::Test::Suite::instance()->modules.last()->test_eq(test_name, arg1, arg2, #arg1, #arg2, __FILE__, __FUNCTION__, skip);
#define TEST_NEQ(test_name, arg1, arg2, skip) \
    Sol::Test::Suite::instance()->modules.last()->test_neq(test_name, arg1, arg2, #arg1, #arg2, __FILE__, __FUNCTION__, skip);
#define TEST_STR_EQ(test_name, arg1, arg2, skip) \
    Sol::Test::Suite::instance()->modules.last()->test_str_eq(test_name, arg1, arg2, #arg1, #arg2, __FILE__, __FUNCTION__, skip);
