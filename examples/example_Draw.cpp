#include "main/example_Main.hpp"

#include <nova/core/nova_Timer.hpp>
#include <nova/rhi/nova_RHI.hpp>
#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>
#include <nova/ui/nova_Draw2D.hpp>
#include <nova/window/nova_Window.hpp>
#include <nova/vfs/nova_VirtualFilesystem.hpp>
#include <nova/asset/nova_Image.hpp>

#include <nova/core/win32/nova_Win32.hpp>
#pragma warning(push)
#pragma warning(disable: 4263)
#include "dcomp.h"
#pragma warning(pop)

NOVA_EXAMPLE(Draw, "draw")
{
    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };

    auto window = nova::Window::Create(app)
        .SetTitle("Nova - Draw")
        .SetDecorate(false)
        .SetTransparency(nova::TransparencyMode::PerPixel)
        .Show(true);

    auto context = nova::Context::Create({
        .debug = false,
    });
    NOVA_DEFER(&) { context.Destroy(); };

    // auto swapchain = nova::Swapchain::Create(context, window.NativeHandle(),
    //     nova::ImageUsage::TransferDst
    //     | nova::ImageUsage::ColorAttach,
    //     nova::PresentMode::Fifo);
    // NOVA_DEFER(&) { swapchain.Destroy(); };

    auto queue = context.Queue(nova::QueueFlags::Graphics, 0);

// -----------------------------------------------------------------------------

    nova::draw::Draw2D im_draw{ context };

// -----------------------------------------------------------------------------

    nova::Image image;
    {
        nova::ImageDescription desc;
        nova::ImageLoadData src;
        nova::Image_Load(&desc, &src, "assets/textures/statue.jpg");
        NOVA_DEFER(&) { src.Destroy(); };

        nova::ImageDescription target_desc{ desc };
        target_desc.is_signed = false;
        target_desc.format = nova::ImageFormat::RGBA8;
        nova::ImageAccessor target_accessor(target_desc);
        std::vector<b8> data(target_accessor.GetSize());
        nova::Image_Copy(desc, src.data, target_accessor, data.data());

        image = nova::Image::Create(context,
            Vec3U(desc.width, desc.height, 0),
            nova::ImageUsage::Sampled,
            nova::Format::RGBA8_UNorm);

        image.Set({}, {desc.width, desc.height, 1}, data.data());
        image.Transition(nova::ImageLayout::Sampled);
    }

// -----------------------------------------------------------------------------

    nova::Image target;
    nova::Buffer buffer;
    HDC hdc_screen = GetDC(nullptr);
    BITMAPINFO bmi = {};
    HANDLE file = {};
    bool file_map_bitmap = false;
    HBITMAP bitmap = {};
    void* bits = {};
    NOVA_DEFER(&) {
        target.Destroy();
        buffer.Destroy();
        if (bitmap) {
            DeleteObject(bitmap);
        }
        if (file) {
            CloseHandle(file);
        }
        // if (buffer) {
        //     context->vkDestroyBuffer(context->device, buffer->buffer, context->alloc);
        // }
        // if (memory) {
        //     context->vkFreeMemory(context->device, memory, context->alloc);
        // }
    };

// -----------------------------------------------------------------------------

    auto size = app.PrimaryDisplay().Size();
    std::cout << "Monitor size = " << size.x << ", " << size.y << '\n';

    auto font = im_draw.LoadFont(nova::vfs::Load("fonts/arial.ttf"), 20.f);

    nova::draw::Rectangle box1 {
        .center_color = { 1.f, 0.f, 0.f, 0.5f },
        .border_color = { 0.2f, 0.2f, 0.2f, 1.f },
        .center_pos = { size.x * 0.25f, size.y * 0.25f },
        .half_extent = { 100.f, 200.f },
        .corner_radius = 15.f,
        .border_width = 5.f,

        .tex_tint = { 1.f, 1.f, 1.f, 1.f },
        .tex_handle = {image.Descriptor(), im_draw.default_sampler.Descriptor()},
        .tex_center_pos = { 0.5f, 0.5f },
        .tex_half_extent = { 0.5f, 1.f },
    };

    nova::draw::Rectangle box2 {
        .center_color = { 0.f, 1.f, 0.f, 0.5f },
        .border_color = { 0.4f, 0.4f, 0.4f, 1.f },
        .center_pos = { size.x * 0.5f, size.y * 0.5f },
        .half_extent = { 100.f, 100.f },
        .corner_radius = 15.f,
        .border_width = 10.f,

        .tex_tint = { 1.f, 1.f, 1.f, 1.f },
        .tex_handle = {image.Descriptor(), im_draw.default_sampler.Descriptor()},
        .tex_center_pos = { 0.5f, 0.5f },
        .tex_half_extent = { 0.5f, 0.5f },
    };

    nova::draw::Rectangle box3 {
        .center_color = { 0.f, 0.f, 1.f, 0.5f },
        .border_color = { 0.6f, 0.6f, 0.6f, 1.f },
        .center_pos = { size.x * 0.75f, size.y * 0.75f },
        .half_extent = { 200.f, 100.f },
        .corner_radius = 25.f,
        .border_width = 10.f,

        .tex_tint = { 1.f, 1.f, 1.f, 1.f },
        .tex_handle = {image.Descriptor(), im_draw.default_sampler.Descriptor()},
        .tex_center_pos = { 0.5f, 0.5f },
        .tex_half_extent = { 1.f, 0.5f },
    };

    bool redraw = true;
    bool skip_update = false;

    auto last_frame = std::chrono::steady_clock::now();

    auto last_time = std::chrono::steady_clock::now();
    auto frames = 0;

    NOVA_DEFER(&) { queue.WaitIdle(); };
    while (app.ProcessEvents()) {

        auto new_time = std::chrono::steady_clock::now();

        float delta_time = std::chrono::duration_cast<std::chrono::duration<float>>(new_time - last_frame).count();
        last_frame = new_time;

        // Debug output statistics
        frames++;
        if (new_time - last_time > 1s) {
            nova::Log("Frametime   = {} ({} fps)"
                    "\nAllocations = {:3} (+ {} /s)",
                nova::DurationToString((new_time - last_time) / frames), frames, nova::rhi::stats::AllocationCount.load(),
                nova::rhi::stats::NewAllocationCount.exchange(0));
            f64 divisor = 1000.0 * frames;
            nova::Log("submit :: clear     = {:.2f}"
                    "\nsubmit :: adapting1 = {:.2f}"
                    "\nsubmit :: adapting2 = {:.2f}"
                    "\npresent             = {:.2f}\n",
                nova::rhi::stats::TimeSubmitting.exchange(0) / divisor,
                nova::rhi::stats::TimeAdaptingFromAcquire.exchange(0)  / divisor,
                nova::rhi::stats::TimeAdaptingToPresent.exchange(0)  / divisor,
                nova::rhi::stats::TimePresenting.exchange(0) / divisor);

            last_time = std::chrono::steady_clock::now();
            frames = 0;
        }

// -----------------------------------------------------------------------------

        if (!skip_update) {
            auto MoveBox = [&](nova::draw::Rectangle& box, nova::VirtualKey left, nova::VirtualKey right, nova::VirtualKey up, nova::VirtualKey down) {
                float speed = 1000.f * delta_time;
                if (app.IsVirtualKeyDown(left))  { box.center_pos.x -= speed; redraw = true; }
                if (app.IsVirtualKeyDown(right)) { box.center_pos.x += speed; redraw = true; }
                if (app.IsVirtualKeyDown(up))    { box.center_pos.y -= speed; redraw = true; }
                if (app.IsVirtualKeyDown(down))  { box.center_pos.y += speed; redraw = true; }
            };

            MoveBox(box1, nova::VirtualKey::A, nova::VirtualKey::D, nova::VirtualKey::W, nova::VirtualKey::S);
            MoveBox(box2, nova::VirtualKey::J, nova::VirtualKey::L, nova::VirtualKey::I, nova::VirtualKey::K);
            MoveBox(box3, nova::VirtualKey::Left, nova::VirtualKey::Right, nova::VirtualKey::Up, nova::VirtualKey::Down);
        }
        else {
            redraw = true;
        }

        skip_update = false;

        if (!redraw) {
            app.WaitForEvents();
            skip_update = true;
        }
        redraw = false;

// -----------------------------------------------------------------------------

        im_draw.Reset();

        im_draw.DrawRect(box1);
        im_draw.DrawRect(box2);
        im_draw.DrawRect(box3);

        im_draw.DrawString(
            "C:/Program Files (x86)/Steam/steamapps/common/BeamNG.drive/BeamNG.drive.exe",
            Vec2(size.x * 0.25f, size.y * 0.4f),
            *font);

// -----------------------------------------------------------------------------

        // Record frame

        queue.WaitIdle();

        auto cmd = queue.Begin();

        // Update window size, record primary buffer and present

        // TODO: Investigate integratino of this and/or a manual swapchain implementation?
        // DCompositionWaitForCompositorClock(0, nullptr, INFINITE);

        u32 w = u32(im_draw.Bounds().Width());
        u32 h = u32(im_draw.Bounds().Height());

        if (!target || target.Extent() != Vec3U(w, h, 1)) {
            nova::Log("Recreating target image, size = ({}, {})", w, h);
            target.Destroy();
            target = nova::Image::Create(context, {w, h, 0},
                nova::ImageUsage::Storage,
                nova::Format::BGRA8_UNorm,
                nova::ImageFlags::None);

            auto alignment = context.Properties().min_imported_host_pointer_alignment;
            usz buffer_size = w * h * 4;

            for(;;) {
                u32 file_offset = 0;

                if (file_map_bitmap) {
                    usz mapped_file_size = nova::AlignUpPower2(buffer_size, alignment) + alignment;

                    if (file) {
                        CloseHandle(file);
                    }

                    file = CreateFileMapping2(nullptr,  // File
                        nullptr,                        // Security Attributes
                        FILE_MAP_READ | FILE_MAP_WRITE, // Desired Access
                        PAGE_READWRITE,                 // Page Protection
                        SEC_COMMIT,                     // Allocation Flags
                        mapped_file_size,               // Size
                        nullptr,                        // Name
                        nullptr, 0);                    // Extended params

                    if (!file) {
                        auto err = GetLastError();
                        nova::Log("Error creating file mapping: ({}) {}", err, nova::win::HResultToString(HRESULT_FROM_WIN32(err)));
                    }

                    auto address = MapViewOfFile(file, FILE_MAP_ALL_ACCESS, 0, 0, mapped_file_size);
                    if (!address) {
                        nova::Log("Error mapping file: {}", nova::win::LastErrorString());
                    }

                    file_offset = u32(nova::AlignUpPower2(uintptr_t(address), alignment) - uintptr_t(address));
                    nova::Log("file_offset = {}", file_offset);

                    UnmapViewOfFile(address);
                }

                if (bitmap) {
                    DeleteObject(bitmap);
                }

                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = int(w);
                bmi.bmiHeader.biHeight = -int(h);
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 32;
                bmi.bmiHeader.biCompression = BI_RGB;
                bitmap = CreateDIBSection(hdc_screen, &bmi, DIB_RGB_COLORS, &bits, file, file_offset);

                if (!bitmap) {
                    nova::Log("Error creating DIB section: {}", nova::win::LastErrorString());
                }

                if (nova::IsAlignedTo(bits, alignment)) {
                    // memory is aligned, ready for import
                    break;
                } else {
                    NOVA_ASSERT(!file_map_bitmap, "Alignment should not fail when file-mapping");
                    nova::Log("CreateDIBSection did not meet Vulkan memory import alignment restrictions, falling back to memory mapped file backing");
                    file_map_bitmap = true;
                    continue;
                }
            }

            // Import BITMAP memory into a buffer

            buffer.Destroy();
            buffer = nova::Buffer::Create(context, buffer_size, {}, nova::BufferFlags::ImportHost, bits);
        }

        // window.SetSize({ w, h }, nova::WindowPart::Client);
        // queue.Acquire({swapchain});
        // auto target = swapchain.Target();

        Vec3 color = { 0.3f, 0.2f, 0.5f };
        f32 alpha = 0.4f;
        cmd.ClearColor(target, Vec4(color * alpha, alpha));

        im_draw.Record(cmd, target);

        // cmd.Present(swapchain);

        cmd.Barrier(nova::PipelineStage::Graphics, nova::PipelineStage::Transfer);
        cmd.CopyFromImage(buffer, target, {{}, {w, h}});

        queue.Submit({cmd}, {}).Wait();

        // window.SetPosition({ im_draw.Bounds().min.x, im_draw.Bounds().min.y }, nova::WindowPart::Client);
        // queue.Present({swapchain}, {});

        {
            HDC hdc = CreateCompatibleDC(hdc_screen);
            NOVA_DEFER(&) { DeleteObject(hdc); };

            auto old_object = SelectObject(hdc, bitmap);
            NOVA_DEFER(&) { SelectObject(hdc, old_object); };

            if (!UpdateLayeredWindow(
                    HWND(window.NativeHandle()),
                    hdc_screen,
                    nova::PtrTo(POINT{LONG(im_draw.Bounds().min.x), LONG(im_draw.Bounds().min.y)}),
                    nova::PtrTo(SIZE{int(w), int(h)}),
                    hdc,
                    nova::PtrTo(POINT{0, 0}),
                    0,
                    nova::PtrTo(BLENDFUNCTION{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA}),
                    ULW_ALPHA)) {
                nova::Log(nova::win::LastErrorString());
            }
        }
    }
}
