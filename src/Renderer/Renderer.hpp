#pragma once

#include <VulkanBackend/VulkanBackend.hpp>

namespace pyr
{
    // struct MeshID { u32 value; };
    // struct MaterialTypeID { u32 value; };
    // struct MaterialID { u32 value; };
    // struct ObjectID { u32 value; };

    // struct Mesh
    // {
    //     u64 verticesVA;
    //     u64 indexVA;
    //     u32 indexCount;
    // };

    // struct MaterialType
    // {
    //     VkShaderEXT shader;
    //     b8 inlineData;
    // };

    // struct Material
    // {
    //     u64 data;
    // };

    // struct Object
    // {
    //     vec3 position;
    //     quat rotation;
    //     vec3 scale;
    //     u64 mesh;
    //     u64 material;
    // };

    struct Renderer
    {
        Context* ctx = {};

        Buffer vertices;
        VkPipelineLayout layout;
        Shader vertexShader;
        Shader fragmentShader;
    public:
        void Init(Context& ctx);
        // MeshID CreateMesh(
        //     const void* pVertices, u32 vertexCount, u32 vertexStride,
        //     const u32* pIndices, u32 indexCount,
        //     const void* pUserdata, usz userdataSize);
        // // MeshID CreateMesh(
        // //     VkStridedDeviceAddressRegionKHR vertices,
        // //     VkBuffer indexBuffer, VkDeviceSize indexOffset, VkIndexType indexType,  uint32_t indexCount,
        // //     VkDeviceAddress userdataAddress);
        // void DeleteMesh(MeshID);

        // MaterialTypeID CreateMaterialType(const char* pShader, b8 inlineData);
        // void DeleteMaterialType(MaterialTypeID);

        // MaterialID CreateMaterial(MaterialTypeID matTypeID, const void* pData, usz size);
        // // MaterialID CreateMaterial(MaterialTypeID matTypeID, VkDeviceAddress data);
        // void DeleteMaterial(MaterialID);

        // ObjectID CreateObject(MeshID meshID, MaterialID matID, vec3 position, quat rotation, vec3 scale);
        // void DeleteObject(ObjectID);

        // u32 RegisterTexture(VkImageView view, VkSampler sampler);
        // void UnregisterTexture(u32 textureIndex);

        // void SetCamera(vec3 position, quat rotation, f32 fov);
        void Draw(Image& target);
    };
}
