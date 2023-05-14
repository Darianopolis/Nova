#include <VulkanBackend/VulkanBackend.hpp>

#include <Renderer/Mesh.hpp>

#include <Renderer/Renderer.hpp>
#include <ImGui/ImGuiBackend.h>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

using namespace pyr;

int main()
{
    auto ctx = pyr::CreateContext(false);
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1080, "test", nullptr, nullptr);

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(ctx->instance, window, nullptr, &surface);

    auto swapchain = ctx->CreateSwapchain(surface,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT
        | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        | VK_IMAGE_USAGE_STORAGE_BIT
        | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_PRESENT_MODE_MAILBOX_KHR);

    pyr::Renderer renderer;
    renderer.Init(*ctx);

    pyr::ImGuiBackend imgui;
    imgui.Init(*ctx, *swapchain, window,
        ImGuiConfigFlags_ViewportsEnable
        | ImGuiConfigFlags_DockingEnable);

// -----------------------------------------------------------------------------

    VkSampler sampler;
    pyr::VkCall(vkCreateSampler(ctx->device, pyr::Temp(VkSamplerCreateInfo {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = 16.f,
        .minLod = -1000.f,
        .maxLod = 1000.f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    }), nullptr, &sampler));

    auto loadTexture = [&](const char* path) -> std::pair<Ref<Image>, TextureID> {
        int width, height, channels;
        auto data = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
        PYR_ON_SCOPE_EXIT(&) { stbi_image_free(data); };
        if (!data)
            PYR_THROW("Failed to load image: {}", path);

        PYR_LOG("Loaded image [{}], size = ({}, {})", path, width, height);

        auto texture = ctx->CreateImage(vec3(u32(width), u32(height), 0),
            VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R8G8B8A8_UNORM, ImageFlags::Mips);
        ctx->CopyToImage(*texture, data, width * height * 4);
        ctx->GenerateMips(*texture);
        ctx->Transition(ctx->cmd, *texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        return { texture, renderer.RegisterTexture(texture->view, sampler) };
    };

    auto[missing, missingID] = loadTexture("assets/textures/missing.png");

// -----------------------------------------------------------------------------

    auto materialTypeID = renderer.CreateMaterialType(
        R"(
struct Vertex
{
    vec3 position;
    vec3 normal;
    vec2 uv;
    uint texIndex;
};

layout(buffer_reference, scalar) buffer VertexBR { Vertex data[]; };
layout(buffer_reference, scalar) buffer MaterialBR { uint ids[]; };

// P is the intersection point
// f is the triangle normal
// d is the ray cone direction
vec4 computeAnistropicEllipseAxes(vec3 P, vec3 f,
    vec3 d, float rayConeRadiusAtIntersection,
    vec3 positions[3], vec2 txcoords[3],
    vec2 interpolatedTexCoordsAtIntersection)
{
    // Compute ellipse axes.
    vec3 a1 = d - dot(f, d) * f;
    vec3 p1 = a1 - dot(d, a1) * d;
    a1 *= rayConeRadiusAtIntersection / max(0.0001, length(p1));
    vec3 a2 = cross(f, a1);
    vec3 p2 = a2 - dot(d, a2) * d;
    a2 *= rayConeRadiusAtIntersection / max(0.0001, length(p2));

    // Compute texture coordinate gradients.
    vec3 eP, delta = P - positions[0];
    vec3 e1 = positions[1] - positions[0];
    vec3 e2 = positions[2] - positions[0];

    float oneOverAreaTriangle = 1.0 / dot(f, cross(e1, e2));

    eP = delta + a1;
    float u1 = dot(f, cross(eP, e2)) * oneOverAreaTriangle;
    float v1 = dot(f, cross(e1, eP)) * oneOverAreaTriangle;
    vec2 texGradient1 = (1.0-u1-v1) * txcoords[0] + u1 * txcoords[1] +
        v1 * txcoords[2] - interpolatedTexCoordsAtIntersection;

    eP = delta + a2;
    float u2 = dot(f, cross(eP, e2)) * oneOverAreaTriangle;
    float v2 = dot(f, cross(e1, eP)) * oneOverAreaTriangle;
    vec2 texGradient2 = (1.0-u2-v2) * txcoords[0] + u2 * txcoords[1] +
        v2 * txcoords[2] - interpolatedTexCoordsAtIntersection;

    return vec4(texGradient1, texGradient2);
}

bool material_AlphaTest(uint64_t vertices, uint64_t material, uvec3 i, vec3 w, vec3 rayDir, float rayConeRadius, bool frontFace)
{
    Vertex v0 = VertexBR(vertices).data[i.x];
    Vertex v1 = VertexBR(vertices).data[i.y];
    Vertex v2 = VertexBR(vertices).data[i.z];

    vec3 pos = v0.position * w.x + v1.position * w.y + v2.position * w.z;
    vec2 texCoord  = v0.uv * w.x + v1.uv * w.y + v2.uv * w.z;
    vec3 nrm = normalize(v0.normal * w.x + v1.normal * w.y + v2.normal * w.z);
    if (!frontFace)
        nrm = -nrm;

    vec4 grad = computeAnistropicEllipseAxes(pos, nrm, rayDir, rayConeRadius,
        vec3[](v0.position, v1.position, v2.position),
        vec2[](v0.uv, v1.uv, v2.uv), texCoord);

    float alpha = textureGrad(textures[nonuniformEXT(MaterialBR(material).ids[v0.texIndex])], texCoord, grad.xy, grad.zw).a;
    return alpha > 0.5;
}

vec4 material_Shade(uint64_t vertices, uint64_t material, uvec3 i, vec3 w, vec3 rayDir, float rayConeRadius, bool frontFace)
{
    Vertex v0 = VertexBR(vertices).data[i.x];
    Vertex v1 = VertexBR(vertices).data[i.y];
    Vertex v2 = VertexBR(vertices).data[i.z];

    vec3 pos = v0.position * w.x + v1.position * w.y + v2.position * w.z;
    vec2 texCoord  = v0.uv * w.x + v1.uv * w.y + v2.uv * w.z;
    vec3 nrm = normalize(v0.normal * w.x + v1.normal * w.y + v2.normal * w.z);
    if (!frontFace)
        nrm = -nrm;

    MaterialBR mat = MaterialBR(material);

    vec4 grad = computeAnistropicEllipseAxes(pos, nrm, rayDir, rayConeRadius,
        vec3[](v0.position, v1.position, v2.position),
        vec2[](v0.uv, v1.uv, v2.uv), texCoord);

    vec4 color = textureGrad(textures[nonuniformEXT(mat.ids[v0.texIndex])], texCoord, grad.xy, grad.zw);
    if (color.a < 0.5)
        return vec4(0);

    // if (w.x > 0.05 && w.y > 0.05 && w.z > 0.05)
    //     return vec4(0.1, 0.1, 0.1, 0);

    float d = dot(normalize(vec3(0.5, 0.5, 0.5)), nrm) * 0.4 + 0.6;
    return vec4(color.rgb * d, color.a);
}
        )",
        false);

    auto redMaterialType = renderer.CreateMaterialType(
        R"(
vec4 material_Shade(uint64_t vertices, uint64_t material, uvec3 i, vec3 w, vec3 rayDir, float rayConeRadius, bool frontFace)
{
    return vec4(1, 0, 0, 1);
}
        )", true);

    auto redMaterial = renderer.CreateMaterial(redMaterialType, nullptr, 0);

    auto greenMaterialType = renderer.CreateMaterialType(
        R"(
vec4 material_Shade(uint64_t vertices, uint64_t material, uvec3 i, vec3 w, vec3 rayDir, float rayConeRadius, bool frontFace)
{
    return vec4(0, 1, 0, 1);
}
        )", true);

    auto greenMaterial = renderer.CreateMaterial(greenMaterialType, nullptr, 0);

// -----------------------------------------------------------------------------

    auto loadMesh = [&](const std::string& folder, const std::string& file = "scene.gltf", MaterialID* customMaterialID = nullptr) {
        auto mesh = pyr::LoadMesh(*ctx, (folder +"/"+ file).c_str(), folder.c_str());

        auto meshID = renderer.CreateMesh(
            sizeof(GltfVertex) * mesh.vertices.size(), mesh.vertices.data(),
            u32(sizeof(GltfVertex)), 0,
            u32(mesh.indices.size()), mesh.indices.data());

        if (mesh.images.empty())
            mesh.images.push_back(missing);

        std::vector<TextureID> textures;
        for (auto& image : mesh.images)
            textures.push_back(renderer.RegisterTexture(image->view, sampler));

        auto materialID = customMaterialID
            ? *customMaterialID
            : renderer.CreateMaterial(materialTypeID, textures.data(), textures.size() * sizeof(TextureID));

        renderer.CreateObject(meshID, materialID, vec3(0.f), vec3(0.f), vec3(1.f));

        return mesh;
    };

    auto sponza = loadMesh("assets/models/SponzaMain", "NewSponza_Main_Blender_glTF.gltf");
    auto sponzaCurtains = loadMesh("assets/models/SponzaCurtains", "NewSponza_Curtains_glTF.gltf", &redMaterial);
    auto sponzaIvy = loadMesh("assets/models/SponzaIvy", "NewSponza_IvyGrowth_glTF.gltf", &greenMaterial);

    // loadMesh("assets/models/SciFiDragonWarrior");
    // loadMesh("assets/models/StationDemerzel");
    // loadMesh("assets/models", "monkey.gltf");

    renderer.RebuildShaderBindingTable();

// -----------------------------------------------------------------------------

    vec3 position = vec3(0.f, 1.f, 1.f);
    quat rotation = vec3(0.f);
    f32 fov = glm::radians(90.f);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ctx->GetNextImage(*swapchain, ctx->graphics, ctx->semaphore.Raw());
        // ctx->semaphore->Wait();

        static f32 moveSpeed = 1.f;

        {
            static f64 lastX = 0.0, lastY = 0.0;
            f64 x, y;
            glfwGetCursorPos(window, &x, &y);
            f32 deltaX = f32(x - lastX), deltaY = f32(y - lastY);
            lastX = x;
            lastY = y;

            using namespace std::chrono;
            static steady_clock::time_point lastTime = steady_clock::now();
            auto now = steady_clock::now();
            auto delta = duration_cast<duration<f32>>(now - lastTime).count();
            lastTime = now;

            PYR_DO_ONCE(&) {
                glfwSetScrollCallback(window, [](auto, f64, f64 dy) {
                    if (dy > 0)
                        moveSpeed *= 1.1f;
                    if (dy < 0)
                        moveSpeed /= 1.1f;
                });
            };

            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_2) == GLFW_PRESS)
            {
                rotation = glm::angleAxis(deltaX * 0.0025f, vec3(0.f, -1.f, 0.f)) * rotation;
                rotation = rotation * glm::angleAxis(deltaY * 0.0025f, vec3(-1.f, 0.f, 0.f));
                rotation = glm::normalize(rotation);
            }

            {
                glm::vec3 translate{};
                if (glfwGetKey(window, GLFW_KEY_W))          translate += vec3( 0.f,  0.f, -1.f);
                if (glfwGetKey(window, GLFW_KEY_A))          translate += vec3(-1.f,  0.f,  0.f);
                if (glfwGetKey(window, GLFW_KEY_S))          translate += vec3( 0.f,  0.f,  1.f);
                if (glfwGetKey(window, GLFW_KEY_D))          translate += vec3( 1.f,  0.f,  0.f);
                if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) translate += vec3( 0.f, -1.f,  0.f);
                if (glfwGetKey(window, GLFW_KEY_SPACE))      translate += vec3( 0.f,  1.f,  0.f);

                position += rotation * (translate * moveSpeed * delta);
            }

            renderer.SetCamera(position, rotation, fov);
        }

        imgui.BeginFrame();

        {
            ImGui::Begin("Settings");
            PYR_ON_SCOPE_EXIT() { ImGui::End(); };

            ImGui::DragFloat("Move speed", &moveSpeed, 0.1f, 0.f, FLT_MAX);

            f32 fovDegrees = glm::degrees(fov);
            if (ImGui::DragFloat("FoV", &fovDegrees, 0.05f, 1.f, 179.f))
                fov = glm::radians(fovDegrees);

            ImGui::Separator();

            ImGui::Checkbox("Ray trace", &renderer.rayTrace);
            float grad100 = 100.f * renderer.rayConeGradient;
            if (ImGui::DragFloat("Ray Cone Gradient", &grad100, 0.001f, 0.f, 1.f))
                renderer.rayConeGradient = 0.01f * grad100;

            ImGui::Separator();

            using namespace std::chrono;
            static u32 frameCount = 0;
            static f32 fps = 0.f;
            static auto lastSecond = steady_clock::now();

            frameCount++;

            auto curTime = steady_clock::now();
            if (curTime > lastSecond + 1s)
            {
                fps = frameCount / duration_cast<duration<float>>(curTime - lastSecond).count();
                lastSecond = curTime;
                frameCount = 0;
            }

            ImGui::Text("FPS: %.2f", fps);
        }

        renderer.Draw(*swapchain->image);

        imgui.EndFrame(*swapchain);

        ctx->Transition(ctx->cmd, *swapchain->image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        ctx->commands->Submit(ctx->cmd, ctx->semaphore.Raw(), ctx->semaphore.Raw());
        ctx->Present(*swapchain, ctx->semaphore.Raw());
        ctx->semaphore->Wait();
        ctx->commands->Clear();
        ctx->cmd = ctx->commands->Allocate();
    }

    vkDeviceWaitIdle(ctx->device);

    glfwDestroyWindow(window);
    glfwTerminate();
}