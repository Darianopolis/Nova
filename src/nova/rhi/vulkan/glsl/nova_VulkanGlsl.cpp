#include "nova_VulkanGlsl.hpp"

#include <nova/core/nova_Files.hpp>

#include <nova/rhi/nova_RHI.hpp>

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
            const char* requestedSource,
            const char* requestingSource,
            bool isRelative)
        {
            std::filesystem::path requested = requestedSource;
            std::filesystem::path current = requestingSource;

            std::filesystem::path target;
            bool exists = false;

            if (isRelative) {
                target = current.parent_path() / requested;
                exists = std::filesystem::exists(target);

            }

            if (!exists) {
                for (auto& dir : includeDirs) {
                    target = dir / requested;
                    if (std::filesystem::exists(target)) {
                        NOVA_LOG("  Found!");
                        exists = true;
                        break;
                    }
                }
            }

            auto userData = new ShaderIncludeUserData{};

            if (exists) {
                userData->name = target.string();
                userData->content = nova::files::ReadTextFile(userData->name);
            } else {
                userData->content = std::format("Failed to find include [{}] requested by [{}]", requestedSource, requestingSource);
                NOVA_LOG("{}", userData->content);
            }

            return new IncludeResult(userData->name, userData->content.data(), userData->content.size(), userData);
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

        void AddIncludeDir(std::filesystem::path path)
        {
            includeDirs.push_back(path);
        }

    private:
        std::vector<std::filesystem::path> includeDirs;
    };

    std::vector<uint32_t> glsl::Compile(
            ShaderStage stage,
            std::string_view entry,
            const std::string& filename,
            Span<std::string_view> fragments)
    {
        NOVA_DO_ONCE() { glslang::InitializeProcess(); };
        NOVA_ON_EXIT() { glslang::FinalizeProcess(); };

        EShLanguage glslangStage;

        switch (stage) {
            break;case nova::ShaderStage::Vertex:       glslangStage = EShLangVertex;
            break;case nova::ShaderStage::TessControl:  glslangStage = EShLangTessControl;
            break;case nova::ShaderStage::TessEval:     glslangStage = EShLangTessEvaluation;
            break;case nova::ShaderStage::Geometry:     glslangStage = EShLangGeometry;
            break;case nova::ShaderStage::Fragment:     glslangStage = EShLangFragment;
            break;case nova::ShaderStage::Compute:      glslangStage = EShLangCompute;
            break;case nova::ShaderStage::RayGen:       glslangStage = EShLangRayGen;
            break;case nova::ShaderStage::AnyHit:       glslangStage = EShLangAnyHit;
            break;case nova::ShaderStage::ClosestHit:   glslangStage = EShLangClosestHit;
            break;case nova::ShaderStage::Miss:         glslangStage = EShLangMiss;
            break;case nova::ShaderStage::Intersection: glslangStage = EShLangIntersect;
            break;case nova::ShaderStage::Callable:     glslangStage = EShLangCallable;
            break;case nova::ShaderStage::Task:         glslangStage = EShLangTask;
            break;case nova::ShaderStage::Mesh:         glslangStage = EShLangMesh;

            break;default: NOVA_THROW("Unknown stage: {}", int(stage));
        }

        glslang::TShader glslShader { glslangStage };
        auto resource = GetDefaultResources();
        glslShader.setEnvInput(glslang::EShSourceGlsl, glslangStage, glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
        glslShader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
        glslShader.setEnvTarget(glslang::EshTargetSpv, glslang::EShTargetSpv_1_6);

        // ---- Source ----

        std::string glsl;
        if (fragments.size()) {
            for (auto& f : fragments) {
                glsl.append(f);
            }
        } else {
            glsl = files::ReadTextFile(filename);
        }

        const char* source = glsl.data();
        i32 sourceLength = i32(glsl.size());
        const char* sourceName = filename.c_str();

        glslShader.setStringsWithLengthsAndNames(&source, &sourceLength, &sourceName, 1);
        glslShader.setPreamble("#extension GL_GOOGLE_include_directive : require\n");
        glslShader.setSourceEntryPoint(entry.data());

        // ---- Defines ----

        // ---- Includes ----

        GlslangIncluder includer;
        includer.AddIncludeDir(".");

        // ---- Preprocessing ----

        auto printShader = [&] {
            std::istringstream iss(glsl);
            std::string line;
            u32 lineNum = 0;
            while (std::getline(iss, line)) {
                NOVA_LOG("{:3} : {}", ++lineNum + 1, line);
            }
        };

        std::string preprocessed;
        if (!glslShader.preprocess(
                resource,
                460,
                ECoreProfile,
                false,
                false,
                EShMessages::EShMsgEnhanced,
                &preprocessed,
                includer)) {
            printShader();
            NOVA_THROW("GLSL preprocessing failed {}\n{}\n{}", filename, glslShader.getInfoLog(), glslShader.getInfoDebugLog());
        }

        const char* preprocessedCStr = preprocessed.c_str();
        glslShader.setStrings(&preprocessedCStr, 1);

        // ---- Parsing ----

        if (!glslShader.parse(resource, 460, false, EShMessages::EShMsgDefault)) {
            printShader();
            NOVA_THROW("GLSL parsing failed {}\n{}\n{}", filename, glslShader.getInfoLog(), glslShader.getInfoDebugLog());
        }

        // ---- Linking ----

        glslang::TProgram program;
        program.addShader(&glslShader);

        if (!program.link(EShMessages(int(EShMessages::EShMsgSpvRules) | int(EShMessages::EShMsgVulkanRules))))
            NOVA_THROW("GLSL linking failed {}\n{}\n{}", filename, program.getInfoLog(), program.getInfoDebugLog());

        // ---- SPIR-V Generation ----

        glslang::SpvOptions spvOptions {
            .generateDebugInfo = false,
            .stripDebugInfo = false,
            .disableOptimizer = true,
            .disassemble = false,
            .validate = true,
            .emitNonSemanticShaderDebugInfo = false,
            .emitNonSemanticShaderDebugSource = false,
        };

        const glslang::TIntermediate* intermediate = program.getIntermediate(glslangStage);

        std::vector<u32> spirv;
        spv::SpvBuildLogger logger;
        glslang::GlslangToSpv(*intermediate, spirv, &logger, &spvOptions);

        if (!logger.getAllMessages().empty()) {
            NOVA_LOG("Shader ({}) SPIR-V messages:\n{}", filename, logger.getAllMessages());
        }

        return spirv;
    }
}