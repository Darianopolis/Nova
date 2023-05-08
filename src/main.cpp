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

    auto meshID = renderer.CreateMesh(
        std::array {
            Vertex { { -0.75f, -0.75f, 0.f }, { 1.f, 0.f, 0.f } },
            Vertex { {  0.75f, -0.75f, 0.f }, { 0.f, 1.f, 0.f } },
            Vertex { { -0.75f,  0.75f, 0.f }, { 0.f, 0.f, 1.f } },
            Vertex { {  0.75f,  0.75f, 0.f }, { 1.f, 1.f, 1.f } },
        }.data(), sizeof(Vertex) * 4,
        0, sizeof(Vertex), // position
        std::array {
            0u, 1u, 2u,
            3u, 2u, 1u,
        }.data(), 6);

    b8 inlineTint = true;

    auto materialTypeID = renderer.CreateMaterialType(
        std::format("{}{}{}",
            R"(
struct Vertex
{
    vec3 position;
    vec3 color;
};

layout(buffer_reference, scalar) buffer VertexBR { Vertex d[]; };
layout(buffer_reference, scalar) buffer TintBR { vec4 value[]; };

vec4 shade(uint64_t vertices, uint64_t material, uvec3 i, vec3 w)
{
    VertexBR v = VertexBR(vertices);
            )",
            inlineTint
                ? "return unpackUnorm4x8(uint(material))"
                : "return TintBR(material).value[0]",
            R"(
        * vec4(v.d[i.x].color * w.x + v.d[i.y].color * w.y + v.d[i.z].color * w.z, 1.0);
}
            )").c_str(),
        inlineTint);

    auto materialID = inlineTint
        ? renderer.CreateMaterial(materialTypeID, pyr::Temp(0xFFFFFFFFu), 4)
        : renderer.CreateMaterial(materialTypeID, pyr::Temp(vec4(1.f, 1.f, 1.f, 1.f)), sizeof(vec4));

    [[maybe_unused]] auto objectID = renderer.CreateObject(
        meshID, materialID,
        vec3(0.f, 0.f, -1.f),
        glm::angleAxis(glm::radians(45.f), vec3(0.f, 1.f, 0.f)),
        vec3(1.f));

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

            float fovDegrees = glm::degrees(fov);
            if (ImGui::DragFloat("FoV", &fovDegrees, 0.05f, 1.f, 179.f))
                fov = glm::radians(fovDegrees);

            ImGui::Separator();

            if (inlineTint)
            {
                auto& material = renderer.materials.Get(materialID);
                vec4 color = glm::unpackUnorm4x8(u32(material.data));
                if (ImGui::ColorPicker4("color", glm::value_ptr(color)))
                {
                    material.data = glm::packUnorm4x8(color);
                }
            }
            else
            {
                vec4* color = reinterpret_cast<vec4*>(renderer.materialBuffer.mapped + Renderer::MaterialSize * u32(materialID));
                ImGui::ColorPicker4("color", glm::value_ptr(*color));
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