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
    vec4 data;
} sceneData;

layout(set = 0, binding = 1) uniform samplerCube irradianceMap;
layout(set = 0, binding = 2) uniform samplerCube prefilteredEnvMap;
layout(set = 0, binding = 3) uniform sampler2D brdfLut;

layout(set = 1, binding = 0) uniform GLTFMaterialData {   

    vec4 colorFactors;
    vec4 metalRoughFactors;
    vec4 emissiveFactors;
    float emissiveStrength;
    float normalScale;
    float occlusionStrength;
} materialData;

layout(set = 1, binding = 1) uniform sampler2D colorTex;
layout(set = 1, binding = 2) uniform sampler2D metalRoughTex;
layout(set = 1, binding = 3) uniform sampler2D normalTex;
layout(set = 1, binding = 4) uniform sampler2D emissiveTex;
layout(set = 1, binding = 5) uniform sampler2D occlusionTex;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inFragWorldPos;

layout (location = 0) out vec4 outFragColor;

const float PI = 3.1415926535897932384626433832795;

// normal distribution function (specular D)
float D_GGX(vec3 N, vec3 H, float a) {
    float a2 = a * a;

    float nom = a2;

    float NdotH = max(dot(N, H), 0.0);

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

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(vec3 H, vec3 V, vec3 F0)
{
    float cosTheta = max(dot(H, V), 0.0);

    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float NdotV, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - NdotV, 0.0, 1.0), 5.0);
}   

mat3 cotangentFrame(vec3 n, vec3 p, vec2 uv) {
	// get edge vectors of the pixel triangle
	vec3 dp1 = dFdx( p );
	vec3 dp2 = dFdy( p );
	vec2 duv1 = dFdx( uv );
	vec2 duv2 = dFdy( uv );

	// solve the linear system
	vec3 dp2perp = cross( dp2, n );
	vec3 dp1perp = cross( n, dp1 );
	vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
	vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
	
	// construct a scale-invariant frame
	float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );
	
	return mat3( T * invmax, B * invmax, n );
}

vec3 perturbNormal(vec3 n, vec3 v) {
	vec3 map = normalize((texture(normalTex, inUV).xyz * 2.0 - 1.0) * vec3(materialData.normalScale, materialData.normalScale, 1.0));

    mat3 TBN = cotangentFrame(n, -v, inUV);

    return normalize(TBN * map);
}

void main() 
{
	// get texture color
	vec4 baseColor = materialData.colorFactors * texture(colorTex, inUV);

	vec3 N = normalize(inNormal);
	vec3 L = normalize(sceneData.sunlightDirection.xyz);
	vec3 V = normalize(sceneData.viewPosition.xyz - inFragWorldPos);
    vec3 H = normalize(V + L);

    // get normal from normal map
    if (sceneData.data.w > 0.9f)
    N = perturbNormal(N, V);

    vec3 R = reflect(-V, N);

    float NdotV = abs(dot(N, V)) + 1e-5;

    float roughness = materialData.metalRoughFactors.y * texture(metalRoughTex, inUV).g;
    float metallic = materialData.metalRoughFactors.x * texture(metalRoughTex, inUV).b;

    // ----------------------------------------------------------------------------------
    vec3 c_diff = mix(baseColor.rgb, vec3(0.0), metallic);
    vec3 F0 = mix(vec3(0.04), baseColor.rgb, metallic);
    float a = roughness * roughness;

    vec3 F = fresnelSchlick(H, V, F0);
    vec3 f_diffuse = (1 - F) * (1 / PI) * c_diff;

    float NDF = D_GGX(N, H, a);
    float G = GeometrySmith(N, V, L, a);

    vec3 f_specular = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);

    float NdotL = max(dot(N, L), 0.0);

    // calculate light output
    vec3 lo = (f_diffuse + f_specular) * sceneData.sunlightColor.xyz * NdotL;

    F = fresnelSchlickRoughness(NdotV, F0, roughness);

    // calculate indirect diffuse lighting
    vec3 ambientDiffuse = (1 - F) * texture(irradianceMap, N).xyz * c_diff;

    // calculate indirect specular lighting
    const float MAX_REFLECTION_LOD = 8.0;
    vec3 prefilteredColor = textureLod(prefilteredEnvMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 envBRDF = texture(brdfLut, vec2(NdotV, roughness)).rg;
    vec3 ambientSpecular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

    // calculate total indirect lighting
    float occlusion = 1.0 + materialData.occlusionStrength * (texture(occlusionTex, inUV).r - 1.0);
    vec3 ambient = (ambientDiffuse + ambientSpecular) * occlusion;

    ambient = max(ambient, 0.0);

    // calculate emissive light
    vec3 emissive = materialData.emissiveFactors.xyz * texture(emissiveTex, inUV).xyz * materialData.emissiveStrength;

    // calculate final color
    vec3 color = lo + ambient + emissive;

    outFragColor = vec4(color, baseColor.a);
}
