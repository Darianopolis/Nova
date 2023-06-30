#include "nova_VulkanRHI.hpp"

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
                // userData->content = nova::ReadFileToString(userData->name);
                NOVA_THROW("Include load file not supported yet");
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

    Shader VulkanContext::Shader_Create(ShaderStage stage, const std::string& filename, const std::string& sourceCode)
    {
        auto[id, shader] = shaders.Acquire();

        shader.stage = VkShaderStageFlagBits(stage);
        shader.id = GetUID();

        NOVA_DO_ONCE() { glslang::InitializeProcess(); };
        NOVA_ON_EXIT() { glslang::FinalizeProcess(); };

        EShLanguage glslangStage;
        switch (shader.stage)
        {
        break;case VK_SHADER_STAGE_VERTEX_BIT:                  glslangStage = EShLangVertex;
        break;case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:    glslangStage = EShLangTessControl;
        break;case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: glslangStage = EShLangTessEvaluation;
        break;case VK_SHADER_STAGE_GEOMETRY_BIT:                glslangStage = EShLangGeometry;
        break;case VK_SHADER_STAGE_FRAGMENT_BIT:                glslangStage = EShLangFragment;
        break;case VK_SHADER_STAGE_COMPUTE_BIT:                 glslangStage = EShLangCompute;
        break;case VK_SHADER_STAGE_RAYGEN_BIT_KHR:              glslangStage = EShLangRayGen;
        break;case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:             glslangStage = EShLangAnyHit;
        break;case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:         glslangStage = EShLangClosestHit;
        break;case VK_SHADER_STAGE_MISS_BIT_KHR:                glslangStage = EShLangMiss;
        break;case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:        glslangStage = EShLangIntersect;
        break;case VK_SHADER_STAGE_CALLABLE_BIT_KHR:            glslangStage = EShLangCallable;
        break;case VK_SHADER_STAGE_TASK_BIT_EXT:                glslangStage = EShLangTask;
        break;case VK_SHADER_STAGE_MESH_BIT_EXT:                glslangStage = EShLangMesh;

        break;default: NOVA_THROW("Unknown stage: {}", int(shader.stage));
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
        const char* sourceName = "shader";
        glslShader.setStringsWithLengthsAndNames(&source, &sourceLength, &sourceName, 1);

        // ---- Defines ----

        // ---- Includes ----

        GlslangIncluder includer;

        // ---- Preprocessing ----

        auto printShader = [&] {
            std::istringstream iss(glslCode);
            std::string line;
            u32 lineNum = 0;
            while (std::getline(iss, line))
            {
                NOVA_LOG("{:3} : {}", ++lineNum, line);
            }
        };

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
            printShader();
            NOVA_THROW("GLSL preprocessing failed {}\n{}\n{}", filename, glslShader.getInfoLog(), glslShader.getInfoDebugLog());
        }

        const char* preprocessedCStr = preprocessed.c_str();
        glslShader.setStrings(&preprocessedCStr, 1);

        // ---- Parsing ----

        if (!glslShader.parse(resource, 100, ENoProfile, EShMessages::EShMsgDefault))
        {
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

        VkCall(vkCreateShaderModule(device, Temp(VkShaderModuleCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv.size() * 4,
            .pCode = spirv.data(),
        }), pAlloc, &shader.handle));

        return id;
    }

    Shader VulkanContext::Shader_Create(ShaderStage stage, Span<ShaderElement> elements)
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

#define write(...) std::format_to(std::back_insert_iterator(shader), __VA_ARGS__)

        auto typeToString = [](ShaderVarType type)
        {
            switch (type)
            {
            break;case ShaderVarType::Mat2:
                return "mat2";
            break;case ShaderVarType::Mat3:
                return "mat3";
            break;case ShaderVarType::Mat4:
                return "mat4";

            break;case ShaderVarType::Mat4x3:
                return "mat4x3";
            break;case ShaderVarType::Mat3x4:
                return "mat3x4";

            break;case ShaderVarType::Vec2:
                return "vec2";
            break;case ShaderVarType::Vec3:
                return "vec3";
            break;case ShaderVarType::Vec4:
                return "vec4";

            break;case ShaderVarType::U32:
                return "uint";
            break;case ShaderVarType::U64:
                return "uint64_t";

            break;case ShaderVarType::I32:
                return "int";
            break;case ShaderVarType::I64:
                return "int64_t";

            break;case ShaderVarType::F32:
                return "float";
            break;case ShaderVarType::F64:
                return "double";
            }

            std::unreachable();
        };

        auto getArrayPart = [&](std::optional<u32> count) {
            return count
                ? count.value() == shader::ArrayCountUnsized
                    ? "[]"
                    : NOVA_FORMAT_TEMP("[{}]", count.value()).c_str()
                : "";
        };

        u32 structureId = 0;
        auto getAnonStructureName = [&] {
            return std::format("GeneratedStructure{}_t", ++structureId);
        };

        u32 inputLocation = 0;
        u32 outputLocation = 0;
        auto getTypeLocationWidth = [](ShaderVarType type) {

            switch (type)
            {
            break;case ShaderVarType::Mat2:
                return 2;
            break;case ShaderVarType::Mat3:
                return 3;
            break;case ShaderVarType::Mat4:
                return 4;

            break;default:
                return 1;
            }
        };

        for (auto& element : elements)
        {
            std::visit(Overloads {
// -----------------------------------------------------------------------------
//                          Structure declaration
// -----------------------------------------------------------------------------
                [&](const shader::Structure& structure) {
                    write("struct {} {{\n", structure.name);
                    for (auto& member : structure.members)
                    {
                        write("    {} {}{};\n",
                            typeToString(member.type), member.name, getArrayPart(member.count));
                    }
                    write("}};\n");
                    write("layout(buffer_reference, scalar) buffer {0}_br {{ {0} data[]; }};\n"
                        "#define {0}_BR(va) {0}_br(va).data\n",
                        structure.name);
                },
// -----------------------------------------------------------------------------
//                             Pipeline Layout
// -----------------------------------------------------------------------------
                [&](const shader::Layout& layout) {
                    auto pipelineLayout = layout.layout;

                    // Push Constants

                    for (auto& pushConstants : Get(pipelineLayout).pcRanges)
                    {
                        write("layout(push_constant, scalar) uniform {} {{\n", getAnonStructureName());
                        for (auto& member : pushConstants.constants)
                        {
                            write("    {} {}{};\n",
                                typeToString(member.type), member.name, getArrayPart(member.count));
                        }
                        write("}} {};\n", pushConstants.name);
                    }

                    // Descriptor Sets

                    for (u32 setIdx = 0; setIdx < Get(pipelineLayout).sets.size(); ++setIdx)
                    {
                        auto set = Get(pipelineLayout).setLayouts[setIdx];
                        for (u32 bindingIdx = 0; bindingIdx < Get(set).bindings.size(); ++bindingIdx)
                        {
                            auto& binding = Get(set).bindings[bindingIdx];

                            std::visit(Overloads {
                                [&](const binding::SampledTexture& binding) {
                                    // TODO: Support 1D/3D
                                    write("layout(set = {}, binding = {}) uniform sampler2D {}{};\n",
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
                                            case Format::RGBA8_SRGB:
                                            case Format::BGRA8_SRGB:
                                        formatString = "rgba8";
                                        imageType = "image2D";
                                    break;case Format::R32UInt:
                                        formatString = "r32ui";
                                        imageType = "uimage2D";
                                    break;default:
                                        NOVA_THROW("Unknown format: {}", u32(binding.format));
                                    }

                                    write("layout(set = {}, binding = {}, {}) uniform {} {}{};\n",
                                        setIdx, bindingIdx, formatString, imageType, binding.name, getArrayPart(binding.count));
                                },
                                [&](const binding::AccelerationStructure& binding) {
                                    write("layout(set = {}, binding = {}) uniform accelerationStructureEXT {}{};\n",
                                        setIdx, bindingIdx, binding.name, getArrayPart(binding.count));
                                },
                                [&](const binding::UniformBuffer& binding) {
                                    write("layout(set = {}, binding = {}) uniform {} {{\n",
                                        setIdx, bindingIdx, getAnonStructureName());
                                    for (auto& member : binding.members)
                                    {
                                        write("    {} {}{};\n",
                                            typeToString(member.type), member.name, getArrayPart(member.count));
                                    }
                                    write("}} {}{};\n", binding.name, getArrayPart(binding.count));
                                }
                            }, binding);
                        }
                    }
                },
// -----------------------------------------------------------------------------
//                            Buffer Reference
// -----------------------------------------------------------------------------
                [&](const shader::BufferReference& bufferReference) {
                    write("layout(buffer_reference, scalar) buffer {0}_br {{ {1} data[]; }};\n"
                        "#define {0}_BR(va) {0}_br(va).data\n",
                        bufferReference.name,
                        bufferReference.scalarType
                            ? typeToString(bufferReference.scalarType.value())
                            : bufferReference.name);
                },
// -----------------------------------------------------------------------------
//                          Shader Input Variable
// -----------------------------------------------------------------------------
                [&](const shader::Input& input) {
                    write("layout(location = {}) in", inputLocation);
                    if (input.flags >= ShaderInputFlags::Flat)
                        write(" flat");
                    if (input.flags >= ShaderInputFlags::PerVertex)
                        write(" pervertexEXT");
                    write(" {} {}", typeToString(input.type), input.name);
                    if (input.flags >= ShaderInputFlags::PerVertex)
                        write("[3]"); // TODO: Primitive vertex count?
                    write(";\n");

                    inputLocation += getTypeLocationWidth(input.type);
                },
// -----------------------------------------------------------------------------
//                          Shader Output Variable
// -----------------------------------------------------------------------------
                [&](const shader::Output& output) {
                    write("layout(location = {}) out {} {};\n",
                        outputLocation, typeToString(output.type), output.name);

                    outputLocation += getTypeLocationWidth(output.type);
                },
// -----------------------------------------------------------------------------
//                              GLSL Fragment
// -----------------------------------------------------------------------------
                [&](const shader::Fragment& fragment) {
                    write("{}", fragment.glsl);
                },
// -----------------------------------------------------------------------------
//                             Compute Kernel
// -----------------------------------------------------------------------------
                [&](const shader::ComputeKernel& computeKernel) {
                    write("layout(local_size_x = {}, local_size_y = {}, local_size_z = {}) in;\nvoid main() {{\n{}\n}}\n",
                        computeKernel.workGroups.x, computeKernel.workGroups.y, computeKernel.workGroups.z,
                        computeKernel.glsl);
                },
// -----------------------------------------------------------------------------
//                                 Kernel
// -----------------------------------------------------------------------------
                [&](const shader::Kernel& kernel) {
                    write("void main() {{\n{}\n}}\n", kernel.glsl);
                },
            }, element);
        }

#undef write

        // NOVA_LOG("Generated shader:\n{}", shader);

        return Shader_Create(stage, "generated", shader);
    }

    void VulkanContext::Shader_Destroy(Shader id)
    {
        if (Get(id).handle)
            vkDestroyShaderModule(device, Get(id).handle, pAlloc);

        shaders.Return(id);
    }
}