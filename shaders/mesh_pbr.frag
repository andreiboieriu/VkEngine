#version 460

#extension GL_GOOGLE_include_directive : require
// #include "input_structures.glsl"

// layout(set = 0, binding = 0) uniform sampler2D skybox;

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

layout(set = 0, binding = 1) uniform samplerCube irradianceMap;
layout(set = 0, binding = 2) uniform samplerCube prefilteredEnvMap;
layout(set = 0, binding = 3) uniform sampler2D brdfLut;
layout(set = 0, binding = 4) uniform sampler2D shadowMap;

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

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inFragWorldPos;
layout(location = 3) in vec4 inFragLightPos;

layout(location = 0) out vec4 outFragColor;

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

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
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
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    // solve the linear system
    vec3 dp2perp = cross(dp2, n);
    vec3 dp1perp = cross(n, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    // construct a scale-invariant frame
    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));

    return mat3(T * invmax, B * invmax, n);
}

vec3 perturbNormal(vec3 n, vec3 v) {
    vec3 tangentNormal = normalize((texture(normalTex, inUV).xyz * 2.0 - 1.0));

    // mat3 TBN = cotangentFrame(n, inFragWorldPos, inUV);

    vec3 Q1 = dFdx(inFragWorldPos);
    vec3 Q2 = dFdy(inFragWorldPos);
    vec2 st1 = dFdx(inUV);
    vec2 st2 = dFdy(inUV);

    vec3 N = normalize(n);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

// A small, pre-calculated set of Poisson disk samples for our soft PCF.
// Using 4 samples is a great balance between quality and performance.
const vec2 POISSON_DISK[4] = vec2[](
        vec2(-0.94201624, -0.39906216),
        vec2(0.94558609, -0.76890725),
        vec2(-0.094184101, -0.92938870),
        vec2(0.34495938, 0.29387760)
    );

// Helper function to generate a random rotation for our PCF pattern
float random(vec2 seed) {
    return fract(sin(dot(seed, vec2(12.9898, 78.233))) * 43758.5453123);
}

float getShadowForCascade(vec3 worldPos, int cascade)
{
    // --- 1. BIASING: Normal Offset ---
    // This is the first and most important improvement. We push the test position
    // slightly along the surface normal. This is far more robust than depth bias alone.
    // shadowConfig.z can be used for the normal offset amount (e.g., 0.1 - 0.5).
    float normalOffset = sceneData.shadowConfig.z;
    vec3 normal = normalize(inNormal);
    vec3 offsetWorldPos = worldPos + normal * normalOffset;

    // --- 2. TRANSFORM TO LIGHT SPACE ---
    // We use our new offset position for the shadow test.
    vec4 lsPos = sceneData.lightSpace[cascade] * vec4(offsetWorldPos, 1.0);
    vec3 ndc = lsPos.xyz / lsPos.w;

    // Boundary check
    if (ndc.z > 1.0 || ndc.x < -1.0 || ndc.x > 1.0 || ndc.y < -1.0 || ndc.y > 1.0) {
        return 1.0; // Not in this cascade's view
    }

    // --- 3. UV and Depth Calculation ---
    vec2 uv = ndc.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;
    uv.x = uv.x / 4.0 + float(cascade) / 4.0;

    float currentDepth = ndc.z;

    // --- 4. DEPTH BIAS (Still useful as a fine-tuning tool) ---
    // You should no longer need to divide this by 10. Your editor values should
    // now work as expected because normal offset bias is doing the heavy lifting.
    vec3 lightDir = normalize(sceneData.sunlightDirection.xyz);
    float constantBias = sceneData.shadowConfig.x;
    float slopeBias = sceneData.shadowConfig.y;
    float lightDepthRange = sceneData.shadowConfig.w;

    float worldUnitBias = max(slopeBias * (1.0 - dot(normal, lightDir)), constantBias);
    float depthBias = worldUnitBias / (lightDepthRange + 0.00001);

    // --- 5. HIGH-QUALITY PCF (Soft Shadows) ---
    float shadow = 0.0;

    // CORRECTED Texel Size: This is crucial. We calculate the size of a texel
    // relative to a SINGLE cascade's texture, not the whole atlas.
    float cascadeWidth = float(textureSize(shadowMap, 0).x) / 4.0;
    vec2 texelSize = 1.0 / vec2(cascadeWidth, float(textureSize(shadowMap, 0).y));

    // Define the radius of our shadow filter in UV space. A larger radius = softer shadows.
    float pcfRadius = 2.2;

    // Get a random rotation angle for the sample pattern to reduce banding
    float randomAngle = random(gl_FragCoord.xy) * 6.283185; // 2 * PI
    mat2 rotationMatrix = mat2(cos(randomAngle), sin(randomAngle), -sin(randomAngle), cos(randomAngle));

    // Loop through our pre-defined sample points
    for (int i = 0; i < 4; i++)
    {
        // Rotate the sample point and scale it by our filter radius
        vec2 offset = rotationMatrix * POISSON_DISK[i];
        float pcfDepth = texture(shadowMap, uv + offset * pcfRadius * texelSize).r;

        // Add the depth bias just before comparing
        if (currentDepth - depthBias <= pcfDepth) {
            shadow += 1.0;
        }
    }

    return shadow / 4.0;
}

float shadowDebug() {
    vec3 viewPos = (sceneData.view * vec4(inFragWorldPos, 1.0)).xyz;
    float depth = abs(viewPos.z);
    vec4 planes = sceneData.cascadePlanes;

    if (depth < planes.x) {
        return 1.0;
    } else if (depth < planes.y) {
        return 0.75;
    } else if (depth < planes.z) {
        return 0.25;
    } else {
        return 0.0;
    }
}

float computeShadow() {
    vec3 viewPos = (sceneData.view * vec4(inFragWorldPos, 1.0)).xyz;
    float depth = abs(viewPos.z);

    // Read the cascade split planes and the new blend distance from the uniform
    vec4 planes = sceneData.cascadePlanes;
    float blendDistance = 5.f;

    // Determine which cascade(s) to sample from
    if (depth < planes.x) {
        // --- We are in Cascade 0, or blending between 0 and 1 ---
        float blendZoneStart = planes.x - blendDistance;

        if (depth > blendZoneStart) {
            // We are in the blend zone
            float alpha = smoothstep(blendZoneStart, planes.x, depth);
            float shadow0 = getShadowForCascade(inFragWorldPos, 0);
            float shadow1 = getShadowForCascade(inFragWorldPos, 1);
            return mix(shadow0, shadow1, alpha);
        } else {
            // We are solidly in Cascade 0
            return getShadowForCascade(inFragWorldPos, 0);
        }
    } else if (depth < planes.y) {
        // --- We are in Cascade 1, or blending between 1 and 2 ---
        float blendZoneStart = planes.y - blendDistance;

        if (depth > blendZoneStart) {
            float alpha = smoothstep(blendZoneStart, planes.y, depth);
            float shadow1 = getShadowForCascade(inFragWorldPos, 1);
            float shadow2 = getShadowForCascade(inFragWorldPos, 2);
            return mix(shadow1, shadow2, alpha);
        } else {
            return getShadowForCascade(inFragWorldPos, 1);
        }
    } else if (depth < planes.z) {
        // --- We are in Cascade 2, or blending between 2 and 3 ---
        float blendZoneStart = planes.z - blendDistance;

        if (depth > blendZoneStart) {
            float alpha = smoothstep(blendZoneStart, planes.z, depth);
            float shadow2 = getShadowForCascade(inFragWorldPos, 2);
            float shadow3 = getShadowForCascade(inFragWorldPos, 3);
            return mix(shadow2, shadow3, alpha);
        } else {
            return getShadowForCascade(inFragWorldPos, 2);
        }
    } else {
        // We are solidly in the last cascade
        return getShadowForCascade(inFragWorldPos, 3);
    }
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
    N = perturbNormal(N, V);

    vec3 R = reflect(-V, N);

    // float NdotV = abs(dot(N, V)) + 1e-5;
    float NdotV = max(dot(N, V), 0.0);

    vec2 metalRough = texture(metalRoughTex, inUV).gb;
    float roughness = materialData.metalRoughFactors.y * metalRough.x;
    roughness = max(roughness, 0.03);
    float metallic = materialData.metalRoughFactors.x * metalRough.y;

    // ----------------------------------------------------------------------------------
    vec3 c_diff = mix(baseColor.rgb, vec3(0.0), metallic);
    vec3 F0 = mix(vec3(0.04), baseColor.rgb, metallic);
    float a = roughness * roughness;

    vec3 F = fresnelSchlick(H, V, F0);
    vec3 f_diffuse = (1 - F) * (1 / PI) * c_diff;

    float NDF = D_GGX(N, H, a);
    float G = GeometrySmith(N, V, L, a);

    float NdotL = max(dot(N, L), 0.0);
    vec3 f_specular = (NDF * G * F) / (4.0 * NdotV * NdotL + 0.0001);

    // calculate light output
    float shadow = computeShadow();
    vec3 lo = shadow * (f_diffuse + f_specular) * sceneData.sunlightColor.xyz * NdotL;

    F = fresnelSchlickRoughness(NdotV, F0, roughness);

    // calculate indirect diffuse lighting
    vec3 irr = texture(irradianceMap, N).xyz;
    vec3 ambientDiffuse = (1 - F) * irr * c_diff;

    // calculate indirect specular lighting
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilteredEnvMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 envBRDF = texture(brdfLut, vec2(NdotV, 1.0 - roughness)).rg;
    vec3 ambientSpecular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

    // calculate total indirect lighting
    float diffuseAO = 1.0 + materialData.occlusionStrength * (textureLod(occlusionTex, inUV, 0.0).r - 1.0);

    vec3 ambient = (ambientDiffuse + ambientSpecular) * diffuseAO * mix(0.3, 1.0, shadow);

    ambient = max(ambient, 0.0);

    // calculate emissive light
    vec3 emissive = materialData.emissiveFactors.xyz * texture(emissiveTex, inUV).xyz * materialData.emissiveStrength;

    // calculate final color
    vec3 color = lo + ambient + emissive;

    outFragColor = vec4(color, baseColor.a);
}
