#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 tangent;
layout(location = 3) in vec2 tex_coord_0;

layout(set = 0, binding = 0) uniform sampler2D tex_sampler[2];

layout(set = 1, binding = 0) uniform UBO {
    mat4 model;
    mat4 proj;
    mat4 view; // combine two of these, dont remember which two...
} ubo;


layout(location = 0) out vec4 frag_color;

void main() {
    gl_Position = vec4(position, 1.0);
    frag_color = vec4(texture(tex_sampler[0], tex_coord_0));
}
