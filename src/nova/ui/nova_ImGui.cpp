#include <nova/ui/nova_ImGui.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>

#include <nova/rhi/vulkan/glsl/nova_VulkanGlsl.hpp>

#include <ImGuizmo.h>

namespace nova
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

    ImGuiLayer::ImGuiLayer(const ImGuiConfig& config)
        : context(config.context)
        , default_sampler(config.sampler)
    {
        vertex_buffer = nova::Buffer::Create(context, 0,
            nova::BufferUsage::Storage,
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);

        index_buffer = nova::Buffer::Create(context, 0,
            nova::BufferUsage::Index,
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);

        vertex_shader = nova::Shader::Create(context, nova::ShaderStage::Vertex, "main",
            nova::glsl::Compile(nova::ShaderStage::Vertex, "main", "", {
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
            }));

        fragment_shader = nova::Shader::Create(context, nova::ShaderStage::Fragment, "main",
            nova::glsl::Compile(nova::ShaderStage::Fragment, "main", "", {
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
            }));

        // Create ImGui context and initialize

        imgui_ctx = ImGui::CreateContext();
        last_imgui_ctx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imgui_ctx);

        auto& io = ImGui::GetIO();
        io.ConfigFlags |= config.flags;
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        ImGui_ImplGlfw_InitForOther(config.window, true);

        // Rescale UI and fonts

        ImGui::GetStyle().ScaleAllSizes(config.ui_scale);
        auto font_config = ImFontConfig();
        font_config.GlyphOffset = ImVec2(config.glyph_offset.x, config.glyph_offset.y);
        ImGui::GetIO().Fonts->ClearFonts();
        ImGui::GetIO().Fonts->AddFontFromFileTTF(config.font, config.font_size, &font_config);

        {
            // Upload texture

            unsigned char* pixels;
            int width, height;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

            font_texture = nova::Texture::Create(context,
                { u32(width), u32(height), 0u },
                nova::TextureUsage::Sampled,
                nova::Format::RGBA8_UNorm, {});
            io.Fonts->SetTexID(GetTextureID(font_texture));

            font_texture.Set({}, font_texture.GetExtent(), pixels);
            font_texture.Transition(nova::TextureLayout::Sampled);
        }

        ImGui::SetCurrentContext(last_imgui_ctx);
    }

    ImGuiLayer::~ImGuiLayer()
    {
        last_imgui_ctx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imgui_ctx);
        ImGui_ImplGlfw_Shutdown();
        ImGui::SetCurrentContext(last_imgui_ctx);
        ImGui::DestroyContext(imgui_ctx);

        vertex_buffer.Destroy();
        index_buffer.Destroy();
        vertex_shader.Destroy();
        fragment_shader.Destroy();
        font_texture.Destroy();
    }

    void ImGuiLayer::BeginFrame(LambdaRef<void()> fn)
    {
        last_imgui_ctx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imgui_ctx);

        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuizmo::BeginFrame();

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
                auto contextBackup = glfwGetCurrentContext();
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                glfwMakeContextCurrent(contextBackup);
            }

            ImGui::SetCurrentContext(last_imgui_ctx);
        }
    }

    bool ImGuiLayer::HasDrawData()
    {
        return ended;
    }

    void ImGuiLayer::DrawFrame(CommandList cmd, Texture target, Fence fence)
    {
        EndFrame();

        last_imgui_ctx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imgui_ctx);
        NOVA_CLEANUP(&) {  ImGui::SetCurrentContext(last_imgui_ctx); };

        auto data = ImGui::GetDrawData();
        if (!data->TotalIdxCount) {
            return;
        }

        // Ensure buffer sizes

        if (vertex_buffer.GetSize() < data->TotalVtxCount * sizeof(ImDrawVert)
                || index_buffer.GetSize() < data->TotalIdxCount * sizeof(ImDrawIdx)) {

            // Flush frames to resize safely (this should only happen a few times in total)
            fence.Wait();

            vertex_buffer.Resize(data->TotalVtxCount * sizeof(ImDrawVert));
            index_buffer.Resize(data->TotalIdxCount * sizeof(ImDrawIdx));
        }

        // Set pipeline state

        cmd.ResetGraphicsState();
        cmd.SetViewports({{{}, Vec2I(target.GetExtent())}});
        cmd.SetBlendState({true});
        cmd.BindShaders({vertex_shader, fragment_shader});
        cmd.BindIndexBuffer(index_buffer, sizeof(ImDrawIdx) == 2 ? nova::IndexType::U16 : nova::IndexType::U32);

        // Draw vertices

        cmd.BeginRendering({{}, Vec2U(target.GetExtent())}, {target});

        Vec2 clip_offset{ data->DisplayPos.x, data->DisplayPos.y };
        Vec2 clip_scale{ data->FramebufferScale.x, data->FramebufferScale.y };

        usz vertex_offset = 0, index_offset = 0;
        for (i32 i = 0; i < data->CmdListsCount; ++i) {
            auto list = data->CmdLists[i];

            vertex_buffer.Set(Span(list->VtxBuffer.Data, list->VtxBuffer.size()), vertex_offset);
            index_buffer.Set(Span(list->IdxBuffer.Data, list->IdxBuffer.size()), index_offset);

            for (i32 j = 0; j < list->CmdBuffer.size(); ++j) {
                const auto& im_cmd = list->CmdBuffer[j];

                auto clip_min = glm::max((Vec2(im_cmd.ClipRect.x, im_cmd.ClipRect.y) - clip_offset) * clip_scale, {});
                auto clip_max = glm::min((Vec2(im_cmd.ClipRect.z, im_cmd.ClipRect.w) - clip_scale), Vec2(target.GetExtent()));
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) {
                    continue;
                }

                cmd.SetScissors({{Vec2I(clip_min), Vec2I(clip_max - clip_min)}});
                cmd.PushConstants(ImGuiPushConstants {
                    .vertices = vertex_buffer.GetAddress(),
                    .scale = 2.f / Vec2(target.GetExtent()),
                    .offset = Vec2(-1.f),
                    .texture = std::bit_cast<Vec2U>(im_cmd.TextureId),
                });

                cmd.DrawIndexed(im_cmd.ElemCount, 1,
                    u32(index_offset) + im_cmd.IdxOffset,
                    u32(vertex_offset) + im_cmd.VtxOffset,
                    0);
            }

            vertex_offset += list->VtxBuffer.size();
            index_offset += list->IdxBuffer.size();
        }

        cmd.EndRendering();
    }
}
