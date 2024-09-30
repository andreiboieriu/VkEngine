#version 460

#extension GL_GOOGLE_include_directive : require
// #include "input_structures.glsl"

// layout(set = 0, binding = 0) uniform sampler2D skybox;

layout(set = 0, binding = 0) uniform SceneData {   

	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 ambientColor;
	vec4 sunlightDirection;
	vec4 sunlightColor;
	vec4 viewPosition;
} sceneData;

layout(set = 1, binding = 0) uniform sampler2D colorTex;
layout(set = 1, binding = 1) uniform sampler2D metalRoughTex;

layout(set = 1, binding = 2) uniform GLTFMaterialData {   

	vec4 colorFactors;
	vec4 metal_rough_factors;
	
} materialData;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inFragWorldPos;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	// get texture color
	vec3 color = inColor * texture(colorTex, inUV).xyz;

	// get ambient color
	vec3 ambient = sceneData.sunlightColor.xyz * sceneData.ambientColor.xyz;

	// get diffuse color
	vec3 normal = normalize(inNormal);
	vec3 lightDirection = normalize(sceneData.sunlightDirection.xyz);
	vec3 diffuse = max(dot(normal, lightDirection), 0.0) * sceneData.sunlightColor.xyz;

	// get specular color
	vec3 viewDirection = normalize(sceneData.viewPosition.xyz - inFragWorldPos);
	vec3 reflectDirection = reflect(-lightDirection, normal);
	vec3 specular = pow(max(dot(viewDirection, reflectDirection), 0.0), 32) * sceneData.sunlightColor.xyz;

	outFragColor = vec4((ambient + diffuse + specular) * color, 1.0);
}
