#version 460

layout(set = 0, binding = 0) uniform samplerCube skybox;

layout(location = 0) in vec3 inPos;

layout(location = 0) out vec4 outFragColor;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

vec3 sRGBToLinear(vec3 rgb)
{
    // See https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl
    return mix(pow((rgb + 0.055) * (1.0 / 1.055), vec3(2.4)),
        rgb * (1.0 / 12.92),
        lessThanEqual(rgb, vec3(0.04045)));
}

void main()
{
    gl_FragDepth = 0.0;
    vec3 color = texture(skybox, normalize(inPos)).xyz;

    outFragColor = vec4(sRGBToLinear(color), 1.0);
}
