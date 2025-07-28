#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

// #include "input_structures.glsl"

layout(set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    mat4 viewproj;
    mat4 lightSpace[4];
    vec4 sunlightDirection; //w for sun power
    vec4 sunlightColor;
    vec4 viewPosition;
    vec4 clearColor;
    vec4 cascadePlanes;
    vec4 shadowConfig;
} sceneData;

layout(set = 1, binding = 0) uniform GLTFMaterialData {
    vec4 colorFactors;
    vec4 metalRoughFactors;
    vec4 emissiveFactors;
    float emissiveStrength;
    float normalScale;
} materialData;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec3 outFragWorldPos;
layout(location = 3) out vec4 outFragLightPos;

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
    vec4 worldPos = PushConstants.render_matrix * position;

    gl_Position = sceneData.viewproj * worldPos;

    outNormal = mat3(transpose(inverse(PushConstants.render_matrix))) * v.normal;
    outUV.x = v.uvX;
    outUV.y = v.uvY;
    outFragWorldPos = vec3(worldPos);
}
