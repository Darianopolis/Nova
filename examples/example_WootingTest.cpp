#include "main/example_Main.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/rhi/nova_RHI.hpp>

#include <nova/ui/nova_ImGui.hpp>

#include <nova/window/nova_Window.hpp>

#include <wooting-analog-wrapper.h>

#include <nova/core/win32//nova_Win32Include.hpp>

#include <stdint.h>
#include <cstdint>

using enum WootingAnalogResult;

#pragma comment(lib, "ntdll")

NOVA_EXAMPLE(WootingTest, "wooting")
{
    auto woores = wooting_analog_initialise();
    if (woores >= 0) {
        NOVA_LOG("Successfully initialized wooting, found {} devices", woores);
    } else {
        switch (WootingAnalogResult(woores)) {
            break;case WootingAnalogResult_NoPlugins:
                NOVA_LOG("Wooting error: NoPlugins");
            break;case WootingAnalogResult_FunctionNotFound:
                NOVA_LOG("Wooting error: FunctionNotFound");
            break;case WootingAnalogResult_IncompatibleVersion:
                NOVA_LOG("Wooting error: IncompatibleVersion");
        }
    }

    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };
    auto window = nova::Window::Create(app)
        .SetTitle("Nova - Triangle Minimal")
        .SetSize({ 1920, 1080 }, nova::WindowPart::Client)
        .Show(true);

    auto context = nova::Context::Create({
        .debug = true,
    });
    auto swapchain = nova::Swapchain::Create(context, window.NativeHandle(),
        nova::ImageUsage::ColorAttach
        | nova::ImageUsage::TransferDst,
        nova::PresentMode::Fifo);
    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);
    auto sampler = nova::Sampler::Create(context, nova::Filter::Linear,
        nova::AddressMode::Repeat, nova::BorderColor::TransparentBlack, 0.f);

    NOVA_DEFER(&)
    {
        sampler.Destroy();
        swapchain.Destroy();
        context.Destroy();
    };

    auto imgui = nova::imgui::ImGuiLayer({
        .window = window,
        .context = context,
        .sampler = sampler,
        .frames_in_flight = 1,
    });

    NOVA_DEFER(&)
    {
        queue.WaitIdle();
    };

    std::vector<unsigned short> wooting_codes (1024);
    std::vector<float>          wooting_values(1024);

    wooting_analog_set_keycode_mode(WootingAnalog_KeycodeType::WootingAnalog_KeycodeType_ScanCode1);

    while (app.ProcessEvents()) {

        queue.WaitIdle();
        queue.Acquire(swapchain);
        auto cmd = queue.Begin();

        cmd.ClearColor(swapchain.Target(), Vec4(0.f));

        imgui.BeginFrame();

        ImGui::Begin("Wooting");
        {
            for (u16 code = 0; code < 200; ++code) {
                float value = wooting_analog_read_analog(code);
                if (value > 0.f) {
                    ImGui::Text("Key[%i]: %.4f", code,  value);
                }
            }
        }
        ImGui::End();

        imgui.DrawFrame(cmd, swapchain.Target());

        cmd.Present(swapchain);
        queue.Submit(cmd, {});
        queue.Present(swapchain, {});

        app.WaitForEvents();
    }
}