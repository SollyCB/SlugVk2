#version 450

layout(binding = 0, set = 4) uniform sampler2D tex_4;
layout(binding = 0, set = 0) uniform sampler2D tex_0;
layout(binding = 0, set = 2) uniform sampler2D tex_2;
layout(binding = 0, set = 1) uniform sampler2D tex_1;
layout(binding = 0, set = 3) uniform sampler2D tex_3;

layout(binding = 1, set = 0, std140) uniform UBO {
    vec3 a_vec;
} ubo;

layout(binding = 2, set = 0) buffer A_VEC {vec3 a_vec;} storage_vec;

layout(binding = 3, set = 0) uniform textureBuffer tex_buffer;

void main() {
    gl_Position = vec4(1, 1, 1, 1);
}
