#version 450

layout(binding = 0, set = 4) uniform sampler2D tex_4;
layout(binding = 0, set = 0) uniform sampler2D tex_0;
layout(binding = 0, set = 2) uniform sampler2D tex_2;
layout(binding = 0, set = 1) uniform sampler2D tex_1;
layout(binding = 0, set = 3) uniform sampler2D tex_3;

void main() {
    gl_Position = vec4(1, 1, 1, 1);
}
