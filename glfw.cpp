#include "glfw.hpp"

static Glfw *s_Glfw;
Glfw* get_glfw_instance() { 
    return s_Glfw;
}

void init_glfw() {
    glfwInit();

    // no opengl context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    glfwSetErrorCallback(error_callback_glfw);

    s_Glfw = (Glfw*)memory_allocate_heap(sizeof(Glfw), 8);

    Glfw *glfw = get_glfw_instance();
    glfw->window = glfwCreateWindow(640, 480, "GLFW Window", NULL, NULL);
    ASSERT(glfw->window, "GLFW Fail To Create Window");
}

void kill_glfw(Glfw *glfw) {
    glfwDestroyWindow(glfw->window);
    glfwTerminate();
    memory_free_heap(glfw);
}

enum InputValues {
    INPUT_CLOSE_WINDOW  = GLFW_KEY_Q,
    INPUT_MOVE_FORWARD  = GLFW_KEY_W,
    INPUT_MOVE_BACKWARD = GLFW_KEY_S,
    INPUT_MOVE_RIGHT    = GLFW_KEY_D,
    INPUT_MOVE_LEFT     = GLFW_KEY_A,
    INPUT_JUMP          = GLFW_KEY_SPACE,
};

void window_poll_and_get_input(Glfw *glfw) {
    glfwPollEvents();

    int state = glfwGetKey(glfw->window, (int)INPUT_CLOSE_WINDOW);
    if (state == GLFW_PRESS) {
        glfwSetWindowShouldClose(glfw->window, GLFW_TRUE);
        println("\nINPUT_CLOSE_WINDOW Key Pressed...");
    }
}
