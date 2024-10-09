#include "ImGui.hpp"
#include "ImGui.slang"

namespace nova::imgui
{
    void ImGui_ImplNova_NewFrame(ImGuiLayer& layer)
    {
        auto& io = ImGui::GetIO();

        {
            // Update Display Size

            auto size = layer.window.Size(WindowPart::Client);
            io.DisplaySize.x = f32(size.x);
            io.DisplaySize.y = f32(size.y);
        }

        {
            // Update delta time

            auto time = std::chrono::steady_clock::now();
            auto delta = time - layer.last_time;
            io.DeltaTime = std::min(std::chrono::duration_cast<std::chrono::duration<float>>(delta).count(), 1.f); // Ignore any time > 1s
            layer.last_time = time;
        }

        if (!(io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) && io.WantCaptureMouse) {

            // Update mouse cursor

            auto imgui_cursor = ImGui::GetMouseCursor();

            if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor) {
                layer.window.SetCursor(Cursor::None);
            } else {
                switch (imgui_cursor) {
                    break;case ImGuiMouseCursor_None:       layer.window.SetCursor(Cursor::None);
                    break;case ImGuiMouseCursor_Arrow:      layer.window.SetCursor(Cursor::Arrow);
                    break;case ImGuiMouseCursor_TextInput:  layer.window.SetCursor(Cursor::IBeam);
                    break;case ImGuiMouseCursor_ResizeAll:  layer.window.SetCursor(Cursor::ResizeAll);
                    break;case ImGuiMouseCursor_ResizeNS:   layer.window.SetCursor(Cursor::ResizeNS);
                    break;case ImGuiMouseCursor_ResizeEW:   layer.window.SetCursor(Cursor::ResizeEW);
                    break;case ImGuiMouseCursor_ResizeNESW: layer.window.SetCursor(Cursor::ResizeNESW);
                    break;case ImGuiMouseCursor_ResizeNWSE: layer.window.SetCursor(Cursor::ResizeNWSE);
                    break;case ImGuiMouseCursor_Hand:       layer.window.SetCursor(Cursor::Hand);
                    break;case ImGuiMouseCursor_NotAllowed: layer.window.SetCursor(Cursor::NotAllowed);
                }
            }
        }
    }

    void ImGui_ImplNova_Init(ImGuiLayer& layer)
    {
        auto app = layer.window.Application();

        app.AddCallback([&layer, app](const AppEvent& event) {

            if (event.window != layer.window) {
                return;
            }

            auto& io = ImGui::GetIO();

            switch (event.type) {
                break;case EventType::Input:
                    {
                        auto vk = app.ToVirtualKey(event.input.channel);

                        switch (vk) {
                            break;case VirtualKey::Shift:
                                  case VirtualKey::LeftShift:
                                  case VirtualKey::RightShift:
                                io.AddKeyEvent(ImGuiMod_Shift, app.IsVirtualKeyDown(VirtualKey::LeftShift) || app.IsVirtualKeyDown(VirtualKey::RightShift));

                            break;case VirtualKey::Control:
                                  case VirtualKey::LeftControl:
                                  case VirtualKey::RightControl:
                                io.AddKeyEvent(ImGuiMod_Ctrl, app.IsVirtualKeyDown(VirtualKey::LeftControl) || app.IsVirtualKeyDown(VirtualKey::RightControl));

                            break;case VirtualKey::Alt:
                                  case VirtualKey::LeftAlt:
                                  case VirtualKey::RightAlt:
                                io.AddKeyEvent(ImGuiMod_Alt, app.IsVirtualKeyDown(VirtualKey::LeftAlt) || app.IsVirtualKeyDown(VirtualKey::RightAlt));

                            break;case VirtualKey::Super:
                                  case VirtualKey::LeftSuper:
                                  case VirtualKey::RightSuper:
                                io.AddKeyEvent(ImGuiMod_Super, app.IsVirtualKeyDown(VirtualKey::LeftSuper) || app.IsVirtualKeyDown(VirtualKey::RightSuper));
                        }

                        switch (vk) {
                            break;case VirtualKey::MousePrimary:   io.AddMouseButtonEvent(ImGuiMouseButton_Left, event.input.pressed);
                            break;case VirtualKey::MouseSecondary: io.AddMouseButtonEvent(ImGuiMouseButton_Right, event.input.pressed);
                            break;case VirtualKey::MouseMiddle:    io.AddMouseButtonEvent(ImGuiMouseButton_Middle, event.input.pressed);

                            break;case VirtualKey::Tab: io.AddKeyEvent(ImGuiKey_Tab, event.input.pressed);

                            break;case VirtualKey::Left:  io.AddKeyEvent(ImGuiKey_LeftArrow, event.input.pressed);
                            break;case VirtualKey::Right: io.AddKeyEvent(ImGuiKey_RightArrow, event.input.pressed);
                            break;case VirtualKey::Up:    io.AddKeyEvent(ImGuiKey_UpArrow, event.input.pressed);
                            break;case VirtualKey::Down:  io.AddKeyEvent(ImGuiKey_DownArrow, event.input.pressed);

                            break;case VirtualKey::PageUp:    io.AddKeyEvent(ImGuiKey_PageUp, event.input.pressed);
                            break;case VirtualKey::PageDown:  io.AddKeyEvent(ImGuiKey_PageDown, event.input.pressed);
                            break;case VirtualKey::Home:      io.AddKeyEvent(ImGuiKey_Home, event.input.pressed);
                            break;case VirtualKey::End:       io.AddKeyEvent(ImGuiKey_End, event.input.pressed);
                            break;case VirtualKey::Insert:    io.AddKeyEvent(ImGuiKey_Insert, event.input.pressed);
                            break;case VirtualKey::Delete:    io.AddKeyEvent(ImGuiKey_Delete, event.input.pressed);
                            break;case VirtualKey::Backspace: io.AddKeyEvent(ImGuiKey_Backspace, event.input.pressed);
                            break;case VirtualKey::Space:     io.AddKeyEvent(ImGuiKey_Space, event.input.pressed);
                            break;case VirtualKey::Enter:     io.AddKeyEvent(ImGuiKey_Enter, event.input.pressed);
                            break;case VirtualKey::Escape:    io.AddKeyEvent(ImGuiKey_Escape, event.input.pressed);

                            break;case VirtualKey::LeftShift:   io.AddKeyEvent(ImGuiKey_LeftShift, event.input.pressed);
                            break;case VirtualKey::LeftControl: io.AddKeyEvent(ImGuiKey_LeftCtrl, event.input.pressed);
                            break;case VirtualKey::LeftAlt:     io.AddKeyEvent(ImGuiKey_LeftAlt, event.input.pressed);
                            break;case VirtualKey::LeftSuper:   io.AddKeyEvent(ImGuiKey_LeftSuper, event.input.pressed);

                            break;case VirtualKey::RightShift:   io.AddKeyEvent(ImGuiKey_RightShift, event.input.pressed);
                            break;case VirtualKey::RightControl: io.AddKeyEvent(ImGuiKey_RightCtrl, event.input.pressed);
                            break;case VirtualKey::RightAlt:     io.AddKeyEvent(ImGuiKey_RightAlt, event.input.pressed);
                            break;case VirtualKey::RightSuper:   io.AddKeyEvent(ImGuiKey_RightSuper, event.input.pressed);

                            break;case VirtualKey::_0:  io.AddKeyEvent(ImGuiKey_0,   event.input.pressed);
                            break;case VirtualKey::_1:  io.AddKeyEvent(ImGuiKey_1,   event.input.pressed);
                            break;case VirtualKey::_2:  io.AddKeyEvent(ImGuiKey_2,   event.input.pressed);
                            break;case VirtualKey::_3:  io.AddKeyEvent(ImGuiKey_3,   event.input.pressed);
                            break;case VirtualKey::_4:  io.AddKeyEvent(ImGuiKey_4,   event.input.pressed);
                            break;case VirtualKey::_5:  io.AddKeyEvent(ImGuiKey_5,   event.input.pressed);
                            break;case VirtualKey::_6:  io.AddKeyEvent(ImGuiKey_6,   event.input.pressed);
                            break;case VirtualKey::_7:  io.AddKeyEvent(ImGuiKey_7,   event.input.pressed);
                            break;case VirtualKey::_8:  io.AddKeyEvent(ImGuiKey_8,   event.input.pressed);
                            break;case VirtualKey::_9:  io.AddKeyEvent(ImGuiKey_9,   event.input.pressed);
                            break;case VirtualKey::A:   io.AddKeyEvent(ImGuiKey_A,   event.input.pressed);
                            break;case VirtualKey::B:   io.AddKeyEvent(ImGuiKey_B,   event.input.pressed);
                            break;case VirtualKey::C:   io.AddKeyEvent(ImGuiKey_C,   event.input.pressed);
                            break;case VirtualKey::D:   io.AddKeyEvent(ImGuiKey_D,   event.input.pressed);
                            break;case VirtualKey::E:   io.AddKeyEvent(ImGuiKey_E,   event.input.pressed);
                            break;case VirtualKey::F:   io.AddKeyEvent(ImGuiKey_F,   event.input.pressed);
                            break;case VirtualKey::G:   io.AddKeyEvent(ImGuiKey_G,   event.input.pressed);
                            break;case VirtualKey::H:   io.AddKeyEvent(ImGuiKey_H,   event.input.pressed);
                            break;case VirtualKey::I:   io.AddKeyEvent(ImGuiKey_I,   event.input.pressed);
                            break;case VirtualKey::J:   io.AddKeyEvent(ImGuiKey_J,   event.input.pressed);
                            break;case VirtualKey::K:   io.AddKeyEvent(ImGuiKey_K,   event.input.pressed);
                            break;case VirtualKey::L:   io.AddKeyEvent(ImGuiKey_L,   event.input.pressed);
                            break;case VirtualKey::M:   io.AddKeyEvent(ImGuiKey_M,   event.input.pressed);
                            break;case VirtualKey::N:   io.AddKeyEvent(ImGuiKey_N,   event.input.pressed);
                            break;case VirtualKey::O:   io.AddKeyEvent(ImGuiKey_O,   event.input.pressed);
                            break;case VirtualKey::P:   io.AddKeyEvent(ImGuiKey_P,   event.input.pressed);
                            break;case VirtualKey::Q:   io.AddKeyEvent(ImGuiKey_Q,   event.input.pressed);
                            break;case VirtualKey::R:   io.AddKeyEvent(ImGuiKey_R,   event.input.pressed);
                            break;case VirtualKey::S:   io.AddKeyEvent(ImGuiKey_S,   event.input.pressed);
                            break;case VirtualKey::T:   io.AddKeyEvent(ImGuiKey_T,   event.input.pressed);
                            break;case VirtualKey::U:   io.AddKeyEvent(ImGuiKey_U,   event.input.pressed);
                            break;case VirtualKey::V:   io.AddKeyEvent(ImGuiKey_V,   event.input.pressed);
                            break;case VirtualKey::W:   io.AddKeyEvent(ImGuiKey_W,   event.input.pressed);
                            break;case VirtualKey::X:   io.AddKeyEvent(ImGuiKey_X,   event.input.pressed);
                            break;case VirtualKey::Y:   io.AddKeyEvent(ImGuiKey_Y,   event.input.pressed);
                            break;case VirtualKey::Z:   io.AddKeyEvent(ImGuiKey_Z,   event.input.pressed);
                            break;case VirtualKey::F1:  io.AddKeyEvent(ImGuiKey_F1,  event.input.pressed);
                            break;case VirtualKey::F2:  io.AddKeyEvent(ImGuiKey_F2,  event.input.pressed);
                            break;case VirtualKey::F3:  io.AddKeyEvent(ImGuiKey_F3,  event.input.pressed);
                            break;case VirtualKey::F4:  io.AddKeyEvent(ImGuiKey_F4,  event.input.pressed);
                            break;case VirtualKey::F5:  io.AddKeyEvent(ImGuiKey_F5,  event.input.pressed);
                            break;case VirtualKey::F6:  io.AddKeyEvent(ImGuiKey_F6,  event.input.pressed);
                            break;case VirtualKey::F7:  io.AddKeyEvent(ImGuiKey_F7,  event.input.pressed);
                            break;case VirtualKey::F8:  io.AddKeyEvent(ImGuiKey_F8,  event.input.pressed);
                            break;case VirtualKey::F9:  io.AddKeyEvent(ImGuiKey_F9,  event.input.pressed);
                            break;case VirtualKey::F10: io.AddKeyEvent(ImGuiKey_F10, event.input.pressed);
                            break;case VirtualKey::F11: io.AddKeyEvent(ImGuiKey_F11, event.input.pressed);
                            break;case VirtualKey::F12: io.AddKeyEvent(ImGuiKey_F12, event.input.pressed);
                            break;case VirtualKey::F13: io.AddKeyEvent(ImGuiKey_F13, event.input.pressed);
                            break;case VirtualKey::F14: io.AddKeyEvent(ImGuiKey_F14, event.input.pressed);
                            break;case VirtualKey::F15: io.AddKeyEvent(ImGuiKey_F15, event.input.pressed);
                            break;case VirtualKey::F16: io.AddKeyEvent(ImGuiKey_F16, event.input.pressed);
                            break;case VirtualKey::F17: io.AddKeyEvent(ImGuiKey_F17, event.input.pressed);
                            break;case VirtualKey::F18: io.AddKeyEvent(ImGuiKey_F18, event.input.pressed);
                            break;case VirtualKey::F19: io.AddKeyEvent(ImGuiKey_F19, event.input.pressed);
                            break;case VirtualKey::F20: io.AddKeyEvent(ImGuiKey_F20, event.input.pressed);
                            break;case VirtualKey::F21: io.AddKeyEvent(ImGuiKey_F21, event.input.pressed);
                            break;case VirtualKey::F22: io.AddKeyEvent(ImGuiKey_F22, event.input.pressed);
                            break;case VirtualKey::F23: io.AddKeyEvent(ImGuiKey_F23, event.input.pressed);
                            break;case VirtualKey::F24: io.AddKeyEvent(ImGuiKey_F24, event.input.pressed);

                            break;case VirtualKey::Apostrophe:   io.AddKeyEvent(ImGuiKey_Apostrophe,   event.input.pressed);
                            break;case VirtualKey::Comma:        io.AddKeyEvent(ImGuiKey_Comma,        event.input.pressed);
                            break;case VirtualKey::Minus:        io.AddKeyEvent(ImGuiKey_Minus,        event.input.pressed);
                            break;case VirtualKey::Period:       io.AddKeyEvent(ImGuiKey_Period,       event.input.pressed);
                            break;case VirtualKey::Slash:        io.AddKeyEvent(ImGuiKey_Slash,        event.input.pressed);
                            break;case VirtualKey::Semicolon:    io.AddKeyEvent(ImGuiKey_Semicolon,    event.input.pressed);
                            break;case VirtualKey::Equal:        io.AddKeyEvent(ImGuiKey_Equal,        event.input.pressed);
                            break;case VirtualKey::LeftBracket:  io.AddKeyEvent(ImGuiKey_LeftBracket,  event.input.pressed);
                            break;case VirtualKey::Backslash:    io.AddKeyEvent(ImGuiKey_Backslash,    event.input.pressed);
                            break;case VirtualKey::RightBracket: io.AddKeyEvent(ImGuiKey_RightBracket, event.input.pressed);
                            break;case VirtualKey::CapsLock:     io.AddKeyEvent(ImGuiKey_CapsLock,     event.input.pressed);
                            break;case VirtualKey::ScrollLock:   io.AddKeyEvent(ImGuiKey_ScrollLock,   event.input.pressed);
                            break;case VirtualKey::NumLock:      io.AddKeyEvent(ImGuiKey_NumLock,      event.input.pressed);
                            break;case VirtualKey::PrintScreen:  io.AddKeyEvent(ImGuiKey_PrintScreen,  event.input.pressed);
                            break;case VirtualKey::Pause:        io.AddKeyEvent(ImGuiKey_Pause,        event.input.pressed);

                            break;case VirtualKey::Num0: io.AddKeyEvent(ImGuiKey_Keypad0, event.input.pressed);
                            break;case VirtualKey::Num1: io.AddKeyEvent(ImGuiKey_Keypad1, event.input.pressed);
                            break;case VirtualKey::Num2: io.AddKeyEvent(ImGuiKey_Keypad2, event.input.pressed);
                            break;case VirtualKey::Num3: io.AddKeyEvent(ImGuiKey_Keypad3, event.input.pressed);
                            break;case VirtualKey::Num4: io.AddKeyEvent(ImGuiKey_Keypad4, event.input.pressed);
                            break;case VirtualKey::Num5: io.AddKeyEvent(ImGuiKey_Keypad5, event.input.pressed);
                            break;case VirtualKey::Num6: io.AddKeyEvent(ImGuiKey_Keypad6, event.input.pressed);
                            break;case VirtualKey::Num7: io.AddKeyEvent(ImGuiKey_Keypad7, event.input.pressed);
                            break;case VirtualKey::Num8: io.AddKeyEvent(ImGuiKey_Keypad8, event.input.pressed);
                            break;case VirtualKey::Num9: io.AddKeyEvent(ImGuiKey_Keypad9, event.input.pressed);

                            break;case VirtualKey::NumDecimal:   io.AddKeyEvent(ImGuiKey_KeypadDecimal,  event.input.pressed);
                            break;case VirtualKey::NumDivide:    io.AddKeyEvent(ImGuiKey_KeypadDivide,   event.input.pressed);
                            break;case VirtualKey::NumMultiply:  io.AddKeyEvent(ImGuiKey_KeypadMultiply, event.input.pressed);
                            break;case VirtualKey::NumSubtract:  io.AddKeyEvent(ImGuiKey_KeypadSubtract, event.input.pressed);
                            break;case VirtualKey::NumAdd:       io.AddKeyEvent(ImGuiKey_KeypadAdd,      event.input.pressed);
                            // break;case VirtualKey::NumEnter:  io.AddKeyEvent(ImGuiKey_KeypadEnter,    event.input.pressed);
                            break;case VirtualKey::NumEqual:     io.AddKeyEvent(ImGuiKey_KeypadEqual,    event.input.pressed);
                            break;case VirtualKey::MouseBack:    io.AddKeyEvent(ImGuiKey_AppBack,        event.input.pressed);
                            break;case VirtualKey::MouseForward: io.AddKeyEvent(ImGuiKey_AppForward,     event.input.pressed);
                        }
                    }
                break;case EventType::MouseMove:
                    io.AddMousePosEvent(event.mouse_move.position.x, event.mouse_move.position.y);

                break;case EventType::MouseScroll:
                    io.AddMouseWheelEvent(event.scroll.scrolled.x, event.scroll.scrolled.y);

                break;case EventType::Text:
                    io.AddInputCharactersUTF8(event.text.text);

                break;case EventType::WindowFocus:
                    if      (event.focus.gaining == layer.window) io.AddFocusEvent(true);
                    else if (event.focus.losing  == layer.window) io.AddFocusEvent(false);
            }
        });
    }

    ImGuiLayer::ImGuiLayer(const ImGuiConfig& config)
        : context(config.context)
        , default_sampler(config.sampler)
        , frames_in_flight(config.frames_in_flight)
    {
        NOVA_ASSERT(frames_in_flight != 0, "frames_in_flight must be greater than 0!");

        for (u32 i = 0; i < frames_in_flight; ++i) {
            frame_data.push_back(FrameData {
                .vertex_buffer = Buffer::Create(context, 0,
                    BufferUsage::Storage,
                    BufferFlags::DeviceLocal | BufferFlags::Mapped),

                .index_buffer = Buffer::Create(context, 0,
                    BufferUsage::Index,
                    BufferFlags::DeviceLocal | BufferFlags::Mapped),
            });
        }

        vertex_shader   = Shader::Create(context, ShaderLang::Slang, ShaderStage::Vertex,   "Vertex",   "nova/ui/ImGui.slang");
        fragment_shader = Shader::Create(context, ShaderLang::Slang, ShaderStage::Fragment, "Fragment", "nova/ui/ImGui.slang");

        // Create ImGui context and initialize

        imgui_ctx = ImGui::CreateContext();
        last_imgui_ctx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imgui_ctx);

        auto& io = ImGui::GetIO();
        io.ConfigFlags |= config.flags;
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

        window = config.window;
        ImGui_ImplNova_Init(*this);

        // Rescale UI and fonts

        ImGui::GetStyle().ScaleAllSizes(config.ui_scale);
        auto font_config = ImFontConfig();
        font_config.GlyphOffset = ImVec2(config.glyph_offset.x, config.glyph_offset.y);
        ImGui::GetIO().Fonts->ClearFonts();

        font_config.FontDataOwnedByAtlas = false;
        ImGui::GetIO().Fonts->AddFontFromMemoryTTF(const_cast<b8*>(config.font.data()), int(config.font.size()), config.font_size, &font_config);

        {
            // Upload font

            unsigned char* pixels;
            int width, height;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

            font_image = Image::Create(context,
                { u32(width), u32(height), 0u },
                ImageUsage::Sampled,
                Format::RGBA8_UNorm, {});
            io.Fonts->SetTexID(TextureID(font_image));

            font_image.Set({}, font_image.Extent(), pixels);
            font_image.Transition(ImageLayout::Sampled);
        }

        ImGui::SetCurrentContext(last_imgui_ctx);
    }

    ImGuiLayer::~ImGuiLayer()
    {
        last_imgui_ctx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imgui_ctx);
        ImGui::SetCurrentContext(last_imgui_ctx);
        ImGui::DestroyContext(imgui_ctx);

        for (auto[vertex_buffer, index_buffer] : frame_data) {
            vertex_buffer.Destroy();
            index_buffer.Destroy();
        }
        vertex_shader.Destroy();
        fragment_shader.Destroy();
        font_image.Destroy();
    }

    void ImGuiLayer::BeginFrame(FunctionRef<void()> fn)
    {
        last_imgui_ctx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imgui_ctx);

        ImGui_ImplNova_NewFrame(*this);
        ImGui::NewFrame();

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiDockNodeFlags dockspace_flags = 0;
            ImGuiWindowFlags dockspace_window_flags = 0;

            // Make dockspace fullscreen
            auto viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
            dockspace_window_flags
                |= ImGuiWindowFlags_NoTitleBar
                | ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoBringToFrontOnFocus
                | ImGuiWindowFlags_NoNavFocus;

            if (dock_menu_bar) {
                dockspace_window_flags |= ImGuiWindowFlags_MenuBar;
            }

            if (no_dock_bg) {
                // Pass through background
                dockspace_window_flags |= ImGuiWindowFlags_NoBackground;
                dockspace_flags |= ImGuiDockNodeFlags_NoDockingOverCentralNode;
            }

            // Register dockspace
            ImGui::Begin("Dockspace", PtrTo(true), dockspace_window_flags);
            ImGui::PopStyleVar(3);
            ImGui::DockSpace(ImGui::GetID("DockspaceID"), ImVec2(0.f, 0.f), dockspace_flags);

            fn();

            ImGui::End();

        }
        ended = false;
    }

    void ImGuiLayer::EndFrame()
    {
        if (!ended) {
            ImGui::Render();
            ended = true;

            if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }

            ImGui::SetCurrentContext(last_imgui_ctx);
        }
    }

    bool ImGuiLayer::HasDrawData()
    {
        return ended;
    }

    void ImGuiLayer::DrawFrame(CommandList cmd, Image target)
    {
        EndFrame();

        last_imgui_ctx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imgui_ctx);
        NOVA_DEFER(&) { ImGui::SetCurrentContext(last_imgui_ctx); };

        auto data = ImGui::GetDrawData();
        if (!data->TotalIdxCount) {
            return;
        }

        // Fetch frame buffers and ensure sizes

        auto&[vertex_buffer, index_buffer] = frame_data[frame_index];
        frame_index = (frame_index + 1) % frames_in_flight;

        vertex_buffer.Resize(data->TotalVtxCount * sizeof(ImDrawVert));
        index_buffer.Resize(data->TotalIdxCount * sizeof(ImDrawIdx));

        // Set pipeline state

        cmd.SetViewports({{{}, Vec2I(target.Extent())}}, true);
        cmd.BindShaders({vertex_shader, fragment_shader});
        cmd.BindIndexBuffer(index_buffer, sizeof(ImDrawIdx) == sizeof(u16) ? IndexType::U16 : IndexType::U32);

        auto ResetRenderState = [&] {
            cmd.ResetGraphicsState();
            cmd.SetBlendState({true});
        };

        ResetRenderState();

        // Draw vertices

        cmd.BeginRendering({
            .region = {{}, Vec2U(target.Extent())},
            .color_attachments = {target}
        });

        Vec2 clip_offset{ data->DisplayPos.x, data->DisplayPos.y };
        Vec2 clip_scale{ data->FramebufferScale.x, data->FramebufferScale.y };

        usz vertex_offset = 0, index_offset = 0;
        for (i32 i = 0; i < data->CmdListsCount; ++i) {
            auto list = data->CmdLists[i];

            vertex_buffer.Set(Span(list->VtxBuffer.Data, list->VtxBuffer.size()), vertex_offset);
            index_buffer.Set(Span(list->IdxBuffer.Data, list->IdxBuffer.size()), index_offset);

            for (i32 j = 0; j < list->CmdBuffer.size(); ++j) {
                const auto& im_cmd = list->CmdBuffer[j];

                if (im_cmd.UserCallback == ImDrawCallback_ResetRenderState) {
                    ResetRenderState();

                } else if (im_cmd.UserCallback) {
                    im_cmd.UserCallback(list, &im_cmd);

                } else {
                    auto clip_min = glm::max((Vec2(im_cmd.ClipRect.x, im_cmd.ClipRect.y) - clip_offset) * clip_scale, {});
                    auto clip_max = glm::min((Vec2(im_cmd.ClipRect.z, im_cmd.ClipRect.w) - clip_scale), Vec2(target.Extent()));
                    if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) {
                        continue;
                    }

                    cmd.SetScissors({{Vec2I(clip_min), Vec2I(clip_max - clip_min)}});
                    cmd.PushConstants(PushConstants {
                        .vertices = (ImDrawVert*)vertex_buffer.DeviceAddress(),
                        .scale = 2.f / Vec2(target.Extent()),
                        .offset = Vec2(-1.f),
                        .texture = std::bit_cast<TextureDescriptor>(im_cmd.TextureId).handle,
                    });

                    cmd.DrawIndexed(im_cmd.ElemCount, 1,
                        u32(index_offset) + im_cmd.IdxOffset,
                        u32(vertex_offset) + im_cmd.VtxOffset,
                        0);
                }
            }

            vertex_offset += list->VtxBuffer.size();
            index_offset += list->IdxBuffer.size();
        }

        cmd.EndRendering();
    }
}
