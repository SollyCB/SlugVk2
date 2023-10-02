#version 450

layout(location = 0) in vec4 frag_color;
layout(location = 0) out vec4 frag_color_out; // bad naming...

void main() {
    frag_color_out = frag_color;
}
