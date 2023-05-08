#include <VulkanBackend/VulkanBackend.hpp>

#include <Renderer/Renderer.hpp>
#include <ImGui/ImGuiBackend.h>

using namespace pyr;

int main()
{
    auto ctx = pyr::CreateContext(true);
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto window = glfwCreateWindow(1920, 1080, "test", nullptr, nullptr);

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(ctx.instance, window, nullptr, &surface);

    auto swapchain = ctx.CreateSwapchain(surface,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT
        | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        | VK_IMAGE_USAGE_STORAGE_BIT
        | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_PRESENT_MODE_MAILBOX_KHR);

    pyr::Renderer renderer;
    renderer.Init(ctx);

    pyr::ImGuiBackend imgui;
    imgui.Init(ctx, swapchain, window,
        ImGuiConfigFlags_ViewportsEnable
        | ImGuiConfigFlags_DockingEnable);

    VkSampler sampler;
    pyr::VkCall(vkCreateSampler(ctx.device, pyr::Temp(VkSamplerCreateInfo {
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

    auto loadTexture = [&](const char* path) -> std::pair<pyr::Image, TextureID> {
        int width, height, channels;
        auto data = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
        PYR_ON_SCOPE_EXIT(&) { stbi_image_free(data); };
        if (!data)
            PYR_THROW("Failed to load image: {}", path);

        PYR_LOG("Loaded image [{}], size = ({}, {})", path, width, height);

        pyr::Image texture = ctx.CreateImage(vec3(u32(width), u32(height), 0),
            VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R8G8B8A8_UNORM, ImageFlags::Mips);
        ctx.CopyToImage(texture, data, width * height * 4);
        ctx.GenerateMips(texture);
        ctx.Transition(ctx.cmd, texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        return { texture, renderer.RegisterTexture(texture.view, sampler) };
    };

    auto[texture, textureID] = loadTexture("assets/textures/statue.jpg");
    auto[texture2, textureID2] = loadTexture("assets/textures/missing.png");

    PYR_LOG("Texture ID = {}", u32(textureID));

    // Test mesh

    struct Vertex
    {
        vec3 position;
        vec3 color;
        vec2 texCoord;
        u32 texIndex;
    };

    std::array mesh = {
        Vertex { { -0.75f, -0.75f, 0.f }, { 1.f, 0.f, 0.f }, { 0.f, 1.f }, u32(textureID2) },
        Vertex { {  0.75f, -0.75f, 0.f }, { 0.f, 1.f, 0.f }, { 1.f, 1.f }, u32(textureID) },
        Vertex { { -0.75f,  0.75f, 0.f }, { 0.f, 0.f, 1.f }, { 0.f, 0.f }, u32(textureID) },
        Vertex { {  0.75f,  0.75f, 0.f }, { 1.f, 1.f, 1.f }, { 1.f, 0.f }, u32(textureID2) },
    };

    auto meshID = renderer.CreateMesh(
        sizeof(mesh), mesh.data(),
        sizeof(Vertex), 0,
        6, std::array {
            0u, 1u, 2u,
            3u, 2u, 1u,
        }.data());

    // Test material

    b8 inlineTint = false;

    auto materialTypeID = renderer.CreateMaterialType(
        std::regex_replace(
            R"(
struct Vertex
{
    vec3 position;
    vec3 color;
    vec2 uv;
    uint texIndex;
};

layout(buffer_reference, scalar) buffer VertexBR { Vertex data[]; };
layout(buffer_reference, scalar) buffer TintBR { vec4 value[]; };

vec4 shade(uint64_t vertices, uint64_t material, uvec3 i, vec3 w)
{
    Vertex v0 = VertexBR(vertices).data[i.x];
    Vertex v1 = VertexBR(vertices).data[i.y];
    Vertex v2 = VertexBR(vertices).data[i.z];

    vec2 texCoord  = v0.uv * w.x + v1.uv * w.y + v2.uv * w.z;

    vec3 tc0 = texture(textures[nonuniformEXT(v0.texIndex)], texCoord).rgb;
    vec3 tc1 = texture(textures[nonuniformEXT(v1.texIndex)], texCoord).rgb;
    vec3 tc2 = texture(textures[nonuniformEXT(v2.texIndex)], texCoord).rgb;

    vec3 vertColor = v0.color * w.x + v1.color * w.y + v2.color * w.z;
    vec3 texColor  = tc0      * w.x + tc1      * w.y + tc2      * w.z;

    vec4 tint = TINT_UNPACK;
    return vec4(mix(vertColor, texColor, tint.rgb), tint.a);
}
            )",
            std::regex("TINT_UNPACK"),
            inlineTint
                ? "unpackUnorm4x8(uint(material))"
                : "TintBR(material).value[0]").c_str(),
        inlineTint);

    auto materialID = inlineTint
        ? renderer.CreateMaterial(materialTypeID, pyr::Temp(0xFFFFFFFFu), 4)
        : renderer.CreateMaterial(materialTypeID, pyr::Temp(vec4(1.f, 1.f, 1.f, 1.f)), sizeof(vec4));

    // Test objet

    for (u32 i = 0; i < 10; ++i)
        renderer.CreateObject(meshID, materialID, vec3(0.f, 0.f, -0.1f * i), vec3(0.f), vec3(1.f));

    vec3 position = vec3(0.f, 0.f, 1.f);
    quat rotation = vec3(0.f);
    f32 fov = glm::radians(90.f);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ctx.GetNextImage(swapchain);

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
        ImGui::ShowDemoWindow();

        {
            ImGui::Begin("Settings");
            PYR_ON_SCOPE_EXIT() { ImGui::End(); };

            ImGui::DragFloat("Move speed", &moveSpeed, 0.1f, 0.f, FLT_MAX);

            f32 fovDegrees = glm::degrees(fov);
            if (ImGui::DragFloat("FoV", &fovDegrees, 0.05f, 1.f, 179.f))
                fov = glm::radians(fovDegrees);

            ImGui::Separator();

            if (inlineTint)
            {
                auto& material = renderer.materials.Get(materialID);
                vec4 color = glm::unpackUnorm4x8(u32(material.data));
                if (ImGui::ColorPicker4("Color", glm::value_ptr(color)))
                    material.data = glm::packUnorm4x8(color);
            }
            else
            {
                vec4* color = reinterpret_cast<vec4*>(renderer.materialBuffer.mapped + Renderer::MaterialSize * u32(materialID));
                ImGui::ColorPicker4("Color", glm::value_ptr(*color));
            }
        }

        renderer.Draw(*swapchain.image);

        imgui.EndFrame(swapchain);
        ctx.Present(swapchain);
    }

    vkDeviceWaitIdle(ctx.device);

    glfwDestroyWindow(window);
    glfwTerminate();
}