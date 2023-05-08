#pragma once

#include <VulkanBackend/VulkanBackend.hpp>

namespace pyr
{
    enum class MeshID         : u32 {};
    enum class MaterialTypeID : u32 {};
    enum class MaterialID     : u32 {};
    enum class ObjectID       : u32 {};

    struct Vertex
    {
        vec2 position;
        vec3 color;
    };

    struct RasterPushConstants
    {
        mat4 mvp;
        u64 vertices;
        u64 material;
    };

    struct Mesh
    {
        Buffer vertices;
        Buffer indices;
        Buffer userdata;
        u32 indexCount;
    };

    struct MaterialType
    {
        Shader shader;
        b8 inlineData;
    };

    struct Material
    {
        MaterialTypeID materialTypeID;
        u64 data;
    };

    struct Object
    {
        vec3 position;
        quat rotation;
        vec3 scale;
        MeshID meshID;
        MaterialID materialID;
    };

    template<class Element, class Key>
    struct Registry
    {
        std::vector<Element> elements;
        std::vector<Key> freelist;

        std::pair<Key, Element&> Acquire()
        {
            if (freelist.empty())
            {
                auto& element = elements.emplace_back();
                return { Key(elements.size() - 1), element };
            }

            Key key = freelist.back();
            freelist.pop_back();
            return { key, Get(key) };
        }

        void Return(Key key)
        {
            freelist.push_back(key);
        }

        Element& Get(Key key)
        {
            return elements[static_cast<std::underlying_type_t<Key>>(key)];
        }
    };

    struct Renderer
    {
        Context* ctx = {};

        VkPipelineLayout layout;
        Shader vertexShader;

        vec3 viewPosition = vec3(0.f, 0.f, 0.f);
        quat viewRotation = vec3(0.f);
        f32 viewFov = glm::radians(90.f);

        Registry<Mesh, MeshID> meshes;
        Registry<Object, ObjectID> objects;
        Registry<MaterialType, MaterialTypeID> materialTypes;
        Registry<Material, MaterialID> materials;
    public:
        void Init(Context& ctx);

        MeshID CreateMesh(
            const void* pVertices, u32 vertexCount, u32 vertexStride,
            const u32* pIndices, u32 indexCount,
            const void* pUserdata, usz userdataSize);
        void DeleteMesh(MeshID);

        MaterialTypeID CreateMaterialType(const char* pShader, b8 inlineData);
        void DeleteMaterialType(MaterialTypeID);

        MaterialID CreateMaterial(MaterialTypeID matTypeID, const void* pData, usz size);
        void DeleteMaterial(MaterialID);

        ObjectID CreateObject(MeshID meshID, MaterialID matID, vec3 position, quat rotation, vec3 scale);
        void DeleteObject(ObjectID);

        // u32 RegisterTexture(VkImageView view, VkSampler sampler);
        // void UnregisterTexture(u32 textureIndex);

        void SetCamera(vec3 position, quat rotation, f32 fov);
        void Draw(Image& target);
    };
}
