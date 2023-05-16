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
            else for (auto& dir : includeDirs)
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
                auto msg = std::format("Failed to find include [{}] requested by [{}]", requestedSource, requestingSource);
                userData->content = msg;
                std::cout << msg << '\n';
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

    Shader* Context::CreateShader(VkShaderStageFlagBits vkStage, VkShaderStageFlags nextStage,
        const std::string& filename, const std::string& sourceCode,
        std::initializer_list<VkPushConstantRange> pushConstantRanges,
        std::initializer_list<VkDescriptorSetLayout> descriptorSetLayouts)
    {
        NOVA_DO_ONCE() { glslang::InitializeProcess(); };
        NOVA_ON_EXIT() { glslang::FinalizeProcess(); };

        EShLanguage stage;
        bool supportsShaderObjects = true;
        switch (vkStage)
        {
        break;case VK_SHADER_STAGE_VERTEX_BIT:                  stage = EShLangVertex;
        break;case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:    stage = EShLangTessControl;
        break;case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: stage = EShLangTessEvaluation;
        break;case VK_SHADER_STAGE_GEOMETRY_BIT:                stage = EShLangGeometry;
        break;case VK_SHADER_STAGE_FRAGMENT_BIT:                stage = EShLangFragment;
        break;case VK_SHADER_STAGE_COMPUTE_BIT:                 stage = EShLangCompute;
        break;case VK_SHADER_STAGE_RAYGEN_BIT_KHR:              stage = EShLangRayGen;
                                                                supportsShaderObjects = false;
        break;case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:             stage = EShLangAnyHit;
                                                                supportsShaderObjects = false;
        break;case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:         stage = EShLangClosestHit;
                                                                supportsShaderObjects = false;
        break;case VK_SHADER_STAGE_MISS_BIT_KHR:                stage = EShLangMiss;
                                                                supportsShaderObjects = false;
        break;case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:        stage = EShLangIntersect;
                                                                supportsShaderObjects = false;
        break;case VK_SHADER_STAGE_CALLABLE_BIT_KHR:            stage = EShLangCallable;
                                                                supportsShaderObjects = false;
        break;case VK_SHADER_STAGE_TASK_BIT_EXT:                stage = EShLangTask;
        break;case VK_SHADER_STAGE_MESH_BIT_EXT:                stage = EShLangMesh;
        break;default: NOVA_THROW("Unknown stage: {}", int(vkStage));
        }

        glslang::TShader shader { stage };
        auto resource = (const TBuiltInResource*)glslang_default_resource();
        shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
        shader.setEnvTarget(glslang::EshTargetSpv, glslang::EShTargetSpv_1_6);

        // ---- Source ----

        const std::string& glslCode = sourceCode.empty() ? ReadFileToString(filename) : sourceCode;
        const char* source = glslCode.c_str();
        int sourceLength = (int)glslCode.size();
        const char* sourceName = filename.c_str();
        shader.setStringsWithLengthsAndNames(&source, &sourceLength, &sourceName, 1);

        // ---- Defines ----

        // ---- Includes ----

        GlslangIncluder includer;

        // ---- Preprocessing ----

        std::string preprocessed;
        if (!shader.preprocess(
            resource,
            100,
            EEsProfile,
            false,
            false,
            EShMessages::EShMsgEnhanced,
            &preprocessed,
            includer))
        {
            NOVA_THROW("GLSL preprocessing failed {}\n{}\n{}", filename, shader.getInfoLog(), shader.getInfoDebugLog());
        }

        const char* preprocessedCStr = preprocessed.c_str();
        shader.setStrings(&preprocessedCStr, 1);

        // ---- Parsing ----

        if (!shader.parse(resource, 100, ENoProfile, EShMessages::EShMsgDefault))
        {
            std::istringstream iss(glslCode);
            std::string line;
            u32 lineNum = 0;
            while (std::getline(iss, line))
            {
                NOVA_LOG("{:3} : {}", lineNum++, line);
            }
            NOVA_THROW("GLSL parsing failed {}\n{}\n{}", filename, shader.getInfoLog(), shader.getInfoDebugLog());
        }

        // ---- Linking ----

        glslang::TProgram program;
        program.addShader(&shader);

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

        const glslang::TIntermediate* intermediate = program.getIntermediate(stage);

        auto newShader = new Shader;
        newShader->context = this;
        newShader->stage = vkStage;

        std::vector<u32> spirv;
        spv::SpvBuildLogger logger;
        glslang::GlslangToSpv(*intermediate, spirv, &spvOptions);

        if (!logger.getAllMessages().empty())
            NOVA_LOG("Shader ({}) SPIR-V messages:\n{}", filename, logger.getAllMessages());

        VkCall(vkCreateShaderModule(device, Temp(VkShaderModuleCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv.size() * 4,
            .pCode = spirv.data(),
        }), pAlloc, &newShader->info.module));
        newShader->info.stage = vkStage;

        if (supportsShaderObjects)
        {
            VkCall(vkCreateShadersEXT(device, 1, Temp(VkShaderCreateInfoEXT {
                .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
                .stage = vkStage,
                .nextStage = nextStage,
                .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
                .codeSize = spirv.size() * 4,
                .pCode = spirv.data(),
                .pName = "main",
                .setLayoutCount = u32(descriptorSetLayouts.size()),
                .pSetLayouts = descriptorSetLayouts.begin(),
                .pushConstantRangeCount = u32(pushConstantRanges.size()),
                .pPushConstantRanges = pushConstantRanges.begin(),
            }), pAlloc, &newShader->shader));
        }

        return newShader;
    }

    void Context::DestroyShader(Shader* shader)
    {
        if (shader->shader)
            vkDestroyShaderEXT(device, shader->shader, pAlloc);

        if (shader->info.module)
            vkDestroyShaderModule(device, shader->info.module, pAlloc);

        delete shader;
    }
}