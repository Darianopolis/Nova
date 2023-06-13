#include <nova/rhi/nova_RHI.hpp>
#include <nova/rhi/nova_RHI_Impl.hpp>

#include <nova/imgui/nova_ImGui.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#pragma warning(push)
#pragma warning(disable: 4245)
#include <micromesh/micromesh_displacement_remeshing.h>
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable: 4189)
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#pragma warning(pop)

using namespace nova::types;

static Mat4 ProjInfReversedZRH(f32 fovY, f32 aspectWbyH, f32 zNear)
{
    // https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/

    f32 f = 1.f / glm::tan(fovY / 2.f);
    Mat4 proj{};
    proj[0][0] = f / aspectWbyH;
    proj[1][1] = f;
    proj[3][2] = zNear; // Right, middle-bottom
    proj[2][3] = -1.f;  // Bottom, middle-right
    return proj;
}

template<> struct fastgltf::ElementTraits<glm::vec3> : fastgltf::ElementTraitsBase<f32, fastgltf::AccessorType::Vec3> {};
template<> struct fastgltf::ElementTraits<glm::vec2> : fastgltf::ElementTraitsBase<f32, fastgltf::AccessorType::Vec2> {};

struct Vertex
{
    Vec3 pos;
    Vec2 uv;
    u32 texIndex;
};

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<u32> indices;

    // nova::Buffer::Arc vertexBuffer;
    // nova::Buffer::Arc indexBuffer;
    u64 vertexOffset = 0;
    u64 indexOffset = 0;

    nova::AccelerationStructure::Arc blas;

    // ~Mesh() { NOVA_DEBUG(); }
};

struct MeshInstance
{
    Mat4 transform;
    std::shared_ptr<Mesh> mesh;

    // ~MeshInstance() { NOVA_DEBUG(); }
};

// ankerl::unordered_dense::map<usz, std::shared_ptr<Mesh>> loadedMeshes;
u32 meshOffset = 0;
std::vector<std::shared_ptr<Mesh>> loadedMeshes;

void ProcessNode(fastgltf::Asset& asset, nova::Context context, Mat4 parentTransform, usz nodeId, std::vector<MeshInstance>& outputMeshes)
{
    auto& node = asset.nodes[nodeId];

    Mat4 hierarchyTransform;
    Mat4 instanceTransform;
    if (auto trs = std::get_if<fastgltf::Node::TRS>(&node.transform))
    {
        // NOVA_LOG(" - Has TRS");
        auto r = glm::mat4_cast(Quat(trs->rotation[3], trs->rotation[0], trs->rotation[1], trs->rotation[2]));
        // auto r = glm::mat4_cast(Quat(trs->rotation[0], trs->rotation[1], trs->rotation[2], trs->rotation[3]));
        auto t = glm::translate(Mat4(1.f), std::bit_cast<glm::vec3>(trs->translation));
        auto s = glm::scale(Mat4(1.f), std::bit_cast<glm::vec3>(trs->scale));
        // auto s = glm::scale(Mat4(1.f), Vec3(1.f));
        hierarchyTransform = parentTransform * (t * r * s);
        instanceTransform = parentTransform * (t * r * s);
    }
    else if (auto tform = std::get_if<fastgltf::Node::TransformMatrix>(&node.transform))
    {
        // NOVA_LOG(" - Has Transform");
        auto glmTform = std::bit_cast<Mat4>(*tform);
        instanceTransform = glmTform * parentTransform;
        hierarchyTransform = instanceTransform;
    }
    else
    {
        instanceTransform = parentTransform;
        hierarchyTransform = parentTransform;
    }

    if (node.meshIndex.has_value())
    {
        auto& meshInstance =  outputMeshes.emplace_back();
        meshInstance.transform = instanceTransform;
        meshInstance.mesh = loadedMeshes[meshOffset + node.meshIndex.value()];
    }

    for (usz child : node.children)
    {
        ProcessNode(asset, context, hierarchyTransform, child, outputMeshes);
    }
}

void LoadGltf(const std::filesystem::path& path, nova::Context context, std::vector<MeshInstance>& outputMeshes)
{
    fastgltf::Parser parser(
        fastgltf::Extensions::KHR_texture_transform
        | fastgltf::Extensions::KHR_texture_basisu
        | fastgltf::Extensions::MSFT_texture_dds
        | fastgltf::Extensions::KHR_mesh_quantization
        | fastgltf::Extensions::EXT_meshopt_compression
        | fastgltf::Extensions::KHR_lights_punctual
        | fastgltf::Extensions::EXT_texture_webp
        | fastgltf::Extensions::KHR_materials_specular
        | fastgltf::Extensions::KHR_materials_ior
        | fastgltf::Extensions::KHR_materials_iridescence
        | fastgltf::Extensions::KHR_materials_volume
        | fastgltf::Extensions::KHR_materials_transmission
        | fastgltf::Extensions::KHR_materials_clearcoat
        | fastgltf::Extensions::KHR_materials_emissive_strength
        | fastgltf::Extensions::KHR_materials_sheen
        | fastgltf::Extensions::KHR_materials_unlit
    );
    fastgltf::GltfDataBuffer data;
    NOVA_TIMEIT_RESET();
    NOVA_LOG("Loading glTF file into memory: {}", path.string());
    data.loadFromFile(path);
    NOVA_TIMEIT("loading-gltf");

    constexpr auto gltfOptions =
        fastgltf::Options::DontRequireValidAssetMember
        | fastgltf::Options::AllowDouble
        | fastgltf::Options::LoadGLBBuffers
        | fastgltf::Options::LoadExternalBuffers
        | fastgltf::Options::LoadExternalImages;

    auto type = fastgltf::determineGltfFileType(&data);
    std::unique_ptr<fastgltf::glTF> gltf;
    NOVA_LOG("Loading file data");
    if (type == fastgltf::GltfType::glTF) {
        gltf = parser.loadGLTF(&data, path.parent_path(), gltfOptions);
    } else if (type == fastgltf::GltfType::GLB) {
        gltf = parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
    } else {
        NOVA_THROW("Failed to determine glTF container");
    }
    NOVA_TIMEIT("loading-gltf-data");

    if (parser.getError() != fastgltf::Error::None)
        NOVA_THROW("Failed to load glTF: {}", fastgltf::to_underlying(parser.getError()));

    NOVA_LOG("Parsing gltf model");
    if (auto res = gltf->parse(fastgltf::Category::Scenes); res != fastgltf::Error::None)
        NOVA_THROW("Failed to parse glTF: {}", fastgltf::to_underlying(res));
    NOVA_TIMEIT("parsing-gltf");

    auto pAsset = gltf->getParsedAsset();
    auto& asset = *pAsset;
    NOVA_TIMEIT("generated-asset");

    meshOffset = u32(loadedMeshes.size());

    NOVA_LOG("Loading meshes...");
    for (auto& inMesh : asset.meshes)
    {
        auto& pOutMesh = loadedMeshes.emplace_back();
        pOutMesh = std::make_shared<Mesh>();
        auto& outMesh = *pOutMesh;

        for (auto& prim : inMesh.primitives)
        {
            if (prim.attributes.find("POSITION") == prim.attributes.end())
                continue;

            if (!prim.indicesAccessor.has_value())
                continue;

            auto& positionAccessor = asset.accessors[prim.attributes["POSITION"]];
            if (!positionAccessor.bufferViewIndex.has_value())
                continue;

            auto& indexAccessor = asset.accessors[prim.indicesAccessor.value()];

            u32 offset = u32(outMesh.vertices.size());
            outMesh.indices.reserve(outMesh.indices.size() + indexAccessor.count);
            fastgltf::iterateAccessor<u32>(asset, indexAccessor, [&](u32 index) {
                outMesh.indices.push_back(offset + index);
            });

            outMesh.vertices.resize(outMesh.vertices.size() + positionAccessor.count);
            u32 vid = offset;
            fastgltf::iterateAccessor<glm::vec3>(asset, positionAccessor, [&](glm::vec3 pos) {
                outMesh.vertices[vid++].pos = pos;
            }, fastgltf::DefaultBufferDataAdapter{});

            if (prim.attributes.find("TEXCOORD_0") != prim.attributes.end())
            {
                auto& texCoordAccessor = asset.accessors[prim.attributes.at("TEXCOORD_0")];

                vid = offset;
                fastgltf::iterateAccessor<glm::vec2>(asset, texCoordAccessor, [&](glm::vec2 uv) {
                    outMesh.vertices[vid++].uv = uv;
                }, fastgltf::DefaultBufferDataAdapter{});
            }
        }
    }
    NOVA_TIMEIT("loaded-meshes");

    ankerl::unordered_dense::set<usz> rootNodes;
    for (usz i = 0; i < asset.nodes.size(); ++i)
        rootNodes.insert(i);

    for (auto& node : asset.nodes)
    {
        for (auto& child : node.children)
            rootNodes.erase(child);
    }
    NOVA_TIMEIT("preprocessed nodes");

    for (auto& root : rootNodes)
    {
        // NOVA_LOG("Root node: {} - {}", root, asset->nodes[root].name);
        ProcessNode(asset, context, Mat4(1.f), root, outputMeshes);
    }
    NOVA_TIMEIT("processed nodes");
}

void TryMain()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1200, "Hello Nova RT Triangle", nullptr, nullptr);
    NOVA_ON_SCOPE_EXIT(&) {
        glfwDestroyWindow(window);
        glfwTerminate();
    };

    auto context = +nova::Context({
        .debug = false,
        .rayTracing = true,
    });

    auto surface = +nova::Surface(context, glfwGetWin32Window(window));
    auto swapchain = +nova::Swapchain(context, surface,
        nova::TextureUsage::ColorAttach
        | nova::TextureUsage::TransferDst
        | nova::TextureUsage::Storage,
        nova::PresentMode::Immediate);

    auto queue = context.GetQueue(nova::QueueFlags::Graphics);
    auto cmdPool = +nova::CommandPool(context, queue);
    auto fence = +nova::Fence(context);
    auto state = +nova::CommandState(context);

    nova::ImGuiWrapper::Arc imgui;
    {
        auto cmd = cmdPool.Begin(state);
        imgui = +nova::ImGuiWrapper(context, cmd, swapchain.GetFormat(), window, { .flags = ImGuiConfigFlags_ViewportsEnable });
        queue.Submit({cmd}, {}, {fence});
        fence.Wait();
    }

    // Vertices

    // std::vector<Vertex> vertices;
    // std::vector<u32> indices;

    // vertices.push_back({{-0.6f, 0.6f, 0.f}, {1.f, 0.f, 0.f}});
    // vertices.push_back({{ 0.6f, 0.6f, 0.f}, {0.f, 1.f, 0.f}});
    // vertices.push_back({{ 0.f, -0.6f, 0.f}, {0.f, 0.f, 1.f}});

    // indices.push_back(0);
    // indices.push_back(1);
    // indices.push_back(2);

    std::vector<MeshInstance> meshes;
    LoadGltf("D:/Dev/Projects/pyrite/pyrite-v4/assets/models/monkey.gltf", context, meshes);

    LoadGltf(R"(D:\Dev\Projects\pyrite\pyrite-v4\assets\models\SponzaMain\NewSponza_Main_Blender_glTF.gltf)", context, meshes);
    LoadGltf(R"(D:\Dev\Projects\pyrite\pyrite-v4\assets\models\SponzaCurtains\NewSponza_Curtains_glTF.gltf)", context, meshes);
    LoadGltf(R"(D:\Dev\Projects\pyrite\pyrite-v4\assets\models\SponzaIvy\NewSponza_IvyGrowth_glTF.gltf)", context, meshes);

    LoadGltf("D:/Dev/Data/3DModels/Small_City_LVL/Small_City_LVL.gltf", context, meshes);

    LoadGltf(R"(D:\Dev\Projects\pyrite\pyrite-v4\assets\models\deccer-cubes\SM_Deccer_Cubes_Textured_Complex.gltf)", context, meshes);

    LoadGltf(R"(D:\Dev\Data\3DModels\MoanaIsland\island-basepackage-v1.1\island\gltf\isIronwoodA1\isIronwoodA1.gltf)", context, meshes);
    LoadGltf(R"(D:\Dev\Data\3DModels\MoanaIsland\island-basepackage-v1.1\island\gltf\isBeach\isBeach.gltf)", context, meshes);
    LoadGltf(R"(D:\Dev\Data\3DModels\MoanaIsland\island-basepackage-v1.1\island\gltf\isBayCedarA1\isBayCedarA1.gltf)", context, meshes);
    LoadGltf(R"(D:\Dev\Data\3DModels\MoanaIsland\island-basepackage-v1.1\island\gltf\isMountainA\isMountainA.gltf)", context, meshes);
    LoadGltf(R"(D:\Dev\Data\3DModels\MoanaIsland\island-basepackage-v1.1\island\gltf\isMountainB\isMountainB.gltf)", context, meshes);

    // {
    //     auto& mesh = loadedMeshes.emplace_back(std::make_shared<Mesh>());
    //     mesh->vertices = { {{0.f, -0.5f, 0.f}}, {{-0.5f, 0.5f, 0.f}}, {{0.5f, 0.5f, 0.f}} };
    //     mesh->indices = { 0, 1, 2 };
    //     mesh->vertexOffset = 0;
    //     mesh->indexOffset = 0;

    //     meshes.push_back({.transform = Mat4(1.f), .mesh = mesh});
    // }

    NOVA_ON_SCOPE_EXIT(&) { loadedMeshes.clear(); };

    nova::Buffer::Arc vertexBuffer;
    nova::Buffer::Arc indexBuffer;
    {
        usz vertexCount = 0;
        usz indexCount = 0;

        for (auto& mesh : loadedMeshes)
        {
            vertexCount += mesh->vertices.size();
            indexCount += mesh->indices.size();
        }

        NOVA_LOGEXPR(vertexCount);
        NOVA_LOGEXPR(indexCount);

        vertexBuffer = +nova::Buffer(context, vertexCount * sizeof(Vertex),
            nova::BufferUsage::Storage | nova::BufferUsage::AccelBuild,
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::CreateMapped);

        indexBuffer = +nova::Buffer(context, indexCount * 4,
            nova::BufferUsage::Index | nova::BufferUsage::AccelBuild,
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::CreateMapped);

        vertexCount = 0;
        indexCount = 0;

        for (auto& mesh : loadedMeshes)
        {
            mesh->vertexOffset = vertexCount;
            mesh->indexOffset = indexCount;

            vertexBuffer.Set<Vertex>(mesh->vertices, vertexCount);
            indexBuffer.Set<u32>(mesh->indices, indexCount);

            vertexCount += mesh->vertices.size();
            indexCount += mesh->indices.size();
        }
    }

    {
        usz totalIndexCount = 0;

        for (auto& instance : meshes)
            totalIndexCount += instance.mesh->indices.size();

        NOVA_LOG("Instances = {}", meshes.size());
        NOVA_LOG("Unique meshes = {}", loadedMeshes.size());
        NOVA_LOG("Total Triangles = {}", totalIndexCount / 3);
    }

    auto builder = +nova::AccelerationStructureBuilder(context);

    for (auto& mesh : loadedMeshes)
    {
        NOVA_LOGEXPR(mesh->vertexOffset);
        NOVA_LOGEXPR(mesh->indexOffset);
        builder.SetTriangles(0,
            vertexBuffer.GetAddress() + mesh->vertexOffset * sizeof(Vertex), nova::Format::RGB32F, u32(sizeof(Vertex)), u32(mesh->vertices.size()) - 1,
            indexBuffer.GetAddress() + mesh->indexOffset * 4, nova::IndexType::U32, u32(mesh->indices.size()) / 3);

        builder.Prepare(
            nova::AccelerationStructureType::BottomLevel,
            nova::AccelerationStructureFlags::PreferFastTrace
            | nova::AccelerationStructureFlags::AllowCompaction, 1);
        NOVA_LOG("Size = {}, Scratch = {}", builder.GetBuildSize(), builder.GetBuildScratchSize());

        auto scratch = +nova::Buffer(context, builder.GetBuildScratchSize(), nova::BufferUsage::Storage, nova::BufferFlags::DeviceLocal);
        auto blas = +nova::AccelerationStructure(context, builder.GetBuildSize(), nova::AccelerationStructureType::BottomLevel);

        auto cmd = cmdPool.Begin(state);
        cmd.BuildAccelerationStructure(builder, blas, scratch);
        queue.Submit({cmd}, {}, {fence});
        fence.Wait();

        auto compactedSize = builder.GetCompactSize();
        mesh->blas = +nova::AccelerationStructure(context, compactedSize, nova::AccelerationStructureType::BottomLevel);
        NOVA_LOG("  Compacted Size = {}", compactedSize);

        cmd = cmdPool.Begin(state);
        cmd.CompactAccelerationStructure(mesh->blas, blas);
        queue.Submit({cmd}, {}, {fence});
        fence.Wait();

        // break;
    }

    nova::Buffer::Arc instances;
    nova::AccelerationStructure::Arc tlas;
    {
NOVA_DEBUG();
        instances = +nova::Buffer(context, 100'000 * builder.GetInstanceSize(), // meshes.size() * builder.GetInstanceSize(),
            nova::BufferUsage::Storage | nova::BufferUsage::AccelBuild,
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::CreateMapped);
NOVA_DEBUG();

        for (u32 i = 0; i < meshes.size(); ++i)
            builder.WriteInstance(instances.GetMapped(), i, meshes[i].mesh->blas, meshes[i].transform, i, 0xFF, 0, {});
NOVA_DEBUG();

        builder.SetInstances(0, instances.GetAddress(), u32(meshes.size()));
        // {
        //     u32 i = 0;
        //     builder.WriteInstance(instances.GetMapped(), i, meshes[i].mesh->blas, meshes[i].transform, i, 0xFF, 0, {});
        //     i = 1;
        //     builder.WriteInstance(instances.GetMapped(), 1, meshes[i].mesh->blas, meshes[i].transform, i, 0xFF, 0, {});
        //     // NOVA_LOGEXPR(meshes[i].mesh->blas->structure);
        //     // builder.WriteInstance(instances.GetMapped(), 0, meshes[i].mesh->blas, meshes[i].transform, i, 0xFF, 0, {});
        //     builder.SetInstances(0, instances.GetAddress(), u32(2));
        // }
NOVA_DEBUG();
        builder.Prepare(nova::AccelerationStructureType::TopLevel, nova::AccelerationStructureFlags::PreferFastTrace, 1);
NOVA_DEBUG();

        auto scratch = +nova::Buffer(context, builder.GetBuildScratchSize(), nova::BufferUsage::Storage, nova::BufferFlags::DeviceLocal);
NOVA_DEBUG();
        tlas = +nova::AccelerationStructure(context, builder.GetBuildSize(), nova::AccelerationStructureType::TopLevel);
NOVA_DEBUG();

        auto cmd = cmdPool.Begin(state);
NOVA_DEBUG();
        cmd.BuildAccelerationStructure(builder, tlas, scratch);
NOVA_DEBUG();
        queue.Submit({cmd}, {}, {fence});
NOVA_DEBUG();
        fence.Wait();
NOVA_DEBUG();
    }

    // // nova::AccelerationStructure::Arc blas;
    // nova::AccelerationStructure::Arc tlas;
    // {
    //     // Vertex data

    //     // auto vertices = +nova::Buffer(context, 3 * sizeof(Vec3),
    //     //     nova::BufferUsage::AccelBuild,
    //     //     nova::BufferFlags::DeviceLocal | nova::BufferFlags::CreateMapped);
    //     // vertices.Set<Vec3>({ {0.f, -0.5f, 0.f}, {-0.5f, 0.5f, 0.f}, {0.5f, 0.5f, 0.f} });

    //     // Index data

    //     // auto indices = +nova::Buffer(context, 3 * sizeof(u32),
    //     //     nova::BufferUsage::AccelBuild,
    //     //     nova::BufferFlags::DeviceLocal | nova::BufferFlags::CreateMapped);
    //     // indices.Set<u32>({ 0u, 1u, 2u });

    //     // Configure BLAS build


    //     // auto& mesh = meshes[0].mesh;

    //     // builder.SetTriangles(0,
    //     //     // vertices.GetAddress(), nova::Format::RGB32F, u32(sizeof(Vec3)), 2,
    //     //     // indices.GetAddress(), nova::IndexType::U32, 1);
    //     //     vertexBuffer.GetAddress() + mesh->vertexOffset * sizeof(Vertex), nova::Format::RGB32F, u32(sizeof(Vertex)), u32(mesh->vertices.size()) - 1,
    //     //     indexBuffer.GetAddress() + mesh->indexOffset * 4, nova::IndexType::U32, u32(mesh->indices.size()) / 3);
    //     // builder.Prepare(nova::AccelerationStructureType::BottomLevel,
    //     //     nova::AccelerationStructureFlags::PreferFastTrace, 1);

    //     // // Create BLAS and scratch buffer

    //     // blas = +nova::AccelerationStructure(context, builder.GetBuildSize(),
    //     //     nova::AccelerationStructureType::BottomLevel);
    //     // auto scratch = +nova::Buffer(context, builder.GetBuildScratchSize(),
    //     //     nova::BufferUsage::Storage, nova::BufferFlags::DeviceLocal);

    //     // // Build BLAS

    //     // auto cmd = cmdPool.Begin(state);
    //     // cmd.BuildAccelerationStructure(builder, blas, scratch);
    //     // queue.Submit({cmd}, {}, {fence});
    //     // fence.Wait();

    //     auto blas = meshes[0].mesh->blas;

    //     auto scratch = +nova::Buffer(context, 256,
    //         nova::BufferUsage::Storage, nova::BufferFlags::DeviceLocal);

    // // -----------------------------------------------------------------------------
    // //                                Scene TLAS
    // // -----------------------------------------------------------------------------

    //     // Instance data

    //     auto instances = +nova::Buffer(context, builder.GetInstanceSize(),
    //         nova::BufferUsage::AccelBuild,
    //         nova::BufferFlags::DeviceLocal | nova::BufferFlags::CreateMapped);

    //     // Configure TLAS build

    //     builder.SetInstances(0, instances.GetAddress(), 1);
    //     builder.Prepare(nova::AccelerationStructureType::TopLevel,
    //         nova::AccelerationStructureFlags::PreferFastTrace, 1);

    //     // Create TLAS and resize scratch buffer

    //     tlas = +nova::AccelerationStructure(context, builder.GetBuildSize(),
    //         nova::AccelerationStructureType::TopLevel);
    //     scratch.Resize(builder.GetBuildScratchSize());

    //     builder.WriteInstance(instances.GetMapped(), 0, blas,
    //         // glm::scale(Mat4(1), Vec3(swapchain.GetExtent(), 1.f)),
    //         Mat4(1.f),
    //         0, 0xFF, 0, {});

    //     auto cmd = cmdPool.Begin(state);
    //     cmd.BuildAccelerationStructure(builder, tlas, scratch);
    //     queue.Submit({cmd}, {}, {fence});
    //     fence.Wait();
    // }

    struct RayTracePC
    {
        alignas(16) Vec3 pos;
        alignas(16) Vec3 camX;
        alignas(16) Vec3 camY;
        f32 camZOffset;
        u64 objectsVA;
    };

//     auto rtPipelineLayout = +nova::PipelineLayout(context,
//         {{nova::ShaderStage::RayGen, sizeof(RayTracePC)}},
//         {descLayout},
//         nova::BindPoint::RayTracing);

//     auto rayGenShader = +nova::Shader(context,
//         nova::ShaderStage::RayGen,
//         "raygen",
//         R"(
// #version 460

// #extension GL_EXT_scalar_block_layout                     : require
// #extension GL_EXT_shader_explicit_arithmetic_types_int64  : require
// #extension GL_EXT_buffer_reference2                       : require
// #extension GL_EXT_ray_tracing                             : require
// #extension GL_NV_shader_invocation_reorder                : require

// layout(set = 0, binding = 0, rgba8) uniform image2D       outImage;
// layout(set = 0, binding = 1) uniform accelerationStructureEXT tlas;

// layout(location = 0) rayPayloadEXT uint            payload;
// layout(location = 0) hitObjectAttributeNV vec3 barycentric;

// layout(push_constant) uniform PushConstants
// {
//     vec3 pos;
//     vec3 camX;
//     vec3 camY;
//     float camZOffset;
//     uint64_t objectsVA;
// } pc;

// void main()
// {
//     // vec3 pos = vec3(vec2(gl_LaunchIDEXT.xy), 1);
//     // vec3 dir = vec3(0, 0, -1);

//     // vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy);
//     // pixelCenter += vec2(0.5);
//     // vec2 inUV = pixelCenter / vec2(gl_LaunchSizeEXT.xy);
//     // vec2 d = inUV * 2.0 - 1.0;
//     // vec3 focalPoint = pc.camZOffset * cross(pc.camX, pc.camY);
//     // vec3 pos = pc.pos;
//     // d.x *= float(gl_LaunchSizeEXT.x) / float(gl_LaunchSizeEXT.y);
//     // d.y *= -1.0;
//     // vec3 dir = normalize((pc.camY * d.y) + (pc.camX * d.x) - focalPoint);

//     vec3 pos = vec3(0, 0, 1);
//     vec3 dir = vec3(0, 0, -1);

//     uint rayFlags = 0;

//     hitObjectNV hit;
//     hitObjectTraceRayNV(hit, tlas, rayFlags, 0xFF, 0, 0, 0, pos, 0.0001, dir, 8000000, 0);

//     if (hitObjectIsHitNV(hit))
//     {
//         // hitObjectGetAttributesNV(hit, 0);
//         // vec3 w = vec3(1.0 - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
//         // vec3 color = vec3(1, 0, 0) * w.x + vec3(0, 1, 0) * w.y + vec3(0, 0, 1) * w.z;
//         // imageStore(outImage, ivec2(gl_LaunchIDEXT.xy), vec4(w, 1));

//         imageStore(outImage, ivec2(gl_LaunchIDEXT.xy), vec4(1));
//     }
//     else
//     {
//         // imageStore(outImage, ivec2(gl_LaunchIDEXT.xy), vec4(vec3(0.1), 1));
//         imageStore(outImage, ivec2(gl_LaunchIDEXT.xy), vec4((dir * 0.5 + 0.5) * 0.3, 1));
//     }

//     // vec2 uv = vec2(gl_LaunchIDEXT.xy) / vec2(gl_LaunchSizeEXT.xy);
//     // imageStore(outImage, ivec2(gl_LaunchIDEXT.xy), vec4(uv, 0.5, 1));

// }
//         )");

//     // Create a ray tracing pipeline with one ray gen shader

//     auto pipeline = +nova::RayTracingPipeline(context);
//     pipeline.Update(rtPipelineLayout, {rayGenShader}, {}, {}, {});

// Create descriptor layout to hold one storage image and acceleration structure

    auto descLayout = +nova::DescriptorSetLayout(context, {
        {nova::DescriptorType::StorageTexture},
        {nova::DescriptorType::AccelerationStructure},
    }, true);

    // Create a pipeline layout for the above set layout

    auto rtPipelineLayout = +nova::PipelineLayout(context, {{nova::ShaderStage::RayGen, sizeof(RayTracePC)}}, {descLayout}, nova::BindPoint::RayTracing);

    // Create the ray gen shader to draw a shaded triangle based on barycentric interpolation

    auto rayGenShader = +nova::Shader(context,
        nova::ShaderStage::RayGen,
        "raygen",
        R"(
#version 460

#extension GL_EXT_scalar_block_layout                     : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64  : require
#extension GL_EXT_buffer_reference2                       : require
#extension GL_EXT_ray_tracing              : require
#extension GL_NV_shader_invocation_reorder : require

layout(set = 0, binding = 0, rgba8) uniform image2D       outImage;
layout(set = 0, binding = 1) uniform accelerationStructureEXT tlas;

layout(location = 0) rayPayloadEXT uint            payload;
layout(location = 0) hitObjectAttributeNV vec3 barycentric;

layout(push_constant) uniform PushConstants
{
    vec3 pos;
    vec3 camX;
    vec3 camY;
    float camZOffset;
    uint64_t objectsVA;
} pc;

void main()
{
    // vec3 pos = vec3(vec2(gl_LaunchIDEXT.xy), 1);
    // vec3 dir = vec3(0, 0, -1);

    vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy);
    pixelCenter += vec2(0.5);
    vec2 inUV = pixelCenter / vec2(gl_LaunchSizeEXT.xy);
    vec2 d = inUV * 2.0 - 1.0;
    vec3 focalPoint = pc.camZOffset * cross(pc.camX, pc.camY);
    vec3 pos = pc.pos;
    d.x *= float(gl_LaunchSizeEXT.x) / float(gl_LaunchSizeEXT.y);
    d.y *= -1.0;
    vec3 dir = normalize((pc.camY * d.y) + (pc.camX * d.x) - focalPoint);

    hitObjectNV hit;
    hitObjectTraceRayNV(hit, tlas, 0, 0xFF, 0, 0, 0, pos, 0.0001, dir, 8000000, 0);

    if (hitObjectIsHitNV(hit))
    {
        hitObjectGetAttributesNV(hit, 0);
        vec3 w = vec3(1.0 - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
        vec3 color = vec3(1, 0, 0) * w.x + vec3(0, 1, 0) * w.y + vec3(0, 0, 1) * w.z;
        imageStore(outImage, ivec2(gl_LaunchIDEXT.xy), vec4(w, 1));
    }
    else
    {
        imageStore(outImage, ivec2(gl_LaunchIDEXT.xy), vec4(vec3(0.1), 1));
    }
}
        )");

    // Create a ray tracing pipeline with one ray gen shader

    auto pipeline = +nova::RayTracingPipeline(context);
    pipeline.Update(rtPipelineLayout, {rayGenShader}, {}, {}, {});

    struct GpuMeshInstance
    {
        glm::mat4 model;
        u64 vertexVA;
    };

    auto instanceBuffer = +nova::Buffer(context, meshes.size() * sizeof(GpuMeshInstance),
        nova::BufferUsage::Storage,
        nova::BufferFlags::DeviceLocal | nova::BufferFlags::CreateMapped);

    for (u32 i = 0; i < meshes.size(); ++i)
    {
        instanceBuffer.Get<GpuMeshInstance>(i) = {
            .model = meshes[i].transform,
            .vertexVA = vertexBuffer.GetAddress() + (meshes[i].mesh->vertexOffset * sizeof(Vertex)),
        };
    }

    // auto countBuffer = +nova::Buffer(context, 4,
    //     nova::BufferUsage::Storage, nova::BufferFlags::DeviceLocal | nova::BufferFlags::CreateMapped);

    // auto indirectBuffer = +nova::Buffer(context, meshes.size() * sizeof(VkDrawIndexedIndirectCommand),
    //     nova::BufferUsage::Indirect,
    //     nova::BufferFlags::DeviceLocal | nova::BufferFlags::CreateMapped);
    struct PushConstants
    {
        // alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 viewProj;
        // u64 vertexVA;
        u64 instanceVA;
        // u64 countVA;
    };

    // Pipeline

    auto pipelineLayout = +nova::PipelineLayout(context,
        {{nova::ShaderStage::Vertex, sizeof(PushConstants)}},
        {},
        nova::BindPoint::Graphics);

    auto vertexShader = +nova::Shader(context,
        nova::ShaderStage::Vertex,
        "vertex",
        R"(
#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_atomic_int64 : require

struct Vertex
{
    vec3 pos;
    vec2 uv;
    uint texIndex;
};
layout(buffer_reference, scalar) buffer VertexBR { Vertex data[]; };

struct GpuMeshInstance
{
    mat4 model;
    uint64_t vertexVA;
};
layout(buffer_reference, scalar) buffer GpuMeshInstanceBR { GpuMeshInstance data[]; };

layout(push_constant) uniform PushConstants
{
    // mat4 model;
    mat4 viewProj;
    uint64_t instanceVA;
    // uint64_t vertexVA;
    // uint64_t countVA;
} pc;
layout(buffer_reference, scalar) buffer CountBR { uint data[]; };

// const vec2 positions[3] = vec2[] (vec2(-0.6, 0.6), vec2(0.6, 0.6), vec2(0, -0.6));
// const vec3    colors[3] = vec3[] (vec3(1, 0, 0),   vec3(0, 1, 0),  vec3(0, 0, 1));

// layout(location = 0) out vec3 color;
layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec2 outTexCoord;

void main()
{
    GpuMeshInstance instance = GpuMeshInstanceBR(pc.instanceVA).data[gl_InstanceIndex];
    // uint vertexId = atomicAdd(CountBR(pc.countVA).data[0], 1);
    // Vertex v = VertexBR(pc.vertexVA).data[vertexId];
    Vertex v = VertexBR(instance.vertexVA).data[gl_VertexIndex];
    // color = v.color;
    outPosition = vec3(instance.model * vec4(v.pos, 1.0));
    gl_Position = pc.viewProj * vec4(outPosition, 1);
    outTexCoord = v.uv;
}
        )");

    auto fragmentShader = +nova::Shader(context,
        nova::ShaderStage::Fragment,
        "fragment",
        R"(
#version 460

#extension GL_EXT_fragment_shader_barycentric : require

layout(location = 0) in pervertexEXT vec3 inPositions[3];
layout(location = 1) in pervertexEXT vec2 inTexCoord[3];
layout(location = 0) out vec4 fragColor;

void main()
{
    // fragColor = vec4(gl_BaryCoordEXT, 1.0);

    vec3 nrm = normalize(cross(inPositions[1] - inPositions[0], inPositions[2] - inPositions[0]));
    fragColor = vec4(nrm * 0.5 + 0.5, 1.0);

    // vec3 w = gl_BaryCoordEXT;
    // vec2 uv = inTexCoord[0] * w.x + inTexCoord[1] * w.y + inTexCoord[1] * w.z;
    // fragColor = vec4(uv, 0, 1);
}
        )");

    // Draw

    Vec3 position {0.f, 0.f, 1.f};
    Quat rotation = glm::angleAxis(0.f, glm::vec3{0.f, 1.f, 0.f});
    float viewFov = glm::radians(90.f);
    static f32 moveSpeed = 1.f;

    nova::Texture::Arc depthBuffer;

    // auto lastReport = std::chrono::steady_clock::now();
    // auto frames = 0;

    bool rayTracing = true;

    NOVA_ON_SCOPE_EXIT(&) { fence.Wait(); };
    while (!glfwWindowShouldClose(window))
    {
        // // Debug output statistics
        // frames++;
        // auto newTime = std::chrono::steady_clock::now();
        // if (newTime - lastReport > 1s)
        // {
        //     NOVA_LOG("\nFps = {} ({:.2f} ms)\nAllocations = {:3} (+ {} /s)",
        //         frames, (1000.0 / frames), nova::ContextImpl::AllocationCount.load(), nova::ContextImpl::NewAllocationCount.exchange(0));
        //     f64 divisor = 1000.0 * frames;
        //     NOVA_LOG("submit :: clear     = {:.2f}\nsubmit :: adapting1 = {:.2f}\nsubmit :: adapting2 = {:.2f}\npresent             = {:.2f}",
        //         nova::TimeSubmitting.exchange(0) / divisor,
        //         nova::TimeAdaptingFromAcquire.exchange(0)  / divisor,
        //         nova::TimeAdaptingToPresent.exchange(0)  / divisor,
        //         nova::TimePresenting.exchange(0) / divisor);

        //     NOVA_LOG("Atomic Operations = {}", nova::NumHandleOperations.load());

        //     lastReport = std::chrono::steady_clock::now();
        //     frames = 0;
        // }

        {
            // Handle movement

            static Vec2 lastPos = {};
            auto pos = [&] {
                double x, y;
                glfwGetCursorPos(window, &x, &y);
                return Vec2(f32(x), f32(y));
            }();
            auto delta = pos - lastPos;
            lastPos = pos;

            using namespace std::chrono;
            static steady_clock::time_point lastTime = steady_clock::now();
            auto now = steady_clock::now();
            auto timeStep = duration_cast<duration<f32>>(now - lastTime).count();
            lastTime = now;

            NOVA_DO_ONCE(&) {
                glfwSetScrollCallback(window, [](auto, f64, f64 dy) {
                    if (dy > 0)
                        moveSpeed *= 1.1f;
                    if (dy < 0)
                        moveSpeed /= 1.1f;
                });
            };

            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_2) == GLFW_PRESS)
            {
                rotation = glm::angleAxis(delta.x * 0.0025f, Vec3(0.f, -1.f, 0.f)) * rotation;
                rotation = rotation * glm::angleAxis(delta.y * 0.0025f, Vec3(-1.f, 0.f, 0.f));
                rotation = glm::normalize(rotation);
            }

            {
                glm::vec3 translate{};
                if (glfwGetKey(window, GLFW_KEY_W))          translate += Vec3( 0.f,  0.f, -1.f);
                if (glfwGetKey(window, GLFW_KEY_A))          translate += Vec3(-1.f,  0.f,  0.f);
                if (glfwGetKey(window, GLFW_KEY_S))          translate += Vec3( 0.f,  0.f,  1.f);
                if (glfwGetKey(window, GLFW_KEY_D))          translate += Vec3( 1.f,  0.f,  0.f);
                if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) translate += Vec3( 0.f, -1.f,  0.f);
                if (glfwGetKey(window, GLFW_KEY_SPACE))      translate += Vec3( 0.f,  1.f,  0.f);

                position += rotation * (translate * moveSpeed * timeStep);
            }
        }

        fence.Wait();
        cmdPool.Reset();
        auto cmd = cmdPool.Begin(state);
        if (queue.Acquire({swapchain}, {fence}))
        {
            depthBuffer = +nova::Texture(context,
                swapchain.GetCurrent().GetExtent(),
                nova::TextureUsage::DepthStencilAttach,
                nova::Format::D32);
        }

        auto proj = ProjInfReversedZRH(viewFov, f32(swapchain.GetExtent().x) / f32(swapchain.GetExtent().y), 0.01f);
        auto translate = glm::translate(Mat4(1.f), position);
        auto rotate = glm::mat4_cast(rotation);
        auto viewProj = proj * glm::affineInverse(translate * rotate);

        if (rayTracing)
        {
            // cmd.Transition(swapchain.GetCurrent(),
            //     nova::ResourceState::GeneralImage,
            //     nova::PipelineStage::RayTracing);

            // // Push swapchain image and TLAS descriptors

            // cmd.PushStorageTexture(rtPipelineLayout, 0, 0, swapchain.GetCurrent());
            // cmd.PushAccelerationStructure(rtPipelineLayout, 0, 1, tlas);

            // // Trace rays

            cmd.PushConstants(rtPipelineLayout, nova::ShaderStage::RayGen,
                0, sizeof(RayTracePC), nova::Temp(RayTracePC {
                    .pos = position,
                    .camX = rotation * Vec3(1, 0, 0),
                    .camY = rotation * Vec3(0, 1, 0),
                    .camZOffset = 1.f / glm::tan(0.5f * viewFov),
                    .objectsVA = 0,
                }));

            // cmd.BindPipeline(pipeline);
            // cmd.TraceRays(pipeline, Vec3U(swapchain.GetExtent(), 1), 0);

            // Transition ready for writing ray trace output

            cmd.Transition(swapchain.GetCurrent(),
                nova::ResourceState::GeneralImage,
                nova::PipelineStage::RayTracing);

            // Push swapchain image and TLAS descriptors

            cmd.PushStorageTexture(rtPipelineLayout, 0, 0, swapchain.GetCurrent());
            cmd.PushAccelerationStructure(rtPipelineLayout, 0, 1, tlas);

            // Trace rays

            cmd.BindPipeline(pipeline);
            cmd.TraceRays(pipeline, Vec3U(swapchain.GetExtent(), 1), 0);
        }
        else
        {
            cmd.BeginRendering({swapchain.GetCurrent()}, depthBuffer);
            cmd.ClearColor(0, Vec4(Vec3(0.1f), 1.f), swapchain.GetExtent());
            cmd.ClearDepth(0.f, swapchain.GetExtent());
            cmd.SetGraphicsState(pipelineLayout, {vertexShader, fragmentShader}, {
                .cullMode = nova::CullMode::None,
                // .cullMode = nova::CullMode::Back,
                .depthEnable = true,
                .flipVertical = true,
            });
            cmd.BindIndexBuffer(indexBuffer, nova::IndexType::U32);
            cmd.PushConstants(pipelineLayout, nova::ShaderStage::Vertex,
                0, sizeof(PushConstants), nova::Temp(PushConstants {
                    // .model = mesh.transform,
                    .viewProj = viewProj,
                    // .vertexVA = vertexBuffer.GetAddress() + (mesh.mesh->vertexOffset * sizeof(Vec3)),// mesh.mesh->vertexBuffer.GetAddress(),
                    .instanceVA = instanceBuffer.GetAddress(),
                    // .countVA = mesh.countBuffer.GetAddress(),
                }));
            // for (auto& mesh : meshes)
            // for (u32 i = 2000; i < 3000; ++i)
            for (u32 i = 0; i < meshes.size(); ++i)
            {
                auto& mesh = meshes[i];
                cmd.DrawIndexed(u32(mesh.mesh->indices.size()), 1, u32(mesh.mesh->indexOffset), 0, i);
            }
            cmd.EndRendering();
        }

        imgui.BeginFrame();
        ImGui::Checkbox("Ray tracing", &rayTracing);
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", position.x, position.y, position.z);
        ImGui::Separator();

        {
            using namespace std::chrono;
            static u32 frameCount = 0;
            static f32 fps = 0.f;
            static i64 totalAllocations = 0;
            static i64 newAllocations = 0;
            static f64 submitting = 0;
            static f64 adapting1 = 0;
            static f64 adapting2 = 0;
            static f64 presenting = 0;
            static f64 timeSettingGraphicsState = 0;
            static u64 operationsPerSecond = 0;
            static u64 operationsPerFrame = 0;
            static u64 lastOperations = 0;
            static auto lastSecond = steady_clock::now();

            frameCount++;

            auto curTime = steady_clock::now();
            if (curTime > lastSecond + 1s)
            {
                auto deltaTime = duration_cast<duration<float>>(curTime - lastSecond);
                fps = frameCount / deltaTime.count();
                lastSecond = curTime;
                totalAllocations = nova::ContextImpl::AllocationCount.load();
                newAllocations = nova::ContextImpl::NewAllocationCount.exchange(0);

                f64 divisor = 1000.0 * frameCount;
                submitting = (nova::TimeSubmitting.exchange(0) / divisor);
                adapting1 = (nova::TimeAdaptingFromAcquire.exchange(0) / divisor);
                adapting2 = (nova::TimeAdaptingToPresent.exchange(0) / divisor);
                presenting = (nova::TimePresenting.exchange(0) / divisor);
                timeSettingGraphicsState = (nova::TimeSettingGraphicsState.exchange(0) / divisor);

                operationsPerSecond = u64((nova::NumHandleOperations.load() - lastOperations) / deltaTime.count());
                operationsPerFrame = ((nova::NumHandleOperations.load() - lastOperations) / frameCount);
                lastOperations = nova::NumHandleOperations.load();

                frameCount = 0;
            }

            ImGui::Text("FPS: %.2f", fps);
            ImGui::Text("Vulkan Allocations: %lli (%lli/s)", totalAllocations, newAllocations);
            ImGui::Separator();
            ImGui::Text("Submit Draw    = %.2llf us", submitting);
            ImGui::Text("Submit Adapt 1 = %.2llf us", adapting1);
            ImGui::Text("Submit Adapt 2 = %.2llf us", adapting2);
            ImGui::Text("Present        = %.2llf us", presenting);
            ImGui::Text("State updates  = %.2llf us", timeSettingGraphicsState);
        }
        imgui.EndFrame(cmd, swapchain.GetCurrent());

        // countBuffer.Set<u32>({0});

        cmd.Present(swapchain.GetCurrent());
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence}, true);

        // NOVA_LOG("Index count = {}", indices.size());
        // NOVA_LOG("Count = {}", countBuffer.Get<u32>(0));

        glfwPollEvents();
    }

    // meshes.clear();
    // loadedMeshes.clear();
    // NOVA_LOG("Shutting down");
}

int main()
{
    try { TryMain(); }
    catch (...) {}
}