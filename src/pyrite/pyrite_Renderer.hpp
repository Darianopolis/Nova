#pragma once

#include "Pyrite_Core.hpp"

namespace pyr
{
    using namespace nova::types;

    enum class MeshID         : u32 {};
    enum class MaterialTypeID : u32 {};
    enum class MaterialID     : u32 {};
    enum class ObjectID       : u32 {};
    enum class TextureID      : u32 {};

    struct RasterPushConstants
    {
        Mat4 viewProj;
        u64 objectsVA;
    };

    struct Mesh
    {
        nova::BufferRef vertices;
        nova::BufferRef indices;
        u32 indexCount;
        u64 vertexOffset;
        u32 vertexStride;

        nova::BufferRef accelBuffer;
        VkAccelerationStructureKHR accelStructure;
        u64 accelAddress;
    };

    struct MaterialType
    {
        nova::ShaderRef fragmentShader;
        nova::ShaderRef closestHitShader;
        nova::ShaderRef anyHitShader;
        u32 sbtOffset;
        bool inlineData;
    };

    struct Material
    {
        MaterialTypeID materialTypeID;
        nova::BufferRef buffer;
        u64 data;
    };

    struct Texture
    {
        u32 index;
    };

    struct Object
    {
        Vec3 position;
        Quat rotation;
        Vec3 scale;
        MeshID meshID;
        MaterialID materialID;
    };

    struct GpuObject
    {
        Mat4 matrix;
        u64 vertices;
        u64 material;
        u64 indices;
        u64 vertexOffset;
        u32 vertexStride;
    };

    template<class Element, class Key>
    struct Registry
    {
        enum class ElementFlag : u8
        {
            Empty = 0,
            Exists = 1,
        };
        std::vector<ElementFlag> flags;
        std::vector<Element> elements;
        std::vector<Key> freelist;

        std::pair<Key, Element&> Acquire()
        {
            if (freelist.empty())
            {
                auto& element = elements.emplace_back();
                flags.push_back(ElementFlag::Exists);
                return { Key(elements.size() - 1), element };
            }

            Key key = freelist.back();
            freelist.pop_back();
            flags[static_cast<std::underlying_type_t<Key>>(key)] = ElementFlag::Exists;
            return { key, Get(key) };
        }

        void Return(Key key)
        {
            flags[static_cast<std::underlying_type_t<Key>>(key)] = ElementFlag::Empty;
            freelist.push_back(key);
        }

        Element& Get(Key key)
        {
            return elements[static_cast<std::underlying_type_t<Key>>(key)];
        }

        template<class Fn>
        void ForEach(Fn&& visit)
        {
            for (u32 i = 0; i < elements.size(); ++i)
            {
                if (flags[i] == ElementFlag::Exists)
                    visit(Key(i), elements[i]);
            }
        }

        u32 GetCount()
        {
            return u32(elements.size() - freelist.size());
        }
    };

    struct RayTracePC
    {
        alignas(16) Vec3 pos;
        alignas(16) Vec3 camX;
        alignas(16) Vec3 camY;
        f32 camZOffset;
        u64 objectsVA;
        f32 rayConeGradient;
    };

    struct Renderer
    {
        static constexpr u32 MaxInstances = 65'536;
        static constexpr VkDeviceSize AccelScratchAlignment = 128;

        static constexpr u32 MaxMaterialTypes = 1024;

        static constexpr u32 MaxMaterials = 65'536;
        static constexpr u32 MaterialSize = 64;
    public:
        nova::Context* ctx = {};

        nova::ImageRef depthBuffer;
        // Image accumImage;
        Vec3U lastExtent;

        VkPipelineLayout layout = {};
        nova::ShaderRef vertexShader;

        nova::ShaderRef compositeShader;
        VkPipelineLayout compositePipelineLayout = {};
        VkDescriptorSetLayout compositeDescriptorLayout = {};

        nova::BufferRef materialBuffer;

        VkDescriptorSetLayout textureDescriptorSetLayout = {};
        VkDescriptorSet textureDescriptorSet = {};
        nova::BufferRef textureDescriptorBuffer;

        nova::BufferRef tlasScratchBuffer;
        nova::BufferRef tlasBuffer;
        VkAccelerationStructureKHR tlas = {};
        nova::BufferRef instanceBuffer;

        nova::ShaderRef rayGenShader;
        nova::ShaderRef rayMissShader;
        VkDescriptorSetLayout rtDescLayout = {};
        VkPipelineLayout rtPipelineLayout = {};
        VkPipeline rtPipeline = {};
        nova::BufferRef sbtBuffer;
        VkStridedDeviceAddressRegionKHR rayGenRegion = {};
        VkStridedDeviceAddressRegionKHR rayMissRegion = {};
        VkStridedDeviceAddressRegionKHR rayHitRegion = {};
        VkStridedDeviceAddressRegionKHR rayCallRegion = {};

        Registry<Mesh, MeshID> meshes;
        Registry<Object, ObjectID> objects;
        Registry<MaterialType, MaterialTypeID> materialTypes;
        Registry<Material, MaterialID> materials;
        Registry<Texture, TextureID> textures;

        nova::BufferRef objectBuffer;

        bool rayTrace = false;

        Vec3 viewPosition = Vec3(0.f, 0.f, 0.f);
        Quat viewRotation = Vec3(0.f);
        f32 viewFov = glm::radians(90.f);

        f32 rayConeGradient = 0.0005f;
    public:
        void Init(nova::Context& ctx);

        MeshID CreateMesh(
            usz dataSize, const void* pData,
            u32 vertexStride, usz vertexOffset,
            u32 indexCount, const u32* pIndices);
        void DeleteMesh(MeshID);

        MaterialTypeID CreateMaterialType(const char* pShader, bool inlineData);
        void DeleteMaterialType(MaterialTypeID);

        MaterialID CreateMaterial(MaterialTypeID matTypeID, const void* pData, usz size);
        void DeleteMaterial(MaterialID);

        ObjectID CreateObject(MeshID meshID, MaterialID matID, Vec3 position, Quat rotation, Vec3 scale);
        void DeleteObject(ObjectID);

        TextureID RegisterTexture(VkImageView view, VkSampler sampler);
        void UnregisterTexture(TextureID);

        void RebuildShaderBindingTable();

        void SetCamera(Vec3 position, Quat rotation, f32 fov);
        void Draw(nova::Image& target);
    };
}
