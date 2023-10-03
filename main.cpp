#include "basic.h"
#include "glfw.hpp"
#include "gpu.hpp"
#include "file.hpp"
#include "spirv.hpp"
#include "math.hpp"
#include "image.hpp"
#include "gltf.hpp"
#include "simd.hpp"
#include "renderer.hpp"

#if TEST
#include "test.hpp"
void run_tests(); // defined below main
#endif

int main() {
    init_allocators();

#if TEST
    run_tests(); 
#endif
    /* Begin Code That Actually Does Stuff */
    Gltf model = parse_gltf("models/cube-static/Cube.gltf");

#if 0
    /* StartUp Code */
    init_glfw(); 
    Glfw *glfw = get_glfw_instance();

    init_gpu();
    Gpu *gpu = get_gpu_instance();

    init_window(gpu, glfw);
    Window *window = get_window_instance();


    /* ShutDown Code */
    kill_window(gpu, window);
    kill_gpu(gpu);
    kill_glfw(glfw);
#endif

    kill_allocators();
    return 0;
}

#if TEST
void run_tests() {
    load_tests();

    test_spirv();
    test_gltf();

    end_tests();
}
#endif
