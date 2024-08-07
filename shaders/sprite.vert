#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

// #include "input_structures.glsl"
struct Vertex {

	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer { 
	Vertex vertices[];
};

layout(set = 0, binding = 0) uniform Ubo {   
    mat4 projectionMatrix;
    mat4 modelMatrix;
    VertexBuffer vertexBuffer;
	vec2 padding0;
	vec4 padding1;
	vec4 padding2;
	vec4 padding3;
	mat4 padding4;
} ubo;

layout (location = 0) out vec2 outUV;

void main() 
{
	Vertex v = ubo.vertexBuffer.vertices[gl_VertexIndex];
	
	vec4 position = vec4(v.position.x, v.position.y, 1.0, 1.0);

	gl_Position = ubo.projectionMatrix * ubo.modelMatrix * position;

	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
}