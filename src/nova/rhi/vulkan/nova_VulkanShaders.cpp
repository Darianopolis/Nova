#include "nova_VulkanRHI.hpp"

#include <nova/core/nova_Files.hpp>

#include <nova/lang/backends/nova_VulkanGlslBackend.hpp>

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
        bool generateShaderObject = true;
        VkShaderStageFlags nextStages = 0;

        switch (GetVulkanShaderStage(shader->stage)) {
        break;case VK_SHADER_STAGE_VERTEX_BIT:                  glslangStage = EShLangVertex;
                                                                nextStages = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT
                                                                    | VK_SHADER_STAGE_GEOMETRY_BIT
                                                                    | VK_SHADER_STAGE_FRAGMENT_BIT;
        break;case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:    glslangStage = EShLangTessControl;
                                                                nextStages = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        break;case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: glslangStage = EShLangTessEvaluation;
                                                                nextStages = VK_SHADER_STAGE_GEOMETRY_BIT
                                                                    | VK_SHADER_STAGE_FRAGMENT_BIT;
        break;case VK_SHADER_STAGE_GEOMETRY_BIT:                glslangStage = EShLangGeometry;
                                                                nextStages = VK_SHADER_STAGE_FRAGMENT_BIT;
        break;case VK_SHADER_STAGE_FRAGMENT_BIT:                glslangStage = EShLangFragment;
        break;case VK_SHADER_STAGE_COMPUTE_BIT:                 glslangStage = EShLangCompute;
        break;case VK_SHADER_STAGE_RAYGEN_BIT_KHR:              glslangStage = EShLangRayGen;
                                                                generateShaderObject = false;
        break;case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:             glslangStage = EShLangAnyHit;
                                                                generateShaderObject = false;
        break;case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:         glslangStage = EShLangClosestHit;
                                                                generateShaderObject = false;
        break;case VK_SHADER_STAGE_MISS_BIT_KHR:                glslangStage = EShLangMiss;
                                                                generateShaderObject = false;
        break;case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:        glslangStage = EShLangIntersect;
                                                                generateShaderObject = false;
        break;case VK_SHADER_STAGE_CALLABLE_BIT_KHR:            glslangStage = EShLangCallable;
                                                                generateShaderObject = false;
        break;case VK_SHADER_STAGE_TASK_BIT_EXT:                glslangStage = EShLangTask;
                                                                nextStages = VK_SHADER_STAGE_MESH_BIT_EXT;
        break;case VK_SHADER_STAGE_MESH_BIT_EXT:                glslangStage = EShLangMesh;
                                                                nextStages = VK_SHADER_STAGE_FRAGMENT_BIT;

        break;default: NOVA_THROW("Unknown stage: {}", int(shader->stage));
        }

        if (!shader->context->shaderObjects)
            generateShaderObject = false;

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

        auto context = shader->context;

        vkh::Check(vkCreateShaderModule(context->device, Temp(VkShaderModuleCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv.size() * 4,
            .pCode = spirv.data(),
        }), context->pAlloc, &shader->handle));

        if (generateShaderObject) {
            vkh::Check(vkCreateShadersEXT(context->device, 1, Temp(VkShaderCreateInfoEXT {
                .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
                .stage = VkShaderStageFlagBits(GetVulkanShaderStage(shader->stage)),
                .nextStage = nextStages,
                .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
                .codeSize = spirv.size() * 4,
                .pCode = spirv.data(),
                .pName = "main",
                .setLayoutCount = context->config.rayTracing ? 2u : 1u,
                .pSetLayouts = std::array {
                    context->heapLayout,
                    context->rtLayout,
                }.data(),
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = Temp(VkPushConstantRange {
                    .stageFlags = VK_SHADER_STAGE_ALL,
                    .size = 128,
                }),
            }), context->pAlloc, &shader->shader));
        }
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

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------

    static
    std::string ShaderPreamble(Context context, ShaderStage stage)
    {
        std::string codeStr = "#version 460\n";
        auto code = std::back_insert_iterator(codeStr);

        constexpr auto Extensions = std::to_array<std::pair<const char*, VkShaderStageFlags>>({
            std::pair("GL_GOOGLE_include_directive", VK_SHADER_STAGE_ALL),

            std::pair("GL_EXT_scalar_block_layout", VK_SHADER_STAGE_ALL),
            std::pair("GL_EXT_buffer_reference2", VK_SHADER_STAGE_ALL),
            std::pair("GL_EXT_nonuniform_qualifier", VK_SHADER_STAGE_ALL),

            std::pair("GL_EXT_shader_explicit_arithmetic_types_int8", VK_SHADER_STAGE_ALL),
            std::pair("GL_EXT_shader_explicit_arithmetic_types_int16", VK_SHADER_STAGE_ALL),
            std::pair("GL_EXT_shader_explicit_arithmetic_types_int32", VK_SHADER_STAGE_ALL),
            std::pair("GL_EXT_shader_explicit_arithmetic_types_int64", VK_SHADER_STAGE_ALL),
            std::pair("GL_EXT_shader_explicit_arithmetic_types_float16", VK_SHADER_STAGE_ALL),
            std::pair("GL_EXT_shader_explicit_arithmetic_types_float32", VK_SHADER_STAGE_ALL),
            std::pair("GL_EXT_shader_explicit_arithmetic_types_float64", VK_SHADER_STAGE_ALL),

            std::pair("GL_EXT_fragment_shader_barycentric", VK_SHADER_STAGE_FRAGMENT_BIT),

            std::pair("GL_EXT_mesh_shader", VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT),

            std::pair("GL_EXT_ray_tracing", VK_SHADER_STAGE_ALL),
            std::pair("GL_EXT_ray_query", VK_SHADER_STAGE_ALL),
            std::pair("GL_EXT_ray_tracing_position_fetch", VK_SHADER_STAGE_ALL),
            std::pair("GL_NV_shader_invocation_reorder", VK_SHADER_STAGE_ALL),
        });

        {
            auto vkStage = GetVulkanShaderStage(stage);
            for (auto& extension : Extensions) {
                if (vkStage & extension.second) {
                    std::format_to(code, "#extension {} : enable\n", extension.first);
                }
            }
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
                std::format_to(code, "layout(set = 0, binding = 0, {}) uniform {}image{} StorageImage{}_{}[];\n",
                    format.first, format.second, dims, dims, format.first);
            }

            std::format_to(code, "layout(set = 0, binding = 0, {}) uniform {}imageBuffer StorageTexelBuffer_{}[];\n",
                format.first, format.second, format.first);
        }

        for (auto dims : Dimensions) {
            std::format_to(code, "layout(set = 0, binding = 0) uniform texture{0} SampledImage{0}[];\n", dims);
        }

        for (auto type : UniformTexelFormats) {
            std::format_to(code, "layout(set = 0, binding = 0) uniform {}textureBuffer UniformTexelBuffer_{}[];\n",
                type.first, type.second);
        }

        // Samplers

        std::format_to(code, "layout(set = 0, binding = 0) uniform sampler Sampler[];\n");

        for (auto dims : Dimensions) {
            std::format_to(code, "#define Sampler{0}(texture, sampler) sampler{0}(SampledImage{0}[texture], Sampler[sampler])\n", dims);
        }

        // Acceleration structure

        if (context->config.rayTracing) {
            std::format_to(code, "layout(set = 1, binding = 0) uniform accelerationStructureEXT AccelerationStructure;\n");
        }

        return codeStr;
    }

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------

    Shader Shader::Create2(HContext context, ShaderStage stage, Span<ShaderElement> elements)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->stage = stage;

        std::string codeStr = ShaderPreamble(context, stage);
        auto code = std::back_insert_iterator(codeStr);

        auto shaderVarTypeToString = [](ShaderVarType type) {
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

        nova::Compiler compiler;

        nova::Scanner scanner;
        scanner.compiler = &compiler;

        nova::Parser parser;
        parser.scanner = &scanner;

        nova::VulkanGlslBackend backend;
        backend.parser = &parser;

        backend.RegisterGlobal("gl_Position",    backend.FindType("vec4"));
        backend.RegisterGlobal("gl_VertexIndex", backend.FindType("uint"));

        auto getType = [&](const ShaderType& shaderType) {
            Type* type = nullptr;

            std::visit(Overloads {
                [&](const BufferReferenceType& br) {
                    auto accessor = backend.RegisterAccessor(
                        backend.FindType(br.element),
                        VulkanGlslBackend::AccessorMode::BufferReference,
                        br.readonly);
                    type = accessor->accessorType;
                },
                [&](const UniformBufferType& ub) {
                    NOVA_LOG("  IS UNIFORM BUFFER");
                    auto accessor = backend.RegisterAccessor(
                        backend.FindType(ub.element),
                        VulkanGlslBackend::AccessorMode::UniformBuffer,
                        ub.readonly);
                    type = accessor->accessorType;
                },
                [&](const StorageBufferType& sb) {
                    NOVA_LOG("  IS STORAGE BUFFER");
                    auto accessor = backend.RegisterAccessor(
                        backend.FindType(sb.element),
                        VulkanGlslBackend::AccessorMode::StorageBuffer,
                        sb.readonly);
                    type = accessor->accessorType;
                },
                [&](const ShaderVarType& varType) {
                    type = backend.FindType(shaderVarTypeToString(varType));
                },
                [&](const auto&) {
                    NOVA_THROW("Unknown type");
                }
            }, shaderType);

            NOVA_LOG("FOUND TYPE: {}", type->name);

            return type;
        };
        auto getTypeString = [&](const ShaderType& shaderType) {
            auto type = getType(shaderType);
            if (std::holds_alternative<StorageBufferType>(shaderType)
                    || std::holds_alternative<UniformBufferType>(shaderType)) {
                return "uvec2"sv;
            }

            return type->name;
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

        // Structures

        std::string fragments;

        for (auto& element : elements) {
            std::visit(Overloads {
                [&](const shader::Structure& structure) {
                    auto s = backend.FindType(structure.name);
                    s->structure = new Struct;
                    for (auto& member : structure.members) {
                        s->structure->members.insert({ member.name, getType(member.type) });
                    }

                    // Emit code

                    std::format_to(code, "struct {} {{\n", structure.name);
                    for (auto& member : structure.members) {
                        std::format_to(code, "    {} {};\n",
                            getTypeString(member.type), member.name);
                    }
                    std::format_to(code, "}};\n");

                    // Buffer reference

                    std::format_to(code, "layout(buffer_reference, scalar, buffer_reference_align = 4) "
                        "buffer {0}_buffer_reference {{ {0} get; }};\n", structure.name);
                    std::format_to(code, "layout(buffer_reference, scalar, buffer_reference_align = 4) "
                        "readonly buffer {0}_readonly_buffer_reference {{ {0} get; }};\n", structure.name);

                    // Uniform buffer

                    std::format_to(code, "layout(set = 0, binding = 0, scalar) uniform {1} {{ {0} data[1024]; }} "
                        "{0}_uniform_buffer[];\n", structure.name, getAnonStructureName());
                    std::format_to(code, "layout(set = 0, binding = 0, scalar) readonly uniform {1} {{ {0} data[1024]; }} "
                        "{0}_readonly_uniform_buffer[];\n", structure.name, getAnonStructureName());

                    // Storage buffer

                    std::format_to(code, "layout(set = 0, binding = 0, scalar) buffer {1} {{ {0} data[]; }} "
                        "{0}_storage_buffer[];\n", structure.name, getAnonStructureName());
                    std::format_to(code, "layout(set = 0, binding = 0, scalar) readonly buffer {1} {{ {0} data[]; }} "
                        "{0}_readonly_storage_buffer[];\n", structure.name, getAnonStructureName());
                },
                [&](const shader::PushConstants& pushConstants) {
                    auto anonName = getAnonStructureName();
                    auto s = backend.FindType(anonName);
                    s->structure = new Struct;
                    for (auto& member : pushConstants.members) {
                        s->structure->members.insert({ member.name, getType(member.type) });
                    }
                    backend.RegisterGlobal(pushConstants.name, s);

                    // Emit code

                    std::format_to(code, "layout(push_constant, scalar) uniform {} {{\n", getAnonStructureName());
                    for (auto& member : pushConstants.members) {
                        std::format_to(code, "    {} {};\n", getTypeString(member.type), member.name);
                    }
                    std::format_to(code, "}} {};\n", pushConstants.name);
                },
                [&](const shader::Input& input) {
                    backend.RegisterGlobal(input.name, getType(input.type));

                    // Emit code

                    auto type = std::get<ShaderVarType>(input.type);

                    std::format_to(code, "layout(location = {}) in", inputLocation);
                    if (input.flags >= ShaderInputFlags::Flat) {
                        std::format_to(code, " flat");
                    }
                    if (input.flags >= ShaderInputFlags::PerVertex) {
                        std::format_to(code, " pervertexEXT");
                    }
                    std::format_to(code, " {} {}", shaderVarTypeToString(type), input.name);
                    if (input.flags >= ShaderInputFlags::PerVertex) {
                        std::format_to(code, "[3]"); // TODO: Primitive vertex count?
                    }
                    std::format_to(code, ";\n");

                    inputLocation += getTypeLocationWidth(type);
                },
                [&](const shader::Output& output) {
                    backend.RegisterGlobal(output.name, getType(output.type));

                    // Emit code

                    auto type = std::get<ShaderVarType>(output.type);

                    std::format_to(code, "layout(location = {}) out {} {};\n",
                        outputLocation, shaderVarTypeToString(type), output.name);

                    outputLocation += getTypeLocationWidth(type);
                },
                [&](const shader::Fragment& fragment) {
                    fragments.append(fragment.glsl);
                    fragments.push_back('\n');
                },
                [&](const auto&) {
                    NOVA_THROW("Unknown element - {}", element.index());
                }
            }, element);
        }

        scanner.source = fragments;
        scanner.ScanTokens();
        parser.Parse();
        backend.Resolve();
        backend.PrintAst();
        {
            std::ostringstream oss;
            backend.Generate(oss);
            codeStr.push_back('\n');
            codeStr.append(oss.str());
        }

        NOVA_LOG("Generated shader:\n{}", codeStr);

        Shader shader{ impl };
        CompileShader(shader, "generated", codeStr);
        return shader;
    }

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------

    Shader Shader::Create(HContext context, ShaderStage stage, Span<ShaderElement> elements)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->stage = stage;

        auto codeStr = ShaderPreamble(context, stage);
        auto code = std::back_insert_iterator(codeStr);

        // Transform GLSL

        thread_local std::string TransformOutput;
        auto transformGlsl = [](const std::string& glsl) -> const std::string& {

            // Convert "templated" descriptor heap accesses
            static std::regex DescriptorFind{ R"((\w+)<(\w+)>)" };
            TransformOutput.clear();
            std::regex_replace(std::back_insert_iterator(TransformOutput), glsl.begin(), glsl.end(),
                DescriptorFind, "$1_$2");

            return TransformOutput;
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
                    u32 align = 1;
                    for (auto& member : structure.members) {
                        align = std::max(align, GetShaderVarTypeAlign(std::get<ShaderVarType>(member.type)));
                    }

                    // Structure definition

                    std::format_to(code, "struct {} {{\n", structure.name);
                    for (auto& member : structure.members) {
                        std::format_to(code, "    {} {}{};\n",
                            typeToString(std::get<ShaderVarType>(member.type)), member.name, getArrayPart(member.count));
                    }
                    std::format_to(code, "}};\n");

                    // Structure Buffer reference

                    std::format_to(code, "layout(buffer_reference, scalar, buffer_reference_align = {0}) buffer BufferReference_{1}  {{ {1} get; }};\n", align, structure.name);

                    // Uniform Buffer

                    std::format_to(code, "layout(set = 0, binding = 0, scalar) uniform {1} {{ {0} data[]; }} UniformBuffer_{0}_[];\n", structure.name, getAnonStructureName());
                    std::format_to(code, "#define UniformBuffer_{0}(id) UniformBuffer_{0}_[id].data\n", structure.name);

                    // Storage Buffer

                    std::format_to(code, "layout(set = 0, binding = 0, scalar) buffer {1} {{ {0} data[]; }} StorageBuffer_{0}_[];\n", structure.name, getAnonStructureName());
                    std::format_to(code, "#define StorageBuffer_{0}(id) StorageBuffer_{0}_[id].data\n", structure.name);
                },
// -----------------------------------------------------------------------------
//                              Push Constants
// -----------------------------------------------------------------------------
                [&](const shader::PushConstants& pushConstants) {
                    std::format_to(code, "layout(push_constant, scalar) uniform {} {{\n", getAnonStructureName());
                    for (auto& member : pushConstants.members) {
                        NOVA_LOGEXPR(member.type.index());
                        std::format_to(code, "    {} {}{};\n",
                            typeToString(std::get<ShaderVarType>(member.type)), member.name, getArrayPart(member.count));
                    }
                    std::format_to(code, "}} {};\n", pushConstants.name);
                },
// -----------------------------------------------------------------------------
//                            Buffer Reference
// -----------------------------------------------------------------------------
                [&](const shader::BufferReference& bufferReference) {
                    u32 align = 1;
                    for (auto& member : bufferReference.members) {
                        align = std::max(align, GetShaderVarTypeAlign(std::get<ShaderVarType>(member.type)));
                    }

                    std::format_to(code, "layout(buffer_reference, scalar, buffer_reference_align = {}) buffer {} {{\n",
                        align, bufferReference.name);
                    for (auto& member : bufferReference.members) {
                        std::format_to(code, "    {} {}{};\n",
                            typeToString(std::get<ShaderVarType>(member.type)), member.name, getArrayPart(member.count));
                    }
                    std::format_to(code, "}};\n");
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
                    std::format_to(code, " {} {}", typeToString(std::get<ShaderVarType>(input.type)), input.name);
                    if (input.flags >= ShaderInputFlags::PerVertex) {
                        std::format_to(code, "[3]"); // TODO: Primitive vertex count?
                    }
                    std::format_to(code, ";\n");

                    inputLocation += getTypeLocationWidth(std::get<ShaderVarType>(input.type));
                },
// -----------------------------------------------------------------------------
//                          Shader Output Variable
// -----------------------------------------------------------------------------
                [&](const shader::Output& output) {
                    std::format_to(code, "layout(location = {}) out {} {};\n",
                        outputLocation, typeToString(std::get<ShaderVarType>(output.type)), output.name);

                    outputLocation += getTypeLocationWidth(std::get<ShaderVarType>(output.type));
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

        if (impl->shader) {
            vkDestroyShaderEXT(impl->context->device, impl->shader, impl->context->pAlloc);
        }

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