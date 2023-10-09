#version 450
#extension GL_EXT_debug_printf : enable

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec2 tex_coord_0;

layout(location = 0) out vec3 color;

vec3 colors[] = {
    vec3(1, 0, 0),
    vec3(0, 1, 0),
    vec3(0, 0, 1),
};
vec3 positions[] = {
    vec3(-0.5f, -0.5f, 0.0f),
    vec3( 0.5f, -0.5f, 0.0f),
    vec3( 0.5f,  0.5f, 0.0f)
};

void main() {
    if (abs(normal.y) == 1) {
        color = vec3(1, 0, 0);
    } else if (abs(normal.x) == 1) {
        color = vec3(0, 1, 0);
    } else if (abs(normal.z) == 1) {
        color = vec3(0, 0, 1);
    }

    // .7071
    mat4 ident = mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    mat4 rotx = mat4(1, 0, 0, 0, 0, 0.7071, -0.7071, 0, 0, 0.7071, 0.7071, 0, 0, 0, 0, 1);
    mat4 roty = mat4(0.7071, 0, 0.7071, 0, 0, 1, 0, 0, -0.7071, 0, 0.7071, 0, 0, 0, 0, 1);

    //ident *= rotx;
    //ident *= roty;

    vec4 pos = rotx * roty * vec4(position, 1);

    gl_Position = vec4(pos.x, pos.y + 0.5, pos.z + 2.2, 1);
}
