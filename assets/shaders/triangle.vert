#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64  : require
#extension GL_EXT_buffer_reference2 : require

struct Vertex
{
    vec2 position;
    vec3 color;
};
layout(buffer_reference, scalar) buffer VertexBR { Vertex data[]; };

layout(push_constant) uniform PushConstants
{
    mat4 viewProj;
    uint64_t vertexVA;
} pc;

layout(location = 0) out uint outVertexIndex;
layout(location = 1) out vec3 color;

void main()
{
    Vertex v = VertexBR(pc.vertexVA).data[gl_VertexIndex];
    color = v.color;
    outVertexIndex = gl_VertexIndex;
    gl_Position = pc.viewProj * vec4(v.position, 0, 1);
}