#include "test.hpp"
#include <cstring>
#include <iostream>
namespace Sol::Test {

static void module_skipped_msg() {
    std::cout << YELLOW << "MODULE SKIPPED" << NC << '\n';
}
static void module_begin_msg(const char* module_name, 
        const char* file_name, const char* function_name, bool skippable, bool global_skip) {
    std::cout << CYAN << "\n[ ModuleFile: " << file_name << " ] \n" << NC \
    << "Running Test Module " << BLUE << module_name << NC << ", " << YELLOW << "SKIPPING TESTS " 
    << NC << "(" << BLUE << "MODULE LOCAL" << NC << ") ";
    if (skippable) {
        std::cout << GREEN << "ENABLED" << NC;
        if (!global_skip)
            std::cout << ", (" << RED << "DISABLED GLOBALLY" << NC << "):\n";
        else
            std::cout << ":\n";
    }
    else
        std::cout << RED << "DISABLED" << NC << ":\n";

}
static void module_pass_msg() {
    std::cout << GREEN << "OK\n" << NC;
}
void test_skipped(const char* test_name) {
    std::cout << YELLOW << "SKIPPED " << NC << test_name << '\n';
}

static Suite sTestSuite;
Suite* Suite::instance() {
    return &sTestSuite;
}
void Suite::init(bool skippable) {
    std::cout << "\nInitializing Test Suite, ";
    enable_skips = skippable;

    if (enable_skips)
        std::cout << YELLOW << " SKIPPING TESTS "<< GREEN << "ENABLED" << NC << ":\n";
    else
        std::cout << YELLOW << " SKIPPING TESTS " << RED << "DISABLED" << NC << ":\n";

    modules.init();
}
void Suite::kill() {
    std::cout << "\nEnding Test Suite:\n";

    uint32_t fail_count = 0;
    Module** failed_modules = reinterpret_cast<Module**>(malloc(modules.len * sizeof(size_t)));
    for(uint32_t i = 0; i < modules.len; ++i) {
        if (!modules.data[i].ok) {
            failed_modules[fail_count] = &modules.data[i];
            ++fail_count;
        }
    }

    if (modules.len == 0) {
        std::cout << YELLOW << "No tests to run...\n";
    } else if (fail_count == 0) {
        std::cout << GREEN << "    All Tests Passed!\n" << NC;
    } else {
        std::cout << "    " << fail_count << " Modules " << RED << "FAILED!\n" << NC;
        for(uint32_t i = 0; i < fail_count; ++i) 
            std::cout << "    [ " << failed_modules[i]->file_name.cstr() << " ] " 
            << "ModuleName: " << BLUE <<  failed_modules[i]->module_name.cstr() << NC << '\n';
    }

    modules.kill();
    free(failed_modules);
}

void Module::begin(const char* name, const char* file_name, const char* function_name, bool skippable, bool skip_module) {
    Module *ret = Suite::instance()->modules.new_last();
    *ret = {};
    ret->module_name = StringBuffer::get(name);
    ret->file_name = StringBuffer::get(file_name);
    ret->skippable = skippable && Suite::instance()->enable_skips;
    ret->skip_module = skip_module && ret->skippable;
    module_begin_msg(name, file_name, function_name, skippable, Suite::instance()->enable_skips);

    if (ret->skippable && skip_module)
        module_skipped_msg();
}
void Module::end() {
    if (Suite::instance()->modules.last()->skip_module)
        return;
    if (Suite::instance()->modules.last()->skipped) {
        std::cout << YELLOW << "ALL TESTS SKIPPED\n" << NC;
        return;
    }

    if (Suite::instance()->modules.last()->ok)
        module_pass_msg();
}
void Module::kill() {
    module_name.kill();
    file_name.kill();
}

StringBuffer StringBuffer::get(const char* str) {
    size_t len = 0;
    do {
        ++len;
    } while(str[len] != '\0');

    StringBuffer ret;
    // Malloc for every string bad...
    // get a linear allocator or smtg for test names...
    ret.data = reinterpret_cast<char*>(malloc(len));
    memcpy(ret.data, str, len);

    return ret;
}
void StringBuffer::kill() {
    free(data);
}
const char* StringBuffer::cstr() {
    return data;
}

void List::init() {
    cap = 32;
    data = reinterpret_cast<Module*>(malloc(cap * sizeof(Module)));
}
void List::kill() {
    for(uint32_t i = 0; i < len; ++i) 
        data[i].kill();
    free(data);
    cap = 0;
    len = 0;
}
void List::grow() {
    cap *= 2;
    Module* old_data = data;
    data = reinterpret_cast<Module*>(malloc(sizeof(Module) * cap));
    memcpy(data, old_data, len * sizeof(Module));
    free(old_data);
}    
void List::push(Module &module) {
    if (cap == len)
        grow();

    data[len] = std::move(module);
    ++len;
}
Module* List::new_last() {
    if (len == cap)
        grow();

    ++len;
    return data + len - 1;
}
Module* List::last() {
    return data + len - 1;
}

} // namespace Sol::Test
