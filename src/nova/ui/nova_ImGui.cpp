#include <nova/ui/nova_ImGui.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>

#include <ImGuizmo.h>

namespace nova
{
    struct ImGuiPushConstants
    {
        u64  vertices;
        Vec2    scale;
        Vec2   offset;
        u32 textureID;
        u32 samplerID;

        static constexpr std::array Layout {
            nova::Member("vertices",   nova::BufferReferenceType("ImDrawVert", true)),
            nova::Member("scale",      nova::ShaderVarType::Vec2),
            nova::Member("offset",     nova::ShaderVarType::Vec2),
            nova::Member("textureID",  nova::ShaderVarType::U32),
            nova::Member("samplerID",  nova::ShaderVarType::U32),
        };
    };

    ImGuiLayer::ImGuiLayer(const ImGuiConfig& config)
        : context(config.context)
        , heap(config.heap)
        , defaultSamplerID(config.sampler)
        , fontTextureID(config.fontTextureID)
    {
        vertexBuffer = nova::Buffer::Create(context, 0,
            nova::BufferUsage::Storage,
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);

        indexBuffer = nova::Buffer::Create(context, 0,
            nova::BufferUsage::Index,
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);

        std::array ImDrawVertLayout {
            nova::Member("pos", nova::ShaderVarType::Vec2),
            nova::Member("uv",  nova::ShaderVarType::Vec2),
            nova::Member("col", nova::ShaderVarType::U32),
        };

        vertexShader = nova::Shader::Create2(context, nova::ShaderStage::Vertex, {
            nova::shader::Structure("ImDrawVert", ImDrawVertLayout),
            nova::shader::PushConstants("pc", ImGuiPushConstants::Layout),
            nova::shader::Output("outUV",    nova::ShaderVarType::Vec2),
            nova::shader::Output("outColor", nova::ShaderVarType::Vec4),
            nova::shader::Fragment(R"glsl(
                fn main() {
                    ref v = pc.vertices[gl_VertexIndex];
                    outUV = v.uv;
                    outColor = unpackUnorm4x8(v.col);
                    gl_Position = vec4((v.pos * pc.scale) + pc.offset, 0, 1);
                }
            )glsl"),
        });

        fragmentShader = nova::Shader::Create2(context, nova::ShaderStage::Fragment, {
            nova::shader::Structure("ImDrawVert", ImDrawVertLayout),
            nova::shader::PushConstants("pc", ImGuiPushConstants::Layout),
            nova::shader::Input("inUV",      nova::ShaderVarType::Vec2),
            nova::shader::Input("inColor",   nova::ShaderVarType::Vec4),
            nova::shader::Output("outColor", nova::ShaderVarType::Vec4),
            nova::shader::Fragment(R"glsl(
                fn main() {
                    outColor = inColor * texture(Sampler2D(pc.textureID, pc.samplerID), inUV);
                }
            )glsl"),
        });

        // Create ImGui context and initialize

        imguiCtx = ImGui::CreateContext();
        lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiCtx);

        auto& io = ImGui::GetIO();
        io.ConfigFlags |= config.flags;
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        ImGui_ImplGlfw_InitForOther(config.window, true);

        // Rescale UI and fonts

        ImGui::GetStyle().ScaleAllSizes(config.uiScale);
        auto fontConfig = ImFontConfig();
        fontConfig.GlyphOffset = ImVec2(config.glyphOffset.x, config.glyphOffset.y);
        ImGui::GetIO().Fonts->ClearFonts();
        ImGui::GetIO().Fonts->AddFontFromFileTTF(config.font, config.fontSize, &fontConfig);

        {
            // Upload texture

            unsigned char* pixels;
            int width, height;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
            usz uploadSize = width * height * 4;
            NOVA_LOGEXPR((void*)pixels);
            NOVA_LOGEXPR(width);
            NOVA_LOGEXPR(height);
            NOVA_LOGEXPR(uploadSize);

            fontTexture = nova::Texture::Create(context,
                { u32(width), u32(height), 0u },
                nova::TextureUsage::Sampled,
                nova::Format::RGBA8_UNorm, {});
            heap.WriteSampledTexture(fontTextureID, fontTexture);
            io.Fonts->SetTexID(GetTextureID(fontTextureID));

            fontTexture.Set({}, fontTexture.GetExtent(), pixels);
            fontTexture.Transition(nova::TextureLayout::Sampled);
        }

        ImGui::SetCurrentContext(lastImguiCtx);
    }

    ImGuiLayer::~ImGuiLayer()
    {
        lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiCtx);
        ImGui_ImplGlfw_Shutdown();
        ImGui::SetCurrentContext(lastImguiCtx);
        ImGui::DestroyContext(imguiCtx);

        vertexBuffer.Destroy();
        indexBuffer.Destroy();
        vertexShader.Destroy();
        fragmentShader.Destroy();
        fontTexture.Destroy();
    }

    void ImGuiLayer::BeginFrame_(DockspaceWindowFn fn, void* payload)
    {
        lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiCtx);

        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuizmo::BeginFrame();

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiDockNodeFlags dockspaceFlags = 0;
            ImGuiWindowFlags dockspaceWindowFlags = 0;

            // Make dockspace fullscreen
            auto viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
            dockspaceWindowFlags |= ImGuiWindowFlags_NoTitleBar
                | ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoBringToFrontOnFocus
                | ImGuiWindowFlags_NoNavFocus;

            if (dockMenuBar) {
                dockspaceWindowFlags |= ImGuiWindowFlags_MenuBar;
            }

            if (noDockBg) {
                // Pass through background
                dockspaceWindowFlags |= ImGuiWindowFlags_NoBackground;
                dockspaceFlags |= ImGuiDockNodeFlags_PassthruCentralNode;
            }

            // Register dockspace
            ImGui::Begin("Dockspace", Temp(true), dockspaceWindowFlags);
            ImGui::PopStyleVar(3);
            ImGui::DockSpace(ImGui::GetID("DockspaceID"), ImVec2(0.f, 0.f), dockspaceFlags);

            fn(payload, *this);

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

            ImGui::SetCurrentContext(lastImguiCtx);
        }
    }

    bool ImGuiLayer::HasDrawData()
    {
        return ended;
    }

    void ImGuiLayer::DrawFrame(CommandList cmd, Texture target, Fence fence)
    {
        EndFrame();

        lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiCtx);
        NOVA_CLEANUP(&) {  ImGui::SetCurrentContext(lastImguiCtx); };

        auto data = ImGui::GetDrawData();
        if (!data->TotalIdxCount) {
            return;
        }

        // Ensure buffer sizes

        if (vertexBuffer.GetSize() < data->TotalVtxCount * sizeof(ImDrawVert)
                || indexBuffer.GetSize() < data->TotalIdxCount * sizeof(ImDrawIdx)) {

            // Flush frames to resize safely (this should only happen a few times in total)
            fence.Wait();

            vertexBuffer.Resize(data->TotalVtxCount * sizeof(ImDrawVert));
            indexBuffer.Resize(data->TotalIdxCount * sizeof(ImDrawIdx));
        }

        // Set pipeline state

        cmd.BeginRendering({{}, Vec2U(target.GetExtent())}, {target});
        cmd.ResetGraphicsState();
        cmd.SetViewports({{{}, Vec2I(target.GetExtent())}});
        cmd.SetBlendState({true});
        cmd.BindShaders({vertexShader, fragmentShader});
        cmd.BindIndexBuffer(indexBuffer, sizeof(ImDrawIdx) == 2 ? nova::IndexType::U16 : nova::IndexType::U32);
        cmd.BindDescriptorHeap(nova::BindPoint::Graphics, heap);

        // Draw vertices

        Vec2 clipOffset{ data->DisplayPos.x, data->DisplayPos.y };
        Vec2 clipScale{ data->FramebufferScale.x, data->FramebufferScale.y };

        usz vertexOffset = 0, indexOffset = 0;
        for (i32 i = 0; i < data->CmdListsCount; ++i) {
            auto list = data->CmdLists[i];

            vertexBuffer.Set(Span(list->VtxBuffer.Data, list->VtxBuffer.size()), vertexOffset);
            indexBuffer.Set(Span(list->IdxBuffer.Data, list->IdxBuffer.size()), indexOffset);

            for (i32 j = 0; j < list->CmdBuffer.size(); ++j) {
                const auto& imCmd = list->CmdBuffer[j];

                auto clipMin = glm::max((Vec2(imCmd.ClipRect.x, imCmd.ClipRect.y) - clipOffset) * clipScale, {});
                auto clipMax = glm::min((Vec2(imCmd.ClipRect.z, imCmd.ClipRect.w) - clipScale), Vec2(target.GetExtent()));
                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y) {
                    continue;
                }

                cmd.SetScissors({{Vec2I(clipMin), Vec2I(clipMax - clipMin)}});
                cmd.PushConstants(0, sizeof(ImGuiPushConstants), Temp(ImGuiPushConstants {
                    .vertices = vertexBuffer.GetAddress(),
                    .scale = 2.f / Vec2(target.GetExtent()),
                    .offset = Vec2(-1.f),
                    .textureID = u32(uintptr_t(imCmd.TextureId) & ~0u),
                    .samplerID = u32(uintptr_t(imCmd.TextureId) >> 32),
                }));

                cmd.DrawIndexed(imCmd.ElemCount, 1,
                    u32(indexOffset) + imCmd.IdxOffset,
                    u32(vertexOffset) + imCmd.VtxOffset,
                    0);
            }

            vertexOffset += list->VtxBuffer.size();
            indexOffset += list->IdxBuffer.size();
        }

        cmd.EndRendering();
    }
}
