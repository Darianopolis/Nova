#include <nova/gpu/vulkan/VulkanRHI.hpp>

#include <nova/core/Files.hpp>

#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>

namespace nova
{
    class GlslangIncluder : public glslang::TShader::Includer
    {
        struct ShaderIncludeUserData
        {
            std::string name;
            std::string content;
        };
    public:
        IncludeResult* include(
            const char* requested_source,
            const char* requesting_source,
            bool is_relative)
        {
            fs::path requested = requested_source;
            fs::path current = requesting_source;

            fs::path target;
            bool exists = false;

            if (is_relative) {
                target = current.parent_path() / requested;
                exists = fs::exists(target);

            }

            if (!exists) {
                for (auto& dir : include_dirs) {
                    target = dir / requested;
                    if (fs::exists(target)) {
                        exists = true;
                        break;
                    }
                }
            }

            auto userdata = new ShaderIncludeUserData{};

            if (exists) {
                userdata->name = target.string();
                if (!included.contains(target)) {
                    included.insert(target);
                    userdata->content = nova::files::ReadTextFile(userdata->name);
                }
            } else {
                userdata->content = Fmt("Failed to find include [{}] requested by [{}]", requested_source, requesting_source);
                Log("{}", userdata->content);
            }

            return new IncludeResult(userdata->name, userdata->content.data(), userdata->content.size(), userdata);
        }

        virtual IncludeResult* includeSystem(const char* headerName, const char* includerName, size_t /*inclusionDepth*/)
        {
            return include(headerName, includerName, false);
        }

        virtual IncludeResult* includeLocal(const char* headerName, const char* includerName, size_t /*inclusionDepth*/)
        {
            return include(headerName, includerName, true);
        }

        virtual void releaseInclude(IncludeResult* data)
        {
            delete (ShaderIncludeUserData*)data->userData;
            delete data;
        }

        void AddIncludeDir(fs::path path)
        {
            include_dirs.push_back(path);
        }

    private:
        std::vector<fs::path> include_dirs;

        ankerl::unordered_dense::set<fs::path> included;
    };

    std::vector<uint32_t> Vulkan_CompileGlslToSpirv(
            ShaderStage          stage,
            StringView           entry,
            StringView        filename,
            Span<StringView> fragments)
    {

        NOVA_DO_ONCE() { glslang::InitializeProcess(); };
        NOVA_ON_EXIT() { glslang::FinalizeProcess(); };

        EShLanguage glslang_stage;

        switch (stage) {
            break;case nova::ShaderStage::Vertex:       glslang_stage = EShLangVertex;
            break;case nova::ShaderStage::TessControl:  glslang_stage = EShLangTessControl;
            break;case nova::ShaderStage::TessEval:     glslang_stage = EShLangTessEvaluation;
            break;case nova::ShaderStage::Geometry:     glslang_stage = EShLangGeometry;
            break;case nova::ShaderStage::Fragment:     glslang_stage = EShLangFragment;
            break;case nova::ShaderStage::Compute:      glslang_stage = EShLangCompute;
            break;case nova::ShaderStage::RayGen:       glslang_stage = EShLangRayGen;
            break;case nova::ShaderStage::AnyHit:       glslang_stage = EShLangAnyHit;
            break;case nova::ShaderStage::ClosestHit:   glslang_stage = EShLangClosestHit;
            break;case nova::ShaderStage::Miss:         glslang_stage = EShLangMiss;
            break;case nova::ShaderStage::Intersection: glslang_stage = EShLangIntersect;
            break;case nova::ShaderStage::Callable:     glslang_stage = EShLangCallable;
            break;case nova::ShaderStage::Task:         glslang_stage = EShLangTask;
            break;case nova::ShaderStage::Mesh:         glslang_stage = EShLangMesh;

            break;default: NOVA_THROW("Unknown stage: {}", int(stage));
        }

        glslang::TShader glslang_shader { glslang_stage };
        auto resource = GetDefaultResources();
        glslang_shader.setEnvInput(glslang::EShSourceGlsl, glslang_stage, glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
        glslang_shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
        glslang_shader.setEnvTarget(glslang::EshTargetSpv, glslang::EShTargetSpv_1_6);

        // ---- Source ----

        std::string glsl;
        if (fragments.size()) {
            for (auto& f : fragments) {
                glsl.append_range(f);
            }
        } else {
            glsl = files::ReadTextFile(filename);
        }

        const char* source = glsl.data();
        i32 source_length = i32(glsl.size());
        auto source_name = filename.CStr();
        glslang_shader.setStringsWithLengthsAndNames(&source, &source_length, &source_name.Get(), 1);

        glslang_shader.setPreamble("#extension GL_GOOGLE_include_directive : require\n");

        auto entry_cstr = entry.CStr();
        glslang_shader.setSourceEntryPoint(entry_cstr);

        // ---- Defines ----

        // ---- Includes ----

        GlslangIncluder includer;
        includer.AddIncludeDir(".");

        // ---- Preprocessing ----

        auto PrintShader = [&] {
            std::istringstream iss(glsl);
            std::string line;
            u32 line_num = 0;
            while (std::getline(iss, line)) {
                Log("{:3} : {}", ++line_num + 1, line);
            }
        };

        std::string preprocessed;
        if (!glslang_shader.preprocess(
                resource,
                460,
                ECoreProfile,
                false,
                false,
                EShMessages::EShMsgEnhanced,
                &preprocessed,
                includer)) {
            PrintShader();
            NOVA_THROW("GLSL preprocessing failed {}\n{}\n{}", filename, glslang_shader.getInfoLog(), glslang_shader.getInfoDebugLog());
        }

        const char* preprocessed_cstr = preprocessed.c_str();
        glslang_shader.setStrings(&preprocessed_cstr, 1);

        // ---- Parsing ----

        if (!glslang_shader.parse(resource, 460, false, EShMessages::EShMsgDefault)) {
            PrintShader();
            NOVA_THROW("GLSL parsing failed {}\n{}\n{}", filename, glslang_shader.getInfoLog(), glslang_shader.getInfoDebugLog());
        }

        // ---- Linking ----

        glslang::TProgram program;
        program.addShader(&glslang_shader);

        if (!program.link(EShMessages(int(EShMessages::EShMsgSpvRules) | int(EShMessages::EShMsgVulkanRules))))
            NOVA_THROW("GLSL linking failed {}\n{}\n{}", filename, program.getInfoLog(), program.getInfoDebugLog());

        // ---- SPIR-V Generation ----

        glslang::SpvOptions spv_options {
            .generateDebugInfo = false,
            .stripDebugInfo = false,
            .disableOptimizer = true,
            .disassemble = false,
            .validate = true,
            .emitNonSemanticShaderDebugInfo = false,
            .emitNonSemanticShaderDebugSource = false,
        };

        const glslang::TIntermediate* intermediate = program.getIntermediate(glslang_stage);

        std::vector<u32> spirv;
        spv::SpvBuildLogger logger;
        glslang::GlslangToSpv(*intermediate, spirv, &logger, &spv_options);

        if (!logger.getAllMessages().empty()) {
            Log("Shader ({}) SPIR-V messages:\n{}", filename, logger.getAllMessages());
        }

        return spirv;
    }

    static
    std::monostate glslang_loaded = Vulkan_RegisterCompiler(ShaderLang::Glsl, Vulkan_CompileGlslToSpirv);
}
