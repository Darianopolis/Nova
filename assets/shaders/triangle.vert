#version 460

const vec2 positions[3] = vec2[] (
    vec2(-1, 1),
    vec2(1, 1),
    vec2(0, -1)
);

const vec3 colors[3] = vec3[] (
    vec3(1, 0, 0),
    vec3(0, 1, 0),
    vec3(0, 0, 1)
);

layout(location = 0) out vec3 color;

void main()
{
    color = colors[gl_VertexIndex];
    gl_Position = vec4(positions[gl_VertexIndex] * vec2(0.75), 0, 1);
}