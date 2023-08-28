#include <nova/ui/nova_ImGui.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>

#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

#include <ImGuizmo.h>

namespace nova
{
    struct ImGuiPushConstants
    {
        u64 vertexVA;
        Vec2 dimensions;
        u32 textureID;
        u32 samplerID;

        static constexpr std::array Layout {
            nova::Member("vertexVA",   nova::ShaderVarType::U64),
            nova::Member("dimensions", nova::ShaderVarType::Vec2),
            nova::Member("textureID",  nova::ShaderVarType::U32),
            nova::Member("samplerID",  nova::ShaderVarType::U32),
        };
    };

    ImGuiLayer::ImGuiLayer(
            const ImGuiConfig& config,
            CommandPool cmdPool,
            CommandState cmdState,
            Queue queue,
            Fence fence)
        : context(config.context)
        , heap(config.heap)
        , defaultSamplerID(config.sampler)
        , fontTextureID(config.fontTextureID)
    {
        // Create RHI backend

        vertexBuffer = nova::Buffer::Create(context, 100'000 * sizeof(ImDrawVert),
            nova::BufferUsage::Storage,
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
        indexBuffer = nova::Buffer::Create(context, 100'000 * sizeof(ImDrawIdx),
            nova::BufferUsage::Index,
            nova::BufferFlags::DeviceLocal | nova::BufferFlags::Mapped);
        vertexShader = nova::Shader::Create(context, nova::ShaderStage::Vertex, {
            nova::shader::Structure("ImDrawVert", {
                nova::Member("pos", nova::ShaderVarType::Vec2),
                nova::Member("uv",  nova::ShaderVarType::Vec2),
                nova::Member("col", nova::ShaderVarType::U32),
            }),
            nova::shader::PushConstants("pc", ImGuiPushConstants::Layout),
            nova::shader::Output("outUV",    nova::ShaderVarType::Vec2),
            nova::shader::Output("outColor", nova::ShaderVarType::Vec4),
            nova::shader::Kernel(R"glsl(
                ImDrawVert v = nova::BufferReference<ImDrawVert>(pc.vertexVA).data[gl_VertexIndex];
                outUV = v.uv;
                outColor = unpackUnorm4x8(v.col);
                gl_Position = vec4((v.pos / pc.dimensions) * 2 - 1, 0, 1);
            )glsl"),
        });

        fragmentShader = nova::Shader::Create(context, nova::ShaderStage::Fragment, {
            nova::shader::PushConstants("pc", ImGuiPushConstants::Layout),
            nova::shader::Input("inUV",      nova::ShaderVarType::Vec2),
            nova::shader::Input("inColor",   nova::ShaderVarType::Vec4),
            nova::shader::Output("outColor", nova::ShaderVarType::Vec4),
            nova::shader::Kernel(R"glsl(
                outColor = inColor * texture(nova::Sampler2D(pc.textureID, pc.samplerID), inUV);
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

            // TODO: Host image copy and avoid staging

            auto staging = nova::Buffer::Create(context, uploadSize,
                nova::BufferUsage::TransferSrc,
                nova::BufferFlags::Mapped);
            NOVA_CLEANUP(&) { staging.Destroy(); };
            staging.Set(Span(pixels, uploadSize));

            auto cmd = cmdPool.Begin(cmdState);
            cmd.CopyToTexture(fontTexture, staging);
            cmd.Transition(fontTexture, nova::TextureLayout::Sampled, nova::PipelineStage::Graphics);
            queue.Submit({cmd}, {}, {fence});
            fence.Wait();
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

        // heap.Destroy();
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

    void ImGuiLayer::DrawFrame(CommandList cmd, Texture texture)
    {
        EndFrame();

        lastImguiCtx = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiCtx);
        NOVA_CLEANUP(&) {  ImGui::SetCurrentContext(lastImguiCtx); };

        auto data = ImGui::GetDrawData();
        if (!data->TotalIdxCount) {
            return;
        }

        ImVec2 clipOff = data->DisplayPos; // (0,0) unless using multi-viewports
        ImVec2 clipScale = data->FramebufferScale;

        cmd.BeginRendering({{}, Vec2U(texture.GetExtent())}, {texture});

        constexpr auto ImGuiIndexType = sizeof(ImDrawIdx) == 2
            ? nova::IndexType::U16
            : nova::IndexType::U32;

        // Resize buffers
        vertexBuffer.Resize(data->TotalVtxCount * sizeof(ImDrawVert));
        indexBuffer.Resize(data->TotalIdxCount * sizeof(ImDrawIdx));

        cmd.SetGraphicsState({ vertexShader, fragmentShader }, {
            .cullMode = nova::CullMode::None,
            .blendEnable = true,
        });
        cmd.BindIndexBuffer(indexBuffer, ImGuiIndexType);
        cmd.BindDescriptorHeap(nova::BindPoint::Graphics, heap);

        // Copy buffer data
        usz vertexOffset = 0, indexOffset = 0;
        for (i32 i = 0; i < data->CmdListsCount; ++i) {
            auto list = data->CmdLists[i];

            vertexBuffer.Set(Span(list->VtxBuffer.Data, list->VtxBuffer.size()), vertexOffset);
            vertexOffset += list->VtxBuffer.size();

            indexBuffer.Set(Span(list->IdxBuffer.Data, list->IdxBuffer.size()), indexOffset);
            indexOffset += list->IdxBuffer.size();
        }

        // Draw vertices
        vertexOffset = indexOffset = 0;
        for (i32 i = 0; i < data->CmdListsCount; ++i) {
            auto list = data->CmdLists[i];

            for (i32 j = 0; j < list->CmdBuffer.size(); ++j) {
                const auto& imCmd = list->CmdBuffer[j];

                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clipMin{ (imCmd.ClipRect.x - clipOff.x) * clipScale.x, (imCmd.ClipRect.y - clipOff.y) * clipScale.y };
                ImVec2 clipMax{ (imCmd.ClipRect.z - clipOff.x) * clipScale.x, (imCmd.ClipRect.w - clipOff.y) * clipScale.y };

                // Clamp to viewport
                if (clipMin.x < 0.0f) { clipMin.x = 0.0f; }
                if (clipMin.y < 0.0f) { clipMin.y = 0.0f; }
                if (clipMax.x > texture.GetExtent().x) { clipMax.x = f32(texture.GetExtent().x); }
                if (clipMax.y > texture.GetExtent().y) { clipMax.y = f32(texture.GetExtent().y); }
                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                    continue;

                // TODO: Add RHI command
                vkCmdSetScissorWithCount(cmd->buffer, 1,
                    Temp(VkRect2D{
                        {i32(clipMin.x), i32(clipMin.y)},
                        {u32(clipMax.x - clipMin.x), u32(clipMax.y - clipMin.y)},
                    }));

                u32 samplerID = u32(uintptr_t(imCmd.TextureId) >> 32);
                u32 textureID = u32(uintptr_t(imCmd.TextureId) & UINT32_MAX);

                cmd.PushConstants(0, sizeof(ImGuiPushConstants), Temp(ImGuiPushConstants {
                    .vertexVA = vertexBuffer.GetAddress(),
                    .dimensions = Vec2(texture.GetExtent()),
                    .textureID = textureID,
                    .samplerID = samplerID,
                }));

                cmd.DrawIndexed(
                    imCmd.ElemCount,
                    1,
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
