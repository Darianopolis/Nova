#include <nova/rhi/slang/nova_Slang.slang>

struct MeshOutput
{
    float4   pos : SV_Position;
    float3 color : COLOR;
};

[shader("amplification")]
[numthreads(1, 1, 1)]
void Task()
{
    int payload = 0;
    DispatchMesh(1, 1, 1, payload);
}

[outputtopology("triangle")]
[numthreads(1, 1, 1)]
[shader("mesh")]
void Mesh(out indices uint3 triangles[1], out vertices MeshOutput vertices[3])
{
    SetMeshOutputCounts(3, 1);

    triangles[0] = uint3(0, 1, 2);

    vertices[0].pos = float4(-0.6, 0.6, 0, 1);
    vertices[0].color = float3(1, 0, 0);

    vertices[1].pos = float4(0.6, 0.6, 0, 1);
    vertices[1].color = float3(0, 1, 0);

    vertices[2].pos = float4(0, -0.6, 0, 1);
    vertices[2].color = float3(0, 0, 1);
}

[shader("fragment")]
float4 Fragment(MeshOutput input)
{
    return float4(input.color, 1);
}