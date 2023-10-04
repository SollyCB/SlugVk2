#version 450

layout(location = 0) in vec3 normal;
layout(location = 1) in vec3 tangent;
layout(location = 2) in vec2 tex_coord_0;

layout(set = 1, binding = 0) uniform sampler2D tex_sampler[2];

layout(location = 0) out vec4 frag_color_out;

void main() {
    frag_color_out = texture(tex_sampler[0], tex_coord_0);
}
