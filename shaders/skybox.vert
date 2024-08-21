#version 460

#extension GL_EXT_buffer_reference : require

struct Vertex {

	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer { 
	Vertex vertices[];
};

//push constants block
layout( push_constant ) uniform constants
{
	mat4 projectionView;
	VertexBuffer vertexBuffer;
	uint mipLevel;
	uint totalMips;
} PushConstants;

layout(location = 0) out vec3 outPos;

void main() 
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];

    outPos = v.position;
	
	vec4 position = PushConstants.projectionView * vec4(v.position, 1.0);

	gl_Position = position;

}
