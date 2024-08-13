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
	vec4 metalRoughFactors; // x = metallic, y = roughness
	
} materialData;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inFragWorldPos;

layout (location = 0) out vec4 outFragColor;

const float PI = 3.1415926535897932384626433832795;

float heavyside(float x) {
    return step(0.0, x);
}

// normal distribution function (specular D)
float D_GGX(float NdotH, float roughness) {
    float a2 = roughness * roughness;

    float nom = a2 * heavyside(NdotH);

    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

// approximated specular V term
float V_SmithGGXCorrelatedFast(float NoV, float NoL, float roughness) {
    float a = roughness;
    float GGXV = NoL * (NoV * (1.0 - a) + a);
    float GGXL = NoV * (NoL * (1.0 - a) + a);

    return 0.5 / (GGXV + GGXL);
}

vec3 toneMapping(vec3 color) {
    vec3 mapped = color / (color + vec3(1.0));
    // mapped = pow(mapped, vec3(1.0) / 2.2);

    return mapped;
}

void main() 
{
	// get texture color
	vec4 baseColor = materialData.colorFactors * texture(colorTex, inUV);

	vec3 n = normalize(inNormal);
	vec3 l = normalize(sceneData.sunlightDirection.xyz);
	vec3 v = normalize(sceneData.viewPosition.xyz - inFragWorldPos);
    vec3 h = normalize(v + l);

    float NdotV = abs(dot(n, v)) + 1e-5;
    float NdotL = clamp(dot(n, l), 0.0, 1.0);
    float NdotH = clamp(dot(n, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);

    float roughness = materialData.metalRoughFactors.y * texture(metalRoughTex, inUV).g;
    float metallic = materialData.metalRoughFactors.x * texture(metalRoughTex, inUV).b;

    vec3 c_diff = mix(baseColor.rgb, vec3(0.0), metallic);
    vec3 f0 = mix(vec3(0.04), baseColor.rgb, metallic);
    float a = roughness * roughness;

    vec3 F = f0 + (1 - f0) * pow((1 - abs(VdotH)), 5);
    
    vec3 f_diffuse = (1 - F) * (1 / PI) * c_diff;
    vec3 f_specular = F * D_GGX(NdotH, roughness) * V_SmithGGXCorrelatedFast(NdotV, NdotL, roughness);

    outFragColor = vec4((f_diffuse + f_specular) * 5 * NdotL, baseColor.a);
}
