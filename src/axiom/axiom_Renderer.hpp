#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

namespace nova
{
    enum class AxiomMesh { Null };
    enum class AxiomMaterialShader { Null };
    enum class AxiomMaterialData { Null };
    enum class AxiomTransform { Null };
    enum class AxiomTexture { Null };

    struct AxiomDataRange
    {
        void* data;
        usz   size;
    };

    class AxiomRenderer
    {
        u64 currentFrame;

    public:
        AxiomMesh CreateMesh();
        void SetMeshVertexData(AxiomMesh, AxiomDataRange, u64 posOffset, u32 posStride);
        void SetMeshIndexData(AxiomMesh, AxiomDataRange);
        void DestroyMesh(AxiomMesh);
        
        AxiomMaterialShader CreateMaterialShader();
        void SetMaterialShaders(AxiomMaterialShader, Shader raster);
        void DestroyMaterialShader(AxiomMaterialShader);

        AxiomMaterialData CreateMaterialData();
        void SetMaterialData(AxiomMaterialData, AxiomMaterialShader, AxiomDataRange);
        void DestroyMaterialData(AxiomMaterialData);

        AxiomTransform CreateTransform();
        void SetTransformLocal(AxiomTransform, Trs);
        void SetTransformParent(AxiomTransform, AxiomTransform parent);
        void DestroyTransform(AxiomTransform);

        AxiomTexture CreateTexture();
        void SetTexture(AxiomTexture, Texture, Shader);
        void DestroyTexture(AxiomTexture);
    };
}