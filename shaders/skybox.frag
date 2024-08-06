#version 460

layout(set = 0, binding = 0) uniform samplerCube skybox;

layout(location = 0) in vec3 inPos;

layout(location = 0) out vec4 outFragColor;

void main() 
{
    gl_FragDepth = 0.0;
	outFragColor = texture(skybox, inPos);
}
