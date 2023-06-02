#include "nova_RHI.hpp"

#include <nova/core/nova_Files.hpp>

#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/resource_limits_c.h>
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

            if (isRelative)
            {
                target = current.parent_path() / requested;
                exists = std::filesystem::exists(target);
            }
            else
            for (auto& dir : includeDirs)
            {
                target = dir / requested;
                if (std::filesystem::exists(target))
                {
                    exists = true;
                    break;
                }
            }

            auto userData = new ShaderIncludeUserData{};

            if (exists)
            {
                userData->name = target.string();
                userData->content = nova::ReadFileToString(userData->name);
            }
            else
            {
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

    Shader::Shader(Context& _context, ShaderStage _stage, ShaderStage _nextStage,
            const std::string& filename, const std::string& sourceCode,
            PipelineLayout& layout)
        : context(&_context)
        , stage(VkShaderStageFlagBits(_stage))
    {
        NOVA_DO_ONCE() { glslang::InitializeProcess(); };
        NOVA_ON_EXIT() { glslang::FinalizeProcess(); };

        auto nextStage = VkShaderStageFlags(_nextStage);

        EShLanguage glslangStage;
        bool supportsShaderObjects = true;
        switch (stage)
        {
        break;case VK_SHADER_STAGE_VERTEX_BIT:                  glslangStage = EShLangVertex;
        break;case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:    glslangStage = EShLangTessControl;
        break;case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: glslangStage = EShLangTessEvaluation;
        break;case VK_SHADER_STAGE_GEOMETRY_BIT:                glslangStage = EShLangGeometry;
        break;case VK_SHADER_STAGE_FRAGMENT_BIT:                glslangStage = EShLangFragment;
        break;case VK_SHADER_STAGE_COMPUTE_BIT:                 glslangStage = EShLangCompute;
        break;case VK_SHADER_STAGE_RAYGEN_BIT_KHR:              glslangStage = EShLangRayGen;
                                                                supportsShaderObjects = false;
        break;case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:             glslangStage = EShLangAnyHit;
                                                                supportsShaderObjects = false;
        break;case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:         glslangStage = EShLangClosestHit;
                                                                supportsShaderObjects = false;
        break;case VK_SHADER_STAGE_MISS_BIT_KHR:                glslangStage = EShLangMiss;
                                                                supportsShaderObjects = false;
        break;case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:        glslangStage = EShLangIntersect;
                                                                supportsShaderObjects = false;
        break;case VK_SHADER_STAGE_CALLABLE_BIT_KHR:            glslangStage = EShLangCallable;
                                                                supportsShaderObjects = false;
        break;case VK_SHADER_STAGE_TASK_BIT_EXT:                glslangStage = EShLangTask;
        break;case VK_SHADER_STAGE_MESH_BIT_EXT:                glslangStage = EShLangMesh;
        break;default: NOVA_THROW("Unknown stage: {}", int(stage));
        }

        glslang::TShader glslShader { glslangStage };
        auto resource = (const TBuiltInResource*)glslang_default_resource();
        glslShader.setEnvInput(glslang::EShSourceGlsl, glslangStage, glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
        glslShader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
        glslShader.setEnvTarget(glslang::EshTargetSpv, glslang::EShTargetSpv_1_6);

        // ---- Source ----

        const std::string& glslCode = sourceCode.empty() ? ReadFileToString(filename) : sourceCode;
        const char* source = glslCode.c_str();
        int sourceLength = (int)glslCode.size();
        const char* sourceName = filename.c_str();
        glslShader.setStringsWithLengthsAndNames(&source, &sourceLength, &sourceName, 1);

        // ---- Defines ----

        // ---- Includes ----

        GlslangIncluder includer;

        // ---- Preprocessing ----

        std::string preprocessed;
        if (!glslShader.preprocess(
            resource,
            100,
            EEsProfile,
            false,
            false,
            EShMessages::EShMsgEnhanced,
            &preprocessed,
            includer))
        {
            NOVA_THROW("GLSL preprocessing failed {}\n{}\n{}", filename, glslShader.getInfoLog(), glslShader.getInfoDebugLog());
        }

        const char* preprocessedCStr = preprocessed.c_str();
        glslShader.setStrings(&preprocessedCStr, 1);

        // ---- Parsing ----

        if (!glslShader.parse(resource, 100, ENoProfile, EShMessages::EShMsgDefault))
        {
            std::istringstream iss(glslCode);
            std::string line;
            u32 lineNum = 0;
            while (std::getline(iss, line))
            {
                NOVA_LOG("{:3} : {}", lineNum++, line);
            }
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
            .disableOptimizer = false,
            .disassemble = false,
            .validate = true,
            .emitNonSemanticShaderDebugInfo = false,
            .emitNonSemanticShaderDebugSource = false,
        };

        const glslang::TIntermediate* intermediate = program.getIntermediate(glslangStage);

        std::vector<u32> spirv;
        spv::SpvBuildLogger logger;
        glslang::GlslangToSpv(*intermediate, spirv, &spvOptions);

        if (!logger.getAllMessages().empty())
            NOVA_LOG("Shader ({}) SPIR-V messages:\n{}", filename, logger.getAllMessages());

        VkCall(vkCreateShaderModule(context->device, Temp(VkShaderModuleCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv.size() * 4,
            .pCode = spirv.data(),
        }), context->pAlloc, &module));

        if (supportsShaderObjects)
        {
            // TODO: Only use ranges and descriptor sets that are included in this shader stage?
            //   Vulkan should already handle this?
            VkCall(vkCreateShadersEXT(context->device, 1, Temp(VkShaderCreateInfoEXT {
                .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
                .stage = stage,
                .nextStage = nextStage,
                .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
                .codeSize = spirv.size() * 4,
                .pCode = spirv.data(),
                .pName = "main",
                .setLayoutCount = u32(layout.sets.size()),
                .pSetLayouts = layout.sets.data(),
                .pushConstantRangeCount = u32(layout.ranges.size()),
                .pPushConstantRanges = layout.ranges.data(),
            }), context->pAlloc, &shader));
        }
    }

    Shader::~Shader()
    {
        if (shader)
            vkDestroyShaderEXT(context->device, shader, context->pAlloc);

        if (module)
            vkDestroyShaderModule(context->device, module, context->pAlloc);
    }

    Shader::Shader(Shader&& other) noexcept
        : context(other.context)
        , stage(other.stage)
        , shader(other.shader)
        , module(other.module)
    {
        other.shader = nullptr;
        other.module = nullptr;
    }

    bool Shader::IsValid() const
    {
        return shader || module;
    }

    VkPipelineShaderStageCreateInfo Shader::GetStageInfo() const
    {
        return {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = stage,
            .module = module,
            .pName = name,
        };
    }
}