#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_tangent;
layout(location = 3) in vec2 in_tex_coord_0;

layout(location = 0) out vec3 out_normal;
layout(location = 1) out vec3 out_tangent;
layout(location = 2) out vec2 out_tex_coord_0;

layout(set = 0, binding = 0) uniform UBO {
    mat4 model;
    mat4 proj;
    mat4 view; // combine two of these, dont remember which two...
} ubo;

void main() {
    gl_Position = vec4(position, 1.0);

    out_normal      = in_normal;
    out_tangent     = in_tangent;
    out_tex_coord_0 = in_tex_coord_0;
}
