#include <nova/rhi/nova_RHI.hpp>
#include <nova/rhi/nova_RHI_Impl.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

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

template<>
struct fastgltf::ElementTraits<glm::vec3> : fastgltf::ElementTraitsBase<f32, fastgltf::AccessorType::Vec3> {};
template<>
struct fastgltf::ElementTraits<glm::vec2> : fastgltf::ElementTraitsBase<f32, fastgltf::AccessorType::Vec2> {};

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
};

struct MeshInstance
{
    Mat4 transform;
    std::shared_ptr<Mesh> mesh;
};

ankerl::unordered_dense::map<usz, std::shared_ptr<Mesh>> loadedMeshes;

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
        hierarchyTransform = parentTransform * (t * r);
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
        auto& pOutMesh = loadedMeshes[node.meshIndex.value()];

        if (!pOutMesh)
        {
            auto& inMesh = asset.meshes[node.meshIndex.value()];
            NOVA_LOG(" - Loading mesh - {}", inMesh.name);

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

        auto& meshInstance =  outputMeshes.emplace_back();
        meshInstance.transform = instanceTransform;
        meshInstance.mesh = pOutMesh;
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
    data.loadFromFile(path);

    constexpr auto gltfOptions =
        fastgltf::Options::DontRequireValidAssetMember
        | fastgltf::Options::AllowDouble
        | fastgltf::Options::LoadGLBBuffers
        | fastgltf::Options::LoadExternalBuffers
        | fastgltf::Options::LoadExternalImages;

    auto type = fastgltf::determineGltfFileType(&data);
    std::unique_ptr<fastgltf::glTF> gltf;
    if (type == fastgltf::GltfType::glTF) {
        gltf = parser.loadGLTF(&data, path.parent_path(), gltfOptions);
    } else if (type == fastgltf::GltfType::GLB) {
        gltf = parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
    } else {
        NOVA_THROW("Failed to determine glTF container");
    }

    if (parser.getError() != fastgltf::Error::None)
        NOVA_THROW("Failed to load glTF: {}", fastgltf::to_underlying(parser.getError()));

    if (auto res = gltf->parse(fastgltf::Category::Scenes); res != fastgltf::Error::None)
        NOVA_THROW("Failed to parse glTF: {}", fastgltf::to_underlying(res));

    auto asset = gltf->getParsedAsset();

    ankerl::unordered_dense::set<usz> rootNodes;
    for (usz i = 0; i < asset->nodes.size(); ++i)
        rootNodes.insert(i);

    for (auto& node : asset->nodes)
    {
        for (auto& child : node.children)
            rootNodes.erase(child);
    }

    for (auto& root : rootNodes)
    {
        // NOVA_LOG("Root node: {} - {}", root, asset->nodes[root].name);
        ProcessNode(*asset, context, Mat4(1.f), root, outputMeshes);
    }
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
    });

    auto surface = +nova::Surface(context, glfwGetWin32Window(window));
    auto swapchain = +nova::Swapchain(context, surface,
        nova::TextureUsage::ColorAttach
        | nova::TextureUsage::TransferDst,
        nova::PresentMode::Immediate);

    auto queue = context.GetQueue(nova::QueueFlags::Graphics);
    auto cmdPool = +nova::CommandPool(context, queue);
    auto fence = +nova::Fence(context);
    auto state = +nova::CommandState(context);

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
    // LoadGltf("D:/Dev/Projects/pyrite/pyrite-v4/assets/models/monkey.gltf", meshes);
    // LoadGltf("D:/Dev/Projects/pyrite/pyrite-v4/assets/models/SponzaMain/NewSponza_Main_Blender_glTF.gltf", meshes);
    LoadGltf("D:/Dev/Data/3DModels/Small_City_LVL/Small_City_LVL.gltf", context, meshes);
    NOVA_ON_EXIT(&) { loadedMeshes.clear(); };

    // for (auto& pos : meshes[0].vertices)
    //     vertices.push_back({pos, {}});
    // indices = meshes[0].indices;

    // std::default_random_engine rng(0);
    // std::uniform_real_distribution<f32> dist11(-1.f, 1.f);
    // std::uniform_real_distribution<f32> dist01( 0.f, 1.f);
    // for (u32 i = 0; i < 20; ++i)
    //     vertices.emplace_back(Vec3(dist11(rng), dist11(rng), dist11(rng)), Vec3(dist01(rng), dist01(rng), dist01(rng)));
    // std::uniform_int_distribution<u32> distIndex(0, u32(vertices.size()) - 1);
    // for (u32 i = 0; i < 1000; ++i)
    // {
    //     indices.emplace_back(distIndex(rng));
    //     indices.emplace_back(distIndex(rng));
    //     indices.emplace_back(distIndex(rng));
    // }

    // auto vertexBuffer = +nova::Buffer(context, vertices.size() * sizeof(Vertex),
    //     nova::BufferUsage::Storage, nova::BufferFlags::DeviceLocal | nova::BufferFlags::CreateMapped);
    // vertexBuffer.Set<Vertex>(vertices);

    // auto indexBuffer = +nova::Buffer(context, indices.size() * 4,
    //     nova::BufferUsage::Index, nova::BufferFlags::DeviceLocal | nova::BufferFlags::CreateMapped);
    // indexBuffer.Set<u32>(indices);

    nova::Buffer::Arc vertexBuffer;
    nova::Buffer::Arc indexBuffer;
    {
        usz vertexCount = 0;
        usz indexCount = 0;

        for (auto&[id, mesh] : loadedMeshes)
        {
            vertexCount += mesh->vertices.size();
            indexCount += mesh->indices.size();
        }

        NOVA_LOGEXPR(vertexCount);
        NOVA_LOGEXPR(indexCount);

        vertexBuffer = +nova::Buffer(context, vertexCount * sizeof(Vertex),
            nova::BufferUsage::Storage, nova::BufferFlags::DeviceLocal | nova::BufferFlags::CreateMapped);

        indexBuffer = +nova::Buffer(context, indexCount * 4,
            nova::BufferUsage::Index, nova::BufferFlags::DeviceLocal | nova::BufferFlags::CreateMapped);

        vertexCount = 0;
        indexCount = 0;

        for (auto&[id, mesh] : loadedMeshes)
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

// layout(location = 0) in vec3 inColor;
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

    auto lastReport = std::chrono::steady_clock::now();
    auto frames = 0;

    NOVA_ON_SCOPE_EXIT(&) { fence.Wait(); };
    while (!glfwWindowShouldClose(window))
    {
        // Debug output statistics
        frames++;
        auto newTime = std::chrono::steady_clock::now();
        if (newTime - lastReport > 1s)
        {
            NOVA_LOG("\nFps = {} ({:.2f} ms)\nAllocations = {:3} (+ {} /s)",
                frames, (1000.0 / frames), nova::ContextImpl::AllocationCount.load(), nova::ContextImpl::NewAllocationCount.exchange(0));
            f64 divisor = 1000.0 * frames;
            NOVA_LOG("submit :: clear     = {:.2f}\nsubmit :: adapting1 = {:.2f}\nsubmit :: adapting2 = {:.2f}\npresent             = {:.2f}",
                nova::TimeSubmitting.exchange(0) / divisor,
                nova::TimeAdaptingFromAcquire.exchange(0)  / divisor,
                nova::TimeAdaptingToPresent.exchange(0)  / divisor,
                nova::TimePresenting.exchange(0) / divisor);

            NOVA_LOG("Atomic Operations = {}", nova::NumHandleOperations.load());

            lastReport = std::chrono::steady_clock::now();
            frames = 0;
        }

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
                nova::Format::D24U_X8);

            cmd.Transition(depthBuffer,
                nova::ResourceState::DepthStencilAttachment,
                nova::PipelineStage::Graphics);
        }

        auto proj = ProjInfReversedZRH(viewFov, f32(swapchain.GetExtent().x) / f32(swapchain.GetExtent().y), 0.01f);
        auto translate = glm::translate(Mat4(1.f), position);
        auto rotate = glm::mat4_cast(rotation);
        auto viewProj = proj * glm::affineInverse(translate * rotate);

        cmd.BeginRendering({swapchain.GetCurrent()}, depthBuffer);
        cmd.ClearColor(0, Vec4(Vec3(0.1f), 1.f), swapchain.GetExtent());
        cmd.ClearDepth(0.f, swapchain.GetExtent());
        cmd.SetGraphicsState(pipelineLayout, {vertexShader, fragmentShader}, {
            // .cullMode = nova::CullMode::None,
            .cullMode = nova::CullMode::Back,
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

        // countBuffer.Set<u32>({0});

        cmd.Present(swapchain.GetCurrent());
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence}, true);

        // NOVA_LOG("Index count = {}", indices.size());
        // NOVA_LOG("Count = {}", countBuffer.Get<u32>(0));

        glfwPollEvents();
    }
}

int main()
{
    try { TryMain(); }
    catch (...) {}
}