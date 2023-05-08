#version 460

#extension GL_EXT_fragment_shader_barycentric : require

layout(location = 0) in pervertexEXT uint vertexIndex[3];
layout(location = 1) in vec3 color;

layout(location = 0) out vec4 fragColor;

const vec3 c[3] = vec3[] (
    vec3(1, 0, 0),
    vec3(0, 1, 0),
    vec3(0, 0, 1)
);

void main()
{
    vec3 w = gl_BaryCoordEXT;
    uvec3 i = uvec3(vertexIndex[0], vertexIndex[1], vertexIndex[2]);
    vec3 o = (c[i.x % 3] * w.x) + (c[i.y % 3] * w.y) + (c[i.z % 3] * w.z);
    fragColor = vec4(o, 1.0);
}