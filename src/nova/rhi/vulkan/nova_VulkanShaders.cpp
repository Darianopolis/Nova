#include "nova_VulkanRHI.hpp"

#include <nova/core/nova_Files.hpp>

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

            } else for (auto& dir : includeDirs) {
                target = dir / requested;
                if (std::filesystem::exists(target)) {
                    exists = true;
                    break;
                }
            }

            auto userData = new ShaderIncludeUserData{};

            if (exists) {
                userData->name = target.string();
                // userData->content = nova::files::ReadTextFile(userData->name);
                NOVA_THROW("Include load file not supported yet");
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

    static
    void CompileShader(Shader shader, const std::string& filename, const std::string& sourceCode)
    {
        shader->id = shader->context->GetUID();

        NOVA_DO_ONCE() { glslang::InitializeProcess(); };
        NOVA_ON_EXIT() { glslang::FinalizeProcess(); };

        EShLanguage glslangStage;
        switch (GetVulkanShaderStage(shader->stage)) {
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

        break;default: NOVA_THROW("Unknown stage: {}", int(shader->stage));
        }

        glslang::TShader glslShader { glslangStage };
        auto resource = GetDefaultResources();
        glslShader.setEnvInput(glslang::EShSourceGlsl, glslangStage, glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
        glslShader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
        glslShader.setEnvTarget(glslang::EshTargetSpv, glslang::EShTargetSpv_1_6);

        // ---- Source ----

        const std::string& glslCode = sourceCode.empty() ? files::ReadTextFile(filename) : sourceCode;
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
            while (std::getline(iss, line)) {
                NOVA_LOG("{:3} : {}", ++lineNum, line);
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

        if (!glslShader.parse(resource, 100, ENoProfile, EShMessages::EShMsgDefault)) {
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
        glslang::GlslangToSpv(*intermediate, spirv, &spvOptions);

        if (!logger.getAllMessages().empty()) {
            NOVA_LOG("Shader ({}) SPIR-V messages:\n{}", filename, logger.getAllMessages());
        }

        // TODO: remove shader module creation,
        //   store spirv and simply pass to pipelines

        VkCall(vkCreateShaderModule(shader->context->device, Temp(VkShaderModuleCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv.size() * 4,
            .pCode = spirv.data(),
        }), shader->context->pAlloc, &shader->handle));
    }

    Shader Shader::Create(HContext context, ShaderStage stage, const std::string& filename, const std::string& sourceCode)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->stage = stage;

        Shader shader{ impl };
        CompileShader(shader, filename, sourceCode);
        return shader;
    }

    Shader Shader::Create(HContext context, ShaderStage stage, Span<ShaderElement> elements)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->stage = stage;

        std::string codeStr = "#version 460\n";
        auto code = std::back_insert_iterator(codeStr);

        constexpr auto Extensions = std::array {
            "GL_GOOGLE_include_directive",

            "GL_EXT_scalar_block_layout",
            "GL_EXT_buffer_reference2",
            "GL_EXT_nonuniform_qualifier",

            "GL_EXT_shader_explicit_arithmetic_types_int8",
            "GL_EXT_shader_explicit_arithmetic_types_int16",
            "GL_EXT_shader_explicit_arithmetic_types_int32",
            "GL_EXT_shader_explicit_arithmetic_types_int64",
            "GL_EXT_shader_explicit_arithmetic_types_float16",
            "GL_EXT_shader_explicit_arithmetic_types_float32",
            "GL_EXT_shader_explicit_arithmetic_types_float64",

            "GL_EXT_fragment_shader_barycentric",

            "GL_EXT_ray_tracing",
            "GL_EXT_ray_tracing_position_fetch",
            "GL_NV_shader_invocation_reorder",
        };

        for (auto& extension : Extensions) {
            std::format_to(code, "#extension {} : enable\n", extension);
        }

        constexpr auto Dimensions = std::array {
            "1D",
            "2D",
            "3D",

            "1DArray",
            "2DArray",

            "Cube",
            "CubeArray",
        };

        constexpr auto ImageFormats = std::array {

            // floating-point formats
            std::pair("rgba32f",        ""),
            std::pair("rgba16f",        ""),
            std::pair("rg32f",          ""),
            std::pair("rg16f",          ""),
            std::pair("r11f_g11f_b10f", ""),
            std::pair("r32f",           ""),
            std::pair("r16f",           ""),

            // unsigned-normalized formats
            std::pair("rgba16",         ""),
            std::pair("rgb10_a2",       ""),
            std::pair("rgba8",          ""),
            std::pair("rg16",           ""),
            std::pair("rg8",            ""),
            std::pair("r16",            ""),
            std::pair("r8",             ""),

            // signed-normalized formats
            std::pair("rgba16_snorm",   ""),
            std::pair("rgba8_snorm",    ""),
            std::pair("rg16_snorm",     ""),
            std::pair("rg8_snorm",      ""),
            std::pair("r16_snorm",      ""),
            std::pair("r8_snorm",       ""),

            // signed-integer formats
            std::pair("rgba32i", "i"),
            std::pair("rgba16i", "i"),
            std::pair("rgba8i",  "i"),
            std::pair("rg32i",   "i"),
            std::pair("rg16i",   "i"),
            std::pair("rg8i",    "i"),
            std::pair("r32i",    "i"),
            std::pair("r16i",    "i"),
            std::pair("r8i",     "i"),

            // unsigned-integer formats
            std::pair("rgba32ui",   "u"),
            std::pair("rgba16ui",   "u"),
            std::pair("rgb10_a2ui", "u"),
            std::pair("rgba8ui",    "u"),
            std::pair("rg32ui",     "u"),
            std::pair("rg16ui",     "u"),
            std::pair("rg8ui",      "u"),
            std::pair("r32ui",      "u"),
            std::pair("r16ui",      "u"),
            std::pair("r8ui",       "u"),
        };

        constexpr std::array UniformTexelFormats {
            std::pair("", "float"),
            std::pair("i",  "int"),
            std::pair("u", "uint"),
        };

        // TODO: Bind these lazily

        for (auto format : ImageFormats) {
            for (auto dims : Dimensions) {
                std::format_to(code, "layout(set = 0, binding = 0, {}) uniform {}image{} nova_StorageImage{}_{}[];\n",
                    format.first, format.second, dims, dims, format.first);
            }

            std::format_to(code, "layout(set = 0, binding = 0, {}) uniform {}imageBuffer nova_StorageTexelBuffer_{}[];\n",
                format.first, format.second, format.first);
        }

        for (auto dims : Dimensions) {
            std::format_to(code, "layout(set = 0, binding = 0) uniform texture{0} nova_SampledImage{0}[];\n", dims);
        }

        for (auto type : UniformTexelFormats) {
            std::format_to(code, "layout(set = 0, binding = 0) uniform {}textureBuffer nova_UniformTexelBuffer_{}[];\n",
                type.first, type.second);
        }

        // Sampler heap
        // TODO: This should be a separate set

        std::format_to(code, "layout(set = 0, binding = 1) uniform sampler nova_Sampler[];\n");

        for (auto dims : Dimensions) {
            std::format_to(code, "#define nova_Sampler{0}(texture, sampler) sampler{0}(nova_SampledImage{0}[texture], nova_Sampler[sampler])\n", dims);
        }

        std::format_to(code, "layout(set = 1, binding = 0) uniform accelerationStructureEXT nova_AccelerationStructure;\n");

        // Transform GLSL

        thread_local std::string TransformOutputA;
        thread_local std::string TransformOutputB;
        auto transformGlsl = [](const std::string& glsl) -> std::string& {

            // Convert "templated" descriptor heap accesses
            static std::regex DescriptorFind = [&] {
                std::string regex = R"(\bnova::(UniformBuffer|StorageBuffer|BufferReference|StorageTexelBuffer)";
                for (auto dims : Dimensions) {
                    std::format_to(std::back_insert_iterator(regex), "|StorageImage{}", dims);
                }
                return std::regex{ regex += R"()<(\w+)>)" };
            }();
            TransformOutputA.clear();
            std::regex_replace(std::back_insert_iterator(TransformOutputA), glsl.begin(), glsl.end(),
                DescriptorFind, "nova_$1_$2");

            // Convert remaining nova:: prefixes
            TransformOutputB.clear();
            static std::regex NovaPrefixFind{ R"(\bnova::)" };
            std::regex_replace(std::back_insert_iterator(TransformOutputB), TransformOutputA.begin(), TransformOutputA.end(),
                NovaPrefixFind, "nova_");

            return TransformOutputB;
        };

        auto typeToString = [](ShaderVarType type) {
            switch (type) {
            break;case ShaderVarType::Mat2: return "mat2";
            break;case ShaderVarType::Mat3: return "mat3";
            break;case ShaderVarType::Mat4: return "mat4";

            break;case ShaderVarType::Mat4x3: return "mat4x3";
            break;case ShaderVarType::Mat3x4: return "mat3x4";

            break;case ShaderVarType::Vec2: return "vec2";
            break;case ShaderVarType::Vec3: return "vec3";
            break;case ShaderVarType::Vec4: return "vec4";

            break;case ShaderVarType::Vec2U: return "uvec2";
            break;case ShaderVarType::Vec3U: return "uvec3";
            break;case ShaderVarType::Vec4U: return "uvec4";

            break;case ShaderVarType::U32: return "uint";
            break;case ShaderVarType::U64: return "uint64_t";

            break;case ShaderVarType::I16: return "int16_t";
            break;case ShaderVarType::I32: return "int";
            break;case ShaderVarType::I64: return "int64_t";

            break;case ShaderVarType::F32: return "float";
            break;case ShaderVarType::F64: return "double";
            }

            std::unreachable();
        };

        auto getArrayPart = [](std::optional<u32> count) {
            return count
                ? count.value() == shader::ArrayCountUnsized
                    ? "[]"
                    : NOVA_FORMAT_TEMP("[{}]", count.value()).c_str()
                : "";
        };

        u32 structureId = 0;
        auto getAnonStructureName = [&] {
            return std::format("_{}_", ++structureId);
        };

        u32 inputLocation = 0;
        u32 outputLocation = 0;
        auto getTypeLocationWidth = [](ShaderVarType type) {

            switch (type)  {
            break;case ShaderVarType::Mat2: return 2;
            break;case ShaderVarType::Mat3: return 3;
            break;case ShaderVarType::Mat4: return 4;
            break;default:                  return 1;
            }
        };

        for (auto& element : elements) {
            std::visit(Overloads {
// -----------------------------------------------------------------------------
//                          Structure declaration
// -----------------------------------------------------------------------------
                [&](const shader::Structure& structure) {
                    std::format_to(code, "struct {} {{\n", structure.name);
                    for (auto& member : structure.members) {
                        std::format_to(code, "    {} {}{};\n",
                            typeToString(member.type), member.name, getArrayPart(member.count));
                    }
                    std::format_to(code, "}};\n");
                    std::format_to(code, "layout(buffer_reference, scalar) buffer nova_BufferReference_{0} {{ {0} data[]; }};\n",          structure.name);
                    std::format_to(code, "layout(set = 0, binding = 0, scalar) uniform {1} {{ {0} data[]; }} nova_UniformBuffer_{0}[];\n", structure.name, getAnonStructureName());
                    std::format_to(code, "layout(set = 0, binding = 0, scalar) buffer {1} {{ {0} data[]; }} nova_StorageBuffer_{0}[];\n",  structure.name, getAnonStructureName());
                },
// -----------------------------------------------------------------------------
//                              Push Constants
// -----------------------------------------------------------------------------
                [&](const shader::PushConstants& pushConstants) {
                    std::format_to(code, "layout(push_constant, scalar) uniform {} {{\n", getAnonStructureName());
                    for (auto& member : pushConstants.members) {
                        std::format_to(code, "    {} {}{};\n",
                            typeToString(member.type), member.name, getArrayPart(member.count));
                    }
                    std::format_to(code, "}} {};\n", pushConstants.name);
                },
// -----------------------------------------------------------------------------
//                            Buffer Reference
// -----------------------------------------------------------------------------
                [&](const shader::BufferReference& bufferReference) {
                    std::format_to(code, "layout(buffer_reference, scalar) buffer nova_BufferReference_{0} {{ {1} data[]; }};\n",
                        bufferReference.name,
                        bufferReference.scalarType
                            ? typeToString(bufferReference.scalarType.value())
                            : bufferReference.name);
                },
// -----------------------------------------------------------------------------
//                          Shader Input Variable
// -----------------------------------------------------------------------------
                [&](const shader::Input& input) {
                    std::format_to(code, "layout(location = {}) in", inputLocation);
                    if (input.flags >= ShaderInputFlags::Flat) {
                        std::format_to(code, " flat");
                    }
                    if (input.flags >= ShaderInputFlags::PerVertex) {
                        std::format_to(code, " pervertexEXT");
                    }
                    std::format_to(code, " {} {}", typeToString(input.type), input.name);
                    if (input.flags >= ShaderInputFlags::PerVertex) {
                        std::format_to(code, "[3]"); // TODO: Primitive vertex count?
                    }
                    std::format_to(code, ";\n");

                    inputLocation += getTypeLocationWidth(input.type);
                },
// -----------------------------------------------------------------------------
//                          Shader Output Variable
// -----------------------------------------------------------------------------
                [&](const shader::Output& output) {
                    std::format_to(code, "layout(location = {}) out {} {};\n",
                        outputLocation, typeToString(output.type), output.name);

                    outputLocation += getTypeLocationWidth(output.type);
                },
// -----------------------------------------------------------------------------
//                              GLSL Fragment
// -----------------------------------------------------------------------------
                [&](const shader::Fragment& fragment) {
                    std::format_to(code, "{}", transformGlsl(fragment.glsl));
                },
// -----------------------------------------------------------------------------
//                             Compute Kernel
// -----------------------------------------------------------------------------
                [&](const shader::ComputeKernel& computeKernel) {
                    std::format_to(code, "layout(local_size_x = {}, local_size_y = {}, local_size_z = {}) in;\nvoid main() {{\n{}\n}}\n",
                        computeKernel.workGroups.x, computeKernel.workGroups.y, computeKernel.workGroups.z,
                        transformGlsl(computeKernel.glsl));
                },
// -----------------------------------------------------------------------------
//                                 Kernel
// -----------------------------------------------------------------------------
                [&](const shader::Kernel& kernel) {
                    std::format_to(code, "void main() {{\n{}\n}}\n", transformGlsl(kernel.glsl));
                },
            }, element);
        }

        NOVA_LOG("Generated shader:\n{}", codeStr);

        Shader shader{ impl };
        CompileShader(shader, "generated", codeStr);
        return shader;
    }

    void Shader::Destroy()
    {
        if (!impl) {
            return;
        }

        vkDestroyShaderModule(impl->context->device, impl->handle, impl->context->pAlloc);

        delete impl;
        impl = nullptr;
    }

    VkPipelineShaderStageCreateInfo Shader::Impl::GetStageInfo() const
    {
        return {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VkShaderStageFlagBits(GetVulkanShaderStage(stage)),
            .module = handle,
            .pName = "main",
        };
    }
}