#version 450

layout(binding = 0, set = 0) buffer UBO {
    vec3 b_vec;
} ubo[4];

void main() {
    gl_Position = vec4(1, 1, 1, 1);
}
