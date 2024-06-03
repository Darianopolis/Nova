#include "nova_VulkanGDISwapchain.hpp"

#pragma warning(push)
#pragma warning(disable: 4263)
#include <dcomp.h>
#pragma warning(pop)

namespace nova
{
    struct GDISwapchainStrategy : SwapchainStrategy
    {
        static
        void ClearResources(GDISwapchainData* swapchain)
        {
            if (swapchain->file) {
                CloseHandle(swapchain->file);
            }

            if (swapchain->bitmap) {
                DeleteObject(swapchain->bitmap);
            }

            swapchain->image.Destroy();
            swapchain->buffer.Destroy();
        }

        virtual void Destroy(Swapchain swapchain) final override
        {
            if (!swapchain) {
                return;
            }

            auto* impl = static_cast<GDISwapchainData*>(swapchain.operator->());

            ClearResources(impl);

            delete impl;
        }

        virtual Image Target(Swapchain swapchain) final override
        {
            auto* data = GDISwapchainData::Get(swapchain);
            return data->image;
        }

        virtual Vec2U Extent(Swapchain swapchain) final override
        {
            auto* data = GDISwapchainData::Get(swapchain);
            return Vec2U(data->image.Extent());
        }

        virtual nova::Format Format(Swapchain swapchain) final override
        {
            NOVA_IGNORE(swapchain);
            return nova::Format::BGRA8_UNorm;
        }

        virtual SyncPoint Acquire(Queue, Span<HSwapchain> swapchains, bool* out_any_resized) final override
        {
            bool any_resized = false;

            for (auto& _swapchain : swapchains) {
                auto* swapchain = static_cast<GDISwapchainData*>(_swapchain.operator->());

                Vec2U new_size = swapchain->window->size;

                if (!swapchain->image || Vec2U(swapchain->image.Extent()) != new_size) {
                    // Log("GDISwapchain resized from ({}, {}) to ({}, {})",
                    //     swapchain->image ? swapchain->image.Extent().x : -1,
                    //     swapchain->image ? swapchain->image.Extent().y : -1,
                    //     new_size.x, new_size.y);

                    any_resized = true;
                    ClearResources(swapchain);

                    swapchain->image = nova::Image::Create(swapchain->context, {new_size.x, new_size.y, 0},
                        swapchain->usage,
                        nova::Format::BGRA8_UNorm,
                        nova::ImageFlags::None);

                    swapchain->bitmap_size = new_size + Vec2U(1);

                    auto alignment = swapchain->context.Properties().min_imported_host_pointer_alignment;
                    usz buffer_size = swapchain->bitmap_size.x * swapchain->bitmap_size.y * 4;

                    for(;;) {
                        u32 file_offset = 0;

                        if (swapchain->file_map_bitmap) {
                            usz mapped_file_size = nova::AlignUpPower2(buffer_size, alignment) + alignment;

                            swapchain->file = CreateFileMapping2(nullptr,  // File
                                nullptr,                        // Security Attributes
                                FILE_MAP_READ | FILE_MAP_WRITE, // Desired Access
                                PAGE_READWRITE,                 // Page Protection
                                SEC_COMMIT,                     // Allocation Flags
                                mapped_file_size,               // Size
                                nullptr,                        // Name
                                nullptr, 0);                    // Extended params

                            if (!swapchain->file) {
                                auto err = GetLastError();
                                NOVA_THROW("Error creating file mapping: ({}) {}", err, nova::win::HResultToString(HRESULT_FROM_WIN32(err)));
                            }

                            auto address = MapViewOfFile(swapchain->file, FILE_MAP_ALL_ACCESS, 0, 0, mapped_file_size);
                            if (!address) {
                                NOVA_THROW("Error mapping file: {}", nova::win::LastErrorString());
                            }

                            file_offset = u32(nova::AlignUpPower2(uintptr_t(address), alignment) - uintptr_t(address));

                            UnmapViewOfFile(address);
                        }

                        BITMAPINFO bmi = {};
                        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                        bmi.bmiHeader.biWidth = int(swapchain->bitmap_size.x);
                        bmi.bmiHeader.biHeight = -int(swapchain->bitmap_size.y);
                        bmi.bmiHeader.biPlanes = 1;
                        bmi.bmiHeader.biBitCount = 32;
                        bmi.bmiHeader.biCompression = BI_RGB;
                        swapchain->bitmap = CreateDIBSection(swapchain->hdc_screen, &bmi, DIB_RGB_COLORS, &swapchain->bits, swapchain->file, file_offset);

                        if (!swapchain->bitmap) {
                            NOVA_THROW("Error creating DIB section: {}", nova::win::LastErrorString());
                        }

                        // Clear bottom and right edges of bitmap as empty "nudge space"
                        // TODO: Need to do this on reducing size of "swapchain" image relative to buffer
                        std::memset(nova::ByteOffsetPointer(swapchain->bits, new_size.y * swapchain->bitmap_size.x * 4),
                            0, swapchain->bitmap_size.x * 4);
                        for (u32 y = 0; y < swapchain->bitmap_size.y; ++y) {
                            ((u32*)swapchain->bits)[y * swapchain->bitmap_size.x + new_size.x] = 0;
                        }

                        if (nova::IsAlignedTo(swapchain->bits, alignment)) {
                            // memory is aligned, ready for import
                            break;
                        } else {
                            NOVA_ASSERT(!swapchain->file_map_bitmap, "Alignment should not fail when file-mapping");
                            nova::Log("CreateDIBSection did not meet Vulkan memory import alignment restrictions, falling back to memory mapped file backing");
                            swapchain->file_map_bitmap = true;
                            continue;
                        }
                    }

                    // Import BITMAP memory into a buffer

                    swapchain->buffer = nova::Buffer::Create(swapchain->context, buffer_size, {}, nova::BufferFlags::ImportHost, swapchain->bits);
                }
            }

            if (out_any_resized) {
                *out_any_resized = any_resized;
            }

            return {};
        }

        virtual void PreparePresent(CommandList cmd, HSwapchain _swapchain) final override
        {
            auto* swapchain = static_cast<GDISwapchainData*>(_swapchain.operator->());
            // TODO: Make this more granular
            cmd.Barrier(nova::PipelineStage::All, nova::PipelineStage::Transfer);
            cmd.CopyFromImage(swapchain->buffer, swapchain->image, {{}, Vec2U(swapchain->image.Extent())}, swapchain->bitmap_size.x);
        }

        virtual void Present(Queue queue, Span<HSwapchain> swapchains, Span<SyncPoint> waits, PresentFlag flags) final override
        {
            NOVA_IGNORE(queue);
            NOVA_IGNORE(flags);

            for (auto& wait : waits) {
                wait.Wait();
            }

            queue.WaitIdle();

            // Wait for compositor
            DCompositionWaitForCompositorClock(0, nullptr, INFINITE);

            for (auto& _swapchain : swapchains) {
                auto* swapchain = static_cast<GDISwapchainData*>(_swapchain.operator->());

                auto size = Vec2U(swapchain->image.Extent());

                if (size != swapchain->window->size) {
                    nova::Log("Size has changed since Acquire, new size won't be applied until next Present!");
                }

                auto window_size = size;

                // If window has moved but not resized, move the edges to force windows to sync move with content update
                if (swapchain->last_pos.x != swapchain->window->position.x && swapchain->last_size.x == size.x) window_size.x++;
                if (swapchain->last_pos.y != swapchain->window->position.y && swapchain->last_size.y == size.y) window_size.y++;

                swapchain->last_pos = swapchain->window->position;
                swapchain->last_size = window_size;

                // Create a device context to use as our update source
                HDC hdc = CreateCompatibleDC(swapchain->hdc_screen);
                NOVA_DEFER(&) { DeleteObject(hdc); };

                // Set BITMAP as device context contents
                auto old_object = SelectObject(hdc, swapchain->bitmap);
                NOVA_DEFER(&) { SelectObject(hdc, old_object); };

                // Perform (nearly) atomic move/size/repaint
                // > This still isn't *perfectly* atomic. Occasionally (frequency depending
                //   on window size: small = very infrequent, larger = more frequent and
                //   more severe) windows will process a move out of sync with presentation
                //   which causes flickering of contents that is positioned independently
                //   of the window moving.
                if (!UpdateLayeredWindow(
                        (HWND)swapchain->window.NativeHandle(),
                        swapchain->hdc_screen,
                        PtrTo(POINT{LONG(swapchain->last_pos.x), LONG(swapchain->last_pos.y)}),
                        PtrTo(SIZE{int(window_size.x), int(window_size.y)}),
                        hdc,
                        PtrTo(POINT{0, 0}),
                        0,
                        PtrTo(BLENDFUNCTION{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA}),
                        ULW_ALPHA)) {
                    NOVA_THROW(win::LastErrorString());
                }
            }
        }
    };

    GDISwapchainStrategy* GetGDISwapchainStrategy()
    {
        static GDISwapchainStrategy strategy;
        return &strategy;
    }

    Swapchain GDISwapchain_Create(HContext context, Window window, ImageUsage usage)
    {
        auto impl = new GDISwapchainData;
        impl->strategy = GetGDISwapchainStrategy();
        impl->context = context;
        impl->usage = usage;
        impl->window = window;

        window->swapchain_handles_move_size = true;

        return { impl };
    }
}