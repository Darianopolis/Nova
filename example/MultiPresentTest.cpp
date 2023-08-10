#include <nova/rhi/nova_RHI.hpp>
#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>
#include <nova/rhi/nova_RHI_Handle.hpp>
#include <nova/core/nova_Timer.hpp>
#include <nova/imgui/nova_ImGui.hpp>

#include <nova/imgui/vulkan/nova_VulkanImGui.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

using namespace nova::types;

void TryMain()
{
    auto context = nova::Context::Create({
        .debug = true,
    });
    NOVA_ON_SCOPE_EXIT(&) { context.Destroy(); };

    auto presentMode = nova::PresentMode::Mailbox;
    auto swapchainUsage = nova::TextureUsage::ColorAttach | nova::TextureUsage::Storage;

    glfwInit();
    NOVA_ON_SCOPE_EXIT(&) { glfwTerminate(); };
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* windows[] { 
        glfwCreateWindow(1920, 1200, "Present Test Window #1", nullptr, nullptr),
        glfwCreateWindow(1920, 1200, "Present Test Window #2", nullptr, nullptr),
    };
    NOVA_ON_SCOPE_EXIT(&) {
        glfwDestroyWindow(windows[0]);
        glfwDestroyWindow(windows[1]);
    };
    nova::Swapchain swapchains[] {
        nova::Swapchain::Create(context, glfwGetWin32Window(windows[0]), swapchainUsage, presentMode),
        nova::Swapchain::Create(context, glfwGetWin32Window(windows[1]), swapchainUsage, presentMode),
    };
    NOVA_ON_SCOPE_EXIT(&) {
        swapchains[0].Destroy();
        swapchains[1].Destroy();
    };

    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto state = nova::CommandState::Create(context);
    NOVA_ON_SCOPE_EXIT(&) { state.Destroy(); };
    u64 waitValues[] { 0ull, 0ull };
    auto fence = nova::Fence::Create(context);
    NOVA_ON_SCOPE_EXIT(&) { fence.Destroy(); };
    nova::CommandPool commandPools[] { 
        nova::CommandPool::Create(context, queue), 
        nova::CommandPool::Create(context, queue) 
    };
    NOVA_ON_SCOPE_EXIT(&) { 
        commandPools[0].Destroy();
        commandPools[1].Destroy();
    };

    auto imgui = [&] {
        auto cmd = commandPools[0].Begin(state);
        auto _imgui = nova::ImGuiLayer(context, cmd, swapchains[0].GetFormat(), 
            windows[0], { .flags = ImGuiConfigFlags_ViewportsEnable });

        queue.Submit({cmd}, {}, {fence});
        fence.Wait();
        
        return _imgui;
    }();

    u64 frame = 0;
    auto lastTime = std::chrono::steady_clock::now();
    auto frames = 0;
    auto update = [&] {

        // Debug output statistics
        frames++;
        auto newTime = std::chrono::steady_clock::now();
        if (newTime - lastTime > 1s)
        {
            NOVA_LOG("\nFps = {}\nAllocations = {:3} (+ {} /s)",
                frames, nova::Context::Impl::AllocationCount.load(), 
                nova::Context::Impl::NewAllocationCount.exchange(0));
            f64 divisor = 1000.0 * frames;
            NOVA_LOG("submit :: clear     = {:.2f}\n"
                     "submit :: adapting1 = {:.2f}\n"
                     "submit :: adapting2 = {:.2f}\n"
                     "present             = {:.2f}",
                nova::TimeSubmitting.exchange(0) / divisor,
                nova::TimeAdaptingFromAcquire.exchange(0)  / divisor,
                nova::TimeAdaptingToPresent.exchange(0)  / divisor,
                nova::TimePresenting.exchange(0) / divisor);

            lastTime = std::chrono::steady_clock::now();
            frames = 0;
        }

        // Pick fence and commandPool for frame in flight
        auto fif = frame++ % 2;

        // Wait for previous commands in frame to complete
        fence.Wait(waitValues[fif]);

        // Acquire new images from swapchains
        queue.Acquire({swapchains[0], swapchains[1]}, {fence});

        // Clear resource state tracking
        // state.Clear(3);

        // Reset command pool and begin new command list
        commandPools[fif].Reset();
        auto cmd = commandPools[fif].Begin(state);

        // Clear screen
        cmd.Clear(swapchains[0].GetCurrent(), Vec4(26 / 255.f, 89 / 255.f, 71 / 255.f, 1.f));

        // Draw ImGui demo window
        imgui.BeginFrame();
        ImGui::ShowDemoWindow();
        imgui.DrawFrame(cmd, swapchains[0].GetCurrent());

        // Present #1
        cmd.Present(swapchains[0]);

        // Clear and present #2
        cmd.Clear(swapchains[1].GetCurrent(), Vec4(112 / 255.f, 53 / 255.f, 132 / 255.f, 1.f));
        cmd.Present(swapchains[1]);

        // Submit work
        queue.Submit({cmd}, {fence}, {fence});

        // Present both swapchains
        queue.Present({swapchains[0], swapchains[1]}, {fence}, false);

        waitValues[fif] = fence.GetPendingValue();
    };
    
    NOVA_ON_SCOPE_EXIT(&) { fence.Wait(); };

    for (auto window : windows)
    {
        glfwSetWindowUserPointer(window, &update);
        glfwSetWindowSizeCallback(window, [](auto w, int,int) {
            (*static_cast<decltype(update)*>(glfwGetWindowUserPointer(w)))();
        });
    }

    while (!glfwWindowShouldClose(windows[0]) && !glfwWindowShouldClose(windows[1]))
    {
        update();
        glfwPollEvents();
    }
}

int main()
{
    try
    {
        TryMain();
    }
    catch(...)
    {
        std::cout << "Error\n";
    }
}