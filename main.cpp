#include "basic.h"
#include "glfw.hpp"
#include "gpu.hpp"
#include "file.hpp"

int main() {
    init_allocators();

    init_glfw(); 
    Glfw *glfw = get_glfw_instance();

    init_gpu();
    Gpu *gpu = get_gpu_instance();

    init_window(gpu, glfw);
    Window *window = get_window_instance();

    while(!glfwWindowShouldClose(glfw->window)) {
        poll_and_get_input_glfw(glfw);
    }

    kill_window(gpu, window);
    kill_gpu(gpu);
    kill_glfw(glfw);

    kill_allocators();
    return 0;
}
