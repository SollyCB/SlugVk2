#version 450

layout(location = 0) in vec3 in_pos;

layout(binding = 0) uniform Camera {
    mat4 model_view;
    mat4 projection;
} camera;


layout(binding = 0) uniform sampler2D tex_sampler[4];


void main() {
    gl_Position = vec4(1, 1, 1, 1);
}
