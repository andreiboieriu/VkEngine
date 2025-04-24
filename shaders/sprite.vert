#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

// #include "input_structures.glsl"
struct Vertex {
    vec3 position;
    float uvX;
    vec3 normal;
    float uvY;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(push_constant) uniform constants
{
    mat4 worldMatrix;
    VertexBuffer vertexBuffer;
} PushConstants;

layout(location = 0) out vec2 outUV;

void main()
{
    Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];

    vec4 position = vec4(v.position.x, v.position.y, 1.0, 1.0);

    gl_Position = PushConstants.worldMatrix * position;

    outUV.x = v.uvX;
    outUV.y = v.uvY;
}
