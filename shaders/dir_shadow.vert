#version 460

#extension GL_EXT_buffer_reference : require

struct Vertex {
    vec3 position;
    float uvX;
    vec3 normal;
    float uvY;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

//push constants block
layout(push_constant) uniform constants
{
    mat4 render_matrix;
    VertexBuffer vertexBuffer;
} PushConstants;

void main()
{
    Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];

    vec4 position = vec4(v.position, 1.0);

    gl_Position = PushConstants.render_matrix * position;
}
