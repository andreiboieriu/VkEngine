#version 460

// layout(set = 0, binding = 0) uniform samplerCube skybox;
layout(set = 0, binding = 0) uniform sampler2D skybox;

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

void main() 
{
    // gl_FragDepth = 0.0;
    vec2 uv = SampleSphericalMap(normalize(inPos));
    vec3 color = texture(skybox, uv).rgb;

	outFragColor = vec4(color, 1.0);
}
