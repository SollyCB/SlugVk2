#version 450

layout(location = 0) in position;
layout(location = 1) in normal;
layout(location = 2) in tangent;
layout(location = 3) in tex_coord_0;

layout(binding = 0) uniform sampler2D tex_sampler_0;
layout(binding = 1) uniform sampler2D pbr_metallic_roughness_sampler;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec4 pbr_metallic_roughness;

void main() {
    gl_Position = position;
}
