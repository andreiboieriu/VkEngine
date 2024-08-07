#version 460

layout(set = 0, binding = 1) uniform sampler2D colorTex;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main() {
    outFragColor = texture(colorTex, inUV);
}