#include <nova/ui/nova_ImGui.hpp>

#include <nova/core/nova_Guards.hpp>

#include <imgui.h>

namespace nova::imgui
{
    namespace
    {
        struct ImGuiPushConstants
        {
            u64  vertices;
            Vec2    scale;
            Vec2   offset;
            Vec2U texture;
        };

        static
        constexpr auto Preamble = R"glsl(
#extension GL_EXT_scalar_block_layout  : require
#extension GL_EXT_buffer_reference2    : require
#extension GL_EXT_nonuniform_qualifier : require

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer ImDrawVert {
    vec2 pos;
    vec2  uv;
    uint col;
};

layout(push_constant, scalar) readonly uniform pc_ {
    ImDrawVert vertices;
    vec2          scale;
    vec2         offset;
    uvec2       texture;
} pc;
        )glsl"sv;
    }

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
        if (frames_in_flight == 0) {
            NOVA_THROW("frames_in_flight must be greater than 0!");
        }

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

        vertex_shader = Shader::Create(context,
            ShaderLang::Glsl, ShaderStage::Vertex, "main", "", {
                Preamble,
                R"glsl(
layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;
void main() {
    ImDrawVert v = pc.vertices[gl_VertexIndex];
    out_uv = v.uv;
    out_color = unpackUnorm4x8(v.col);
    gl_Position = vec4((v.pos * pc.scale) + pc.offset, 0, 1);
}
                )glsl"
            });

        fragment_shader = Shader::Create(context,
            ShaderLang::Glsl, ShaderStage::Fragment, "main", "", {
                Preamble,
                R"glsl(
layout(set = 0, binding = 0) uniform texture2D Image2D[];
layout(set = 0, binding = 2) uniform sampler Sampler[];
layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_color;
layout(location = 0) out vec4 out_color;
void main() {
    out_color = texture(sampler2D(Image2D[pc.texture.x], Sampler[pc.texture.y]), in_uv)
        * in_color;
}
                )glsl"
            });

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
        ImGui::GetIO().Fonts->AddFontFromFileTTF(config.font, config.font_size, &font_config);

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
            dockspace_window_flags |= ImGuiWindowFlags_NoTitleBar
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
            ImGui::Begin("Dockspace", Temp(true), dockspace_window_flags);
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
                    cmd.PushConstants(ImGuiPushConstants {
                        .vertices = vertex_buffer.DeviceAddress(),
                        .scale = 2.f / Vec2(target.Extent()),
                        .offset = Vec2(-1.f),
                        .texture = std::bit_cast<Vec2U>(im_cmd.TextureId),
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
