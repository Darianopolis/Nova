#include "nova_RHI_Impl.hpp"

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

    NOVA_DEFINE_HANDLE_OPERATIONS(Shader)

    Shader::Shader(Context context, ShaderStage stage, const std::string& filename, const std::string& sourceCode)
        : ImplHandle(new ShaderImpl)
    {
        impl->context = context;
        impl->stage = VkShaderStageFlagBits(stage);
        impl->id = context->GetUID();

        NOVA_DO_ONCE() { glslang::InitializeProcess(); };
        NOVA_ON_EXIT() { glslang::FinalizeProcess(); };

        EShLanguage glslangStage;
        bool createShaderObject = context->config.shaderObjects;
        switch (impl->stage)
        {
        break;case VK_SHADER_STAGE_VERTEX_BIT:
            glslangStage = EShLangVertex;

        break;case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
            glslangStage = EShLangTessControl;

        break;case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
            glslangStage = EShLangTessEvaluation;

        break;case VK_SHADER_STAGE_GEOMETRY_BIT:
            glslangStage = EShLangGeometry;

        break;case VK_SHADER_STAGE_FRAGMENT_BIT:
            glslangStage = EShLangFragment;

        break;case VK_SHADER_STAGE_COMPUTE_BIT:
            glslangStage = EShLangCompute;

        break;case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
            glslangStage = EShLangRayGen;
            createShaderObject = false;

        break;case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
            glslangStage = EShLangAnyHit;
            createShaderObject = false;

        break;case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
            glslangStage = EShLangClosestHit;
            createShaderObject = false;

        break;case VK_SHADER_STAGE_MISS_BIT_KHR:
            glslangStage = EShLangMiss;
            createShaderObject = false;

        break;case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
            glslangStage = EShLangIntersect;
            createShaderObject = false;

        break;case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
            glslangStage = EShLangCallable;
            createShaderObject = false;

        break;case VK_SHADER_STAGE_TASK_BIT_EXT:
            glslangStage = EShLangTask;

        break;case VK_SHADER_STAGE_MESH_BIT_EXT:
            glslangStage = EShLangMesh;

        break;default: NOVA_THROW("Unknown stage: {}", int(impl->stage));
        }

        glslang::TShader glslShader { glslangStage };
        auto resource = (const TBuiltInResource*)glslang_default_resource();
        glslShader.setEnvInput(glslang::EShSourceGlsl, glslangStage, glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
        glslShader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
        glslShader.setEnvTarget(glslang::EshTargetSpv, glslang::EShTargetSpv_1_6);

        // ---- Source ----

        const std::string& glslCode = sourceCode.empty() ? ReadFileToString(filename) : sourceCode;
        const char* source = glslCode.data();
        i32 sourceLength = i32(glslCode.size());
        const char* sourceName = filename.data();
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
                NOVA_LOG("{:3} : {}", ++lineNum, line);
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
        }), context->pAlloc, &impl->module));

        if (createShaderObject)
            impl->spirv = std::move(spirv);
    }

    ShaderImpl::~ShaderImpl()
    {
        context->PushDeletedObject(id);

        if (module)
            vkDestroyShaderModule(context->device, module, context->pAlloc);
    }

    VkPipelineShaderStageCreateInfo Shader::GetStageInfo() const noexcept
    {
        return {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = impl->stage,
            .module = impl->module,
            .pName = ShaderImpl::EntryPoint,
        };
    }

// -----------------------------------------------------------------------------

    std::string Shader::GenerateShader(
        Span<std::string> structures,
        Span<DescriptorSetLayout> sets,
        Span<std::string> bufferReferences,
        Span<std::string> fragments,
        ShaderEntryInfo entryInfo)
    {
        std::string shader = R"(#version 460

#extension GL_GOOGLE_include_directive                   : enable
#extension GL_EXT_scalar_block_layout                    : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
#extension GL_EXT_buffer_reference2                      : enable
#extension GL_EXT_nonuniform_qualifier                   : enable
#extension GL_EXT_ray_tracing                            : enable
#extension GL_EXT_ray_tracing_position_fetch             : enable
#extension GL_NV_shader_invocation_reorder               : enable
#extension GL_EXT_fragment_shader_barycentric            : enable
)";

        for (auto& structure : structures)
            (shader += structure) += '\n';

        for (u32 setIdx = 0; setIdx < sets.size(); ++setIdx)
        {
            auto& set = sets[setIdx];

            for (u32 bindingIdx = 0; bindingIdx < set->bindings.size(); ++bindingIdx)
            {
                auto& binding = set->bindings[bindingIdx];

                auto getArrayPart = [&](std::optional<u32> count) {
                    return count
                        ? count.value() == UINT32_MAX
                            ? "[]"
                            : NOVA_FORMAT_TEMP("[{}]", count.value()).c_str()
                        : "";
                };

                std::visit(Overloads {
                    [&](const binding::SampledTexture& binding) {
                        // TODO: Support 1D/3D
                        std::format_to(std::back_inserter(shader), "layout(set = {}, binding = {}) uniform sampler2D {}{};\n",
                            setIdx, bindingIdx, binding.name, getArrayPart(binding.count));
                    },
                    [&](const binding::StorageTexture& binding) {
                        // TODO: Support 1D/3D
                        const char* formatString;
                        const char* imageType;
                        switch (binding.format)
                        {
                        break;case Format::RGBA8U:
                                case Format::BGRA8U:
                            formatString = "rgba8";
                            imageType = "image2D";
                        break;case Format::R32UInt:
                            formatString = "r32ui";
                            imageType = "uimage2D";
                        break;default:
                            NOVA_THROW("Unknown format: {}", u32(binding.format));
                        }

                        std::format_to(std::back_inserter(shader), "layout(set = {}, binding = {}, {}) uniform {} {}{};\n",
                            setIdx, bindingIdx, formatString, imageType, binding.name, getArrayPart(binding.count));
                    },
                    [&](const binding::AccelerationStructure& binding) {
                        std::format_to(std::back_inserter(shader), "layout(set = {}, binding = {}) uniform accelerationStructureEXT {}{};\n",
                            setIdx, bindingIdx, binding.name, getArrayPart(binding.count));
                    }
                }, binding);
            }
        }

        for (auto& type : bufferReferences)
        {
            std::format_to(std::back_inserter(shader), R"(layout(buffer_reference, scalar) buffer {0}_br {{ {0} data[]; }};
#define {0}_BR(va) {0}_br(va).data
)", type);
        }

        std::visit(Overloads {
            [&](shader_entry_info::ComputeInfo& compute) {
                std::format_to(std::back_inserter(shader),
                    "layout(local_size_x = {}, local_size_y = {}, local_size_z = {}) in;\n",
                    compute.workGroups.x, compute.workGroups.y, compute.workGroups.z);
            },
            [](auto&) {},
        }, entryInfo);

        for (auto& fragment : fragments)
            (shader += fragment) += '\n';

        return shader;
    }
}