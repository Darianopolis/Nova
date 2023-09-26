
//     enum class ShaderVarType
//     {
//         Mat2, Mat3, Mat4,
//         Mat4x3, Mat3x4,
//         Vec2, Vec3, Vec4,
//         Vec2U, Vec3U, Vec4U,
//         U32, U64,
//         I16, I32, I64,
//         F32, F64,
//     };

//     inline constexpr
//     u32 GetShaderVarTypeSize(ShaderVarType type)
//     {
//         switch (type) {
//         break;case ShaderVarType::Mat2: return  4 * 4;
//         break;case ShaderVarType::Mat3: return  9 * 4;
//         break;case ShaderVarType::Mat4: return 16 * 4;

//         break;case ShaderVarType::Mat4x3: return 12 * 4;
//         break;case ShaderVarType::Mat3x4: return 12 * 4;

//         break;case ShaderVarType::Vec2: return  2 * 4;
//         break;case ShaderVarType::Vec3: return  3 * 4;
//         break;case ShaderVarType::Vec4: return  4 * 4;

//         break;case ShaderVarType::I16: return 2;

//         break;case ShaderVarType::U32:
//               case ShaderVarType::I32:
//               case ShaderVarType::F32: return 4;

//         break;case ShaderVarType::U64:
//               case ShaderVarType::I64:
//               case ShaderVarType::F64: return 8;
//         }
//         return 0;
//     }

//     inline constexpr
//     u32 GetShaderVarTypeAlign(ShaderVarType type)
//     {
//         switch (type) {
//         break;case ShaderVarType::U64: return 8;
//         break;case ShaderVarType::I16: return 2;
//         }
//         return 4;
//     }

// // -----------------------------------------------------------------------------

//     struct UniformBufferType
//     {
//         std::string_view element;
//         bool            readonly;
//     };

//     struct StorageBufferType
//     {
//         std::string_view element;
//         bool            readonly;
//     };

//     struct BufferReferenceType
//     {
//         std::string_view element;
//         bool            readonly;
//     };

//     struct SampledImageType
//     {
//         i32 dims;
//     };

//     struct StorageImageType
//     {
//         Format format;
//         i32      dims;
//     };

//     using ShaderType = std::variant<
//         ShaderVarType,
//         UniformBufferType,
//         StorageBufferType,
//         BufferReferenceType,
//         SampledImageType,
//         StorageImageType>;

//     struct Member
//     {
//         std::string_view    name;
//         ShaderType          type;
//         std::optional<u32> count = std::nullopt;
//     };

// // -----------------------------------------------------------------------------

//     enum class ShaderInputFlags
//     {
//         None,
//         Flat,
//         PerVertex,
//     };
//     NOVA_DECORATE_FLAG_ENUM(ShaderInputFlags)

//     namespace shader
//     {
//         constexpr u32 ArrayCountUnsized = UINT32_MAX;
//     }

//     namespace shader
//     {
//         struct Structure
//         {
//             std::string     name;
//             Span<Member> members;
//         };

//         struct PushConstants
//         {
//             std::string     name;
//             Span<Member> members;
//         };

//         struct BufferReference
//         {
//             std::string name;
//             Span<Member> members;
//         };

//         struct Input
//         {
//             std::string       name;
//             ShaderType        type;
//             ShaderInputFlags flags = {};
//         };

//         struct Output
//         {
//             std::string   name;
//             ShaderType    type;
//         };

//         struct Fragment
//         {
//             std::string glsl;
//         };

//         struct ComputeKernel
//         {
//             Vec3U workGroups;
//             std::string glsl;
//         };

//         struct Kernel
//         {
//             std::string glsl;
//         };
//     }

//     using ShaderElement = std::variant<
//         shader::Structure,
//         shader::PushConstants,
//         shader::BufferReference,
//         shader::Input,
//         shader::Output,
//         shader::Fragment,
//         shader::ComputeKernel,
//         shader::Kernel>;































// // -----------------------------------------------------------------------------
// //
// // -----------------------------------------------------------------------------

//     static
//     std::string ShaderPreamble(Context context, ShaderStage stage, bool allImageFormats)
//     {
//         std::string codeStr = "#version 460\n";
//         auto code = std::back_insert_iterator(codeStr);

//         constexpr auto Extensions = std::to_array<std::pair<const char*, VkShaderStageFlags>>({
//             // std::pair("GL_GOOGLE_include_directive", VK_SHADER_STAGE_ALL),

//             std::pair("GL_EXT_scalar_block_layout", VK_SHADER_STAGE_ALL),
//             std::pair("GL_EXT_buffer_reference2", VK_SHADER_STAGE_ALL),
//             std::pair("GL_EXT_nonuniform_qualifier", VK_SHADER_STAGE_ALL),

//             // std::pair("GL_EXT_shader_explicit_arithmetic_types_int8", VK_SHADER_STAGE_ALL),
//             // std::pair("GL_EXT_shader_explicit_arithmetic_types_int16", VK_SHADER_STAGE_ALL),
//             // std::pair("GL_EXT_shader_explicit_arithmetic_types_int32", VK_SHADER_STAGE_ALL),
//             std::pair("GL_EXT_shader_explicit_arithmetic_types_int64", VK_SHADER_STAGE_ALL),
//             // std::pair("GL_EXT_shader_explicit_arithmetic_types_float16", VK_SHADER_STAGE_ALL),
//             // std::pair("GL_EXT_shader_explicit_arithmetic_types_float32", VK_SHADER_STAGE_ALL),
//             // std::pair("GL_EXT_shader_explicit_arithmetic_types_float64", VK_SHADER_STAGE_ALL),

//             std::pair("GL_EXT_fragment_shader_barycentric", VK_SHADER_STAGE_FRAGMENT_BIT),

//             std::pair("GL_EXT_mesh_shader", VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT),

//             std::pair("GL_EXT_ray_tracing", VK_SHADER_STAGE_ALL),
//             std::pair("GL_EXT_ray_query", VK_SHADER_STAGE_ALL),
//             std::pair("GL_EXT_ray_tracing_position_fetch", VK_SHADER_STAGE_ALL),
//             std::pair("GL_NV_shader_invocation_reorder", VK_SHADER_STAGE_ALL),
//         });

//         {
//             auto vkStage = GetVulkanShaderStage(stage);
//             for (auto& extension : Extensions) {
//                 if (vkStage & extension.second) {
//                     std::format_to(code, "#extension {} : enable\n", extension.first);
//                 }
//             }
//         }

//         constexpr auto Dimensions = std::array {
//             "1D",
//             "2D",
//             "3D",

//             "1DArray",
//             "2DArray",

//             "Cube",
//             "CubeArray",
//         };

//         constexpr auto ImageFormats = std::array {

//             // floating-point formats
//             std::pair("rgba32f",        ""),
//             std::pair("rgba16f",        ""),
//             std::pair("rg32f",          ""),
//             std::pair("rg16f",          ""),
//             std::pair("r11f_g11f_b10f", ""),
//             std::pair("r32f",           ""),
//             std::pair("r16f",           ""),

//             // unsigned-normalized formats
//             std::pair("rgba16",         ""),
//             std::pair("rgb10_a2",       ""),
//             std::pair("rgba8",          ""),
//             std::pair("rg16",           ""),
//             std::pair("rg8",            ""),
//             std::pair("r16",            ""),
//             std::pair("r8",             ""),

//             // signed-normalized formats
//             std::pair("rgba16_snorm",   ""),
//             std::pair("rgba8_snorm",    ""),
//             std::pair("rg16_snorm",     ""),
//             std::pair("rg8_snorm",      ""),
//             std::pair("r16_snorm",      ""),
//             std::pair("r8_snorm",       ""),

//             // signed-integer formats
//             std::pair("rgba32i", "i"),
//             std::pair("rgba16i", "i"),
//             std::pair("rgba8i",  "i"),
//             std::pair("rg32i",   "i"),
//             std::pair("rg16i",   "i"),
//             std::pair("rg8i",    "i"),
//             std::pair("r32i",    "i"),
//             std::pair("r16i",    "i"),
//             std::pair("r8i",     "i"),

//             // unsigned-integer formats
//             std::pair("rgba32ui",   "u"),
//             std::pair("rgba16ui",   "u"),
//             std::pair("rgb10_a2ui", "u"),
//             std::pair("rgba8ui",    "u"),
//             std::pair("rg32ui",     "u"),
//             std::pair("rg16ui",     "u"),
//             std::pair("rg8ui",      "u"),
//             std::pair("r32ui",      "u"),
//             std::pair("r16ui",      "u"),
//             std::pair("r8ui",       "u"),
//         };

//         constexpr std::array UniformTexelFormats {
//             std::pair("", "float"),
//             std::pair("i",  "int"),
//             std::pair("u", "uint"),
//         };

//         if (allImageFormats) {
//             for (auto format : ImageFormats) {
//                 for (auto dims : Dimensions) {
//                     std::format_to(code, "layout(set = 0, binding = 0, {}) uniform {}image{} StorageImage{}_{}[];\n",
//                         format.first, format.second, dims, dims, format.first);
//                 }

//                 std::format_to(code, "layout(set = 0, binding = 0, {}) uniform {}imageBuffer StorageTexelBuffer_{}[];\n",
//                     format.first, format.second, format.first);
//             }

//             for (auto dims : Dimensions) {
//                 std::format_to(code, "layout(set = 0, binding = 0) uniform texture{0} SampledImage{0}[];\n", dims);
//             }

//             for (auto type : UniformTexelFormats) {
//                 std::format_to(code, "layout(set = 0, binding = 0) uniform {}textureBuffer UniformTexelBuffer_{}[];\n",
//                     type.first, type.second);
//             }
//         }

//         // Samplers

//         std::format_to(code, "layout(set = 0, binding = 0) uniform sampler Sampler[];\n");

//         if (allImageFormats) {
//             for (auto dims : Dimensions) {
//                 std::format_to(code, "#define Sampler{0}(texture, sampler) sampler{0}(SampledImage{0}[texture], Sampler[sampler])\n", dims);
//             }
//         }

//         // Acceleration structure

//         if (context->config.rayTracing) {
//             std::format_to(code, "layout(set = 1, binding = 0) uniform accelerationStructureEXT AccelerationStructure;\n");
//         }

//         return codeStr;
//     }

//     static
//     std::string_view ShaderVarTypeToString(ShaderVarType type)
//     {
//         switch (type) {
//         break;case ShaderVarType::Mat2: return "mat2";
//         break;case ShaderVarType::Mat3: return "mat3";
//         break;case ShaderVarType::Mat4: return "mat4";

//         break;case ShaderVarType::Mat4x3: return "mat4x3";
//         break;case ShaderVarType::Mat3x4: return "mat3x4";

//         break;case ShaderVarType::Vec2: return "vec2";
//         break;case ShaderVarType::Vec3: return "vec3";
//         break;case ShaderVarType::Vec4: return "vec4";

//         break;case ShaderVarType::Vec2U: return "uvec2";
//         break;case ShaderVarType::Vec3U: return "uvec3";
//         break;case ShaderVarType::Vec4U: return "uvec4";

//         break;case ShaderVarType::U32: return "uint";
//         break;case ShaderVarType::U64: return "uint64_t";

//         break;case ShaderVarType::I16: return "int16_t";
//         break;case ShaderVarType::I32: return "int";
//         break;case ShaderVarType::I64: return "int64_t";

//         break;case ShaderVarType::F32: return "float";
//         break;case ShaderVarType::F64: return "double";
//         }

//         std::unreachable();
//     };

//     static
//     std::string_view FormatToGlsl(Format format)
//     {
//         switch (format) {
//         break;case Format::RGBA8_UNorm:   return "rgba8";
//         break;case Format::RGBA8_SRGB:    return "rgba8";
//         break;case Format::RGBA16_SFloat: return "rgba16f";
//         break;case Format::RGBA32_SFloat: return "rgba32f";
//         break;case Format::BGRA8_UNorm:   return "rgba8";
//         break;case Format::BGRA8_SRGB:    return "rgba8";
//         break;case Format::RGB32_SFloat:  return "rgb32f";
//         break;case Format::R8_UNorm:      return "r8";
//         break;case Format::R32_SFloat:    return "r32f";
//         break;case Format::R8_UInt:       return "r8ui";
//         break;case Format::R16_UInt:      return "r16ui";
//         break;case Format::R32_UInt:      return "r32ui";
//         break;case Format::D24_UNorm:     ;
//         break;case Format::S8_D24_UNorm:  ;
//         break;case Format::D32_SFloat:    ;
//         }

//         NOVA_THROW("Unknown Format: {}", u32(format));
//     }

//     static
//     std::string_view FormatToGlslPrefix(Format format)
//     {
//         switch (format) {
//         break;case Format::RGBA8_UNorm:   return "";
//         break;case Format::RGBA8_SRGB:    return "";
//         break;case Format::RGBA16_SFloat: return "";
//         break;case Format::RGBA32_SFloat: return "";
//         break;case Format::BGRA8_UNorm:   return "";
//         break;case Format::BGRA8_SRGB:    return "";
//         break;case Format::RGB32_SFloat:  return "";
//         break;case Format::R8_UNorm:      return "";
//         break;case Format::R32_SFloat:    return "";
//         break;case Format::R8_UInt:       return "u";
//         break;case Format::R16_UInt:      return "u";
//         break;case Format::R32_UInt:      return "u";
//         break;case Format::D24_UNorm:     ;
//         break;case Format::S8_D24_UNorm:  ;
//         break;case Format::D32_SFloat:    ;
//         }

//         NOVA_THROW("Unknown Format: {}", u32(format));
//     }

// // -----------------------------------------------------------------------------
// //
// // -----------------------------------------------------------------------------

//     Shader Shader::Create2(HContext context, ShaderStage stage, Span<ShaderElement> elements)
//     {
//         auto impl = new Impl;
//         impl->context = context;
//         impl->stage = stage;

//         std::string codeStr = ShaderPreamble(context, stage, false);
//         auto code = std::back_insert_iterator(codeStr);

//         nova::Compiler compiler;

//         nova::Scanner scanner;
//         scanner.compiler = &compiler;

//         nova::Parser parser;
//         parser.scanner = &scanner;

//         nova::VulkanGlslBackend backend;
//         backend.parser = &parser;

//         for (auto& type : {
//              "vec2",  "vec3",  "vec4",
//             "ivec2", "ivec3", "ivec4",
//             "uvec2", "uvec3", "uvec4",

//             "uint16_t", "int16_t",
//             "uint",     "int",
//             "uint64_t", "int64_t",

//             "mat2", "mat3", "mat4",
//             "mat4x3", "mat3x4",

//             "float", "double",

//             "bool",
//         }) {
//             backend.FindType(type);
//         }

//         backend.RegisterGlobal("gl_Position",           backend.FindType("vec4"));
//         backend.RegisterGlobal("gl_FragCoord",          backend.FindType("vec4"));
//         backend.RegisterGlobal("gl_VertexIndex",        backend.FindType("uint"));
//         backend.RegisterGlobal("gl_InstanceIndex",      backend.FindType("uint"));
//         backend.RegisterGlobal("gl_GlobalInvocationID", backend.FindType("uvec2"));
//         backend.RegisterGlobal("gl_NumWorkGroups",      backend.FindType("uvec2"));
//         backend.RegisterGlobal("gl_WorkGroupSize",      backend.FindType("uvec2"));

//         ankerl::unordered_dense::set<std::string> declared;

//         auto isDeclaredType = [&](std::string_view type) {
//             // TODO: Cover all types
//             return type == "uint";
//         };

//         auto getType = [&](const ShaderType& shaderType) {
//             Type* type = nullptr;

//             std::visit(Overloads {
//                 [&](const BufferReferenceType& br) {
//                     auto accessor = backend.RegisterBufferAccessor(
//                         backend.FindType(br.element),
//                         VulkanGlslBackend::AccessorMode::BufferReference,
//                         br.readonly);
//                     if (isDeclaredType(br.element) && !declared.contains(accessor->name)) {
//                         declared.emplace(accessor->name);
//                         std::format_to(code, "layout(buffer_reference, scalar, buffer_reference_align = 4) buffer {0}{1}_buffer_reference {{ {0} get; }};\n",
//                             br.element, br.readonly ? "_readonly" : "");
//                     }
//                     type = accessor->accessorType;
//                 },
//                 [&](const UniformBufferType& ub) {
//                     NOVA_LOG("  IS UNIFORM BUFFER");
//                     auto accessor = backend.RegisterBufferAccessor(
//                         backend.FindType(ub.element),
//                         VulkanGlslBackend::AccessorMode::UniformBuffer,
//                         ub.readonly);
//                     type = accessor->accessorType;
//                 },
//                 [&](const StorageBufferType& sb) {
//                     NOVA_LOG("  IS STORAGE BUFFER");
//                     auto accessor = backend.RegisterBufferAccessor(
//                         backend.FindType(sb.element),
//                         VulkanGlslBackend::AccessorMode::StorageBuffer,
//                         sb.readonly);
//                     type = accessor->accessorType;
//                 },
//                 [&](const SampledImageType& t) {
//                     NOVA_LOG("  IS SAMPLED IMAGE");
//                     // TODO: Support Array/Cube texture dims
//                     auto& declaration = NOVA_FORMAT_TEMP("SampledImage{}D", t.dims);
//                     if (!declared.contains(declaration)) {
//                         declared.emplace(declaration);
//                         std::format_to(code, "layout(set = 0, binding = 0) uniform texture{0}D SampledImage{0}D[];\n", t.dims);
//                     }
//                     auto accessor = backend.RegisterImageAccessor("", t.dims,
//                         VulkanGlslBackend::AccessorMode::SampledImage);
//                     type = accessor->accessorType;
//                 },
//                 [&](const StorageImageType& t) {
//                     NOVA_LOG("  IS STORAGE IMAGE");
//                     auto formatStr = FormatToGlsl(t.format);
//                     // TODO: Support Array/Cube texture dims
//                     auto& declaration = NOVA_FORMAT_TEMP("StorageImage{}D_{}", t.dims, formatStr);
//                     if (!declared.contains(declaration)) {
//                         declared.emplace(declaration);
//                         std::format_to(code, "layout(set = 0, binding = 0, {0}) uniform {1}image{2}D StorageImage{2}D_{0}[];\n",
//                             formatStr, FormatToGlslPrefix(t.format), t.dims);
//                     }
//                     auto accessor = backend.RegisterImageAccessor(FormatToGlsl(t.format), t.dims,
//                         VulkanGlslBackend::AccessorMode::StorageImage);
//                     type = accessor->accessorType;
//                 },
//                 [&](const ShaderVarType& varType) {
//                     type = backend.FindType(ShaderVarTypeToString(varType));
//                 },
//                 [&](const auto&) {
//                     NOVA_THROW("Unknown type");
//                 }
//             }, shaderType);

//             NOVA_LOG("FOUND TYPE: {}", type->name);

//             return type;
//         };
//         auto getTypeString = [&](const ShaderType& shaderType) {
//             auto type = getType(shaderType);
//             if (std::holds_alternative<StorageBufferType>(shaderType)
//                     || std::holds_alternative<UniformBufferType>(shaderType)
//                     || std::holds_alternative<SampledImageType>(shaderType)
//                     || std::holds_alternative<StorageImageType>(shaderType)) {
//                 return "uvec2"sv;
//             }

//             return type->name;
//         };

//         u32 structureId = 0;
//         auto getAnonStructureName = [&] {
//             return std::format("_{}_", ++structureId);
//         };

//         u32 inputLocation = 0;
//         u32 outputLocation = 0;
//         auto getTypeLocationWidth = [](ShaderVarType type) {

//             switch (type)  {
//             break;case ShaderVarType::Mat2: return 2;
//             break;case ShaderVarType::Mat3: return 3;
//             break;case ShaderVarType::Mat4: return 4;
//             break;default:                  return 1;
//             }
//         };

//         // Structures

//         std::string fragments;

//         auto getShaderTypeAlign = [&](ShaderType type) -> u32 {
//             if (std::holds_alternative<BufferReferenceType>(type)) {
//                 return 8;
//             } else if (auto pShaderVarType = std::get_if<ShaderVarType>(&type); pShaderVarType) {
//                 return GetShaderVarTypeAlign(*pShaderVarType);
//             } else {
//                 // StorageBuffer / UniformBuffer / SampledImage / StorageImage
//                 return 4;
//             }
//         };

//         for (auto& element : elements) {
//             std::visit(Overloads {
//                 [&](const shader::Structure& structure) {
//                     u32 align = 1;
//                     for (auto& member : structure.members) {
//                         align = std::max(align, getShaderTypeAlign(member.type));
//                     }

//                     auto s = backend.FindType(structure.name);
//                     s->structure = new Struct;
//                     for (auto& member : structure.members) {
//                         s->structure->members.insert({ member.name, getType(member.type) });
//                     }

//                     // Emit code

//                     std::format_to(code, "struct {} {{\n", structure.name);
//                     for (auto& member : structure.members) {
//                         std::format_to(code, "    {} {};\n",
//                             getTypeString(member.type), member.name);
//                     }
//                     std::format_to(code, "}};\n");

//                     // Buffer reference

//                     std::format_to(code, "layout(buffer_reference, scalar, buffer_reference_align = {1}) "
//                         "buffer {0}_buffer_reference {{ {0} get; }};\n", structure.name, align);
//                     std::format_to(code, "layout(buffer_reference, scalar, buffer_reference_align = {1}) "
//                         "readonly buffer {0}_readonly_buffer_reference {{ {0} get; }};\n", structure.name, align);

//                     // Uniform buffer

//                     std::format_to(code, "layout(set = 0, binding = 0, scalar) uniform {1} {{ {0} data[1024]; }} "
//                         "{0}_uniform_buffer[];\n", structure.name, getAnonStructureName());
//                     std::format_to(code, "layout(set = 0, binding = 0, scalar) readonly uniform {1} {{ {0} data[1024]; }} "
//                         "{0}_readonly_uniform_buffer[];\n", structure.name, getAnonStructureName());

//                     // Storage buffer

//                     std::format_to(code, "layout(set = 0, binding = 0, scalar) buffer {1} {{ {0} data[]; }} "
//                         "{0}_storage_buffer[];\n", structure.name, getAnonStructureName());
//                     std::format_to(code, "layout(set = 0, binding = 0, scalar) readonly buffer {1} {{ {0} data[]; }} "
//                         "{0}_readonly_storage_buffer[];\n", structure.name, getAnonStructureName());
//                 },
//                 [&](const shader::PushConstants& pushConstants) {
//                     auto anonName = getAnonStructureName();
//                     auto s = backend.FindType(anonName);
//                     s->structure = new Struct;
//                     for (auto& member : pushConstants.members) {
//                         s->structure->members.insert({ member.name, getType(member.type) });
//                     }
//                     backend.RegisterGlobal(pushConstants.name, s);

//                     // Emit code

//                     std::format_to(code, "layout(push_constant, scalar) uniform {} {{\n", getAnonStructureName());
//                     for (auto& member : pushConstants.members) {
//                         std::format_to(code, "    {} {};\n", getTypeString(member.type), member.name);
//                     }
//                     std::format_to(code, "}} {};\n", pushConstants.name);
//                 },
//                 [&](const shader::Input& input) {
//                     backend.RegisterGlobal(input.name, getType(input.type));

//                     // Emit code

//                     auto type = std::get<ShaderVarType>(input.type);

//                     std::format_to(code, "layout(location = {}) in", inputLocation);
//                     if (input.flags >= ShaderInputFlags::Flat) {
//                         std::format_to(code, " flat");
//                     }
//                     if (input.flags >= ShaderInputFlags::PerVertex) {
//                         std::format_to(code, " pervertexEXT");
//                     }
//                     std::format_to(code, " {} {}", ShaderVarTypeToString(type), input.name);
//                     if (input.flags >= ShaderInputFlags::PerVertex) {
//                         std::format_to(code, "[3]"); // TODO: Primitive vertex count?
//                     }
//                     std::format_to(code, ";\n");

//                     inputLocation += getTypeLocationWidth(type);
//                 },
//                 [&](const shader::Output& output) {
//                     backend.RegisterGlobal(output.name, getType(output.type));

//                     // Emit code

//                     auto type = std::get<ShaderVarType>(output.type);

//                     std::format_to(code, "layout(location = {}) out {} {};\n",
//                         outputLocation, ShaderVarTypeToString(type), output.name);

//                     outputLocation += getTypeLocationWidth(type);
//                 },
//                 [&](const shader::Fragment& fragment) {
//                     fragments.append(fragment.glsl);
//                     fragments.push_back('\n');
//                 },
//                 [&](const shader::ComputeKernel& computeKernel) {
//                     std::format_to(code, "layout(local_size_x = {}, local_size_y = {}, local_size_z = {}) in;\n",
//                         computeKernel.workGroups.x, computeKernel.workGroups.y, computeKernel.workGroups.z);
//                 },
//                 [&](const auto&) {
//                     NOVA_THROW("Unknown element - {}", element.index());
//                 }
//             }, element);
//         }

//         scanner.source = fragments;
//         scanner.ScanTokens();
//         parser.Parse();
//         backend.Resolve();
//         backend.PrintAst();
//         {
//             std::ostringstream oss;
//             backend.Generate(oss);
//             codeStr.push_back('\n');
//             codeStr.append(oss.str());
//         }

//         NOVA_LOG("Generated shader:\n{}", codeStr);

//         Shader shader{ impl };
//         CompileShader(shader, "generated", codeStr);
//         return shader;
//     }

// // -----------------------------------------------------------------------------
// //
// // -----------------------------------------------------------------------------

//     Shader Shader::Create(HContext context, ShaderStage stage, Span<ShaderElement> elements)
//     {
//         auto impl = new Impl;
//         impl->context = context;
//         impl->stage = stage;

//         auto codeStr = ShaderPreamble(context, stage, true);
//         auto code = std::back_insert_iterator(codeStr);

//         // Transform GLSL

//         thread_local std::string TransformOutput;
//         auto transformGlsl = [](const std::string& glsl) -> const std::string& {

//             // Convert "templated" descriptor heap accesses
//             static std::regex DescriptorFind{ R"((\w+)<(\w+)>)" };
//             TransformOutput.clear();
//             std::regex_replace(std::back_insert_iterator(TransformOutput), glsl.begin(), glsl.end(),
//                 DescriptorFind, "$1_$2");

//             return TransformOutput;
//         };

//         auto getArrayPart = [](std::optional<u32> count) {
//             return count
//                 ? count.value() == shader::ArrayCountUnsized
//                     ? "[]"
//                     : NOVA_FORMAT_TEMP("[{}]", count.value()).c_str()
//                 : "";
//         };

//         u32 structureId = 0;
//         auto getAnonStructureName = [&] {
//             return std::format("_{}_", ++structureId);
//         };

//         u32 inputLocation = 0;
//         u32 outputLocation = 0;
//         auto getTypeLocationWidth = [](ShaderVarType type) {

//             switch (type)  {
//             break;case ShaderVarType::Mat2: return 2;
//             break;case ShaderVarType::Mat3: return 3;
//             break;case ShaderVarType::Mat4: return 4;
//             break;default:                  return 1;
//             }
//         };

//         for (auto& element : elements) {
//             std::visit(Overloads {
// // -----------------------------------------------------------------------------
// //                          Structure declaration
// // -----------------------------------------------------------------------------
//                 [&](const shader::Structure& structure) {
//                     u32 align = 1;
//                     for (auto& member : structure.members) {
//                         align = std::max(align, GetShaderVarTypeAlign(std::get<ShaderVarType>(member.type)));
//                     }

//                     // Structure definition

//                     std::format_to(code, "struct {} {{\n", structure.name);
//                     for (auto& member : structure.members) {
//                         std::format_to(code, "    {} {}{};\n",
//                             ShaderVarTypeToString(std::get<ShaderVarType>(member.type)), member.name, getArrayPart(member.count));
//                     }
//                     std::format_to(code, "}};\n");

//                     // Structure Buffer reference

//                     std::format_to(code, "layout(buffer_reference, scalar, buffer_reference_align = {0}) buffer BufferReference_{1}  {{ {1} get; }};\n", align, structure.name);

//                     // Uniform Buffer

//                     std::format_to(code, "layout(set = 0, binding = 0, scalar) uniform {1} {{ {0} data[]; }} UniformBuffer_{0}_[];\n", structure.name, getAnonStructureName());
//                     std::format_to(code, "#define UniformBuffer_{0}(id) UniformBuffer_{0}_[id].data\n", structure.name);

//                     // Storage Buffer

//                     std::format_to(code, "layout(set = 0, binding = 0, scalar) buffer {1} {{ {0} data[]; }} StorageBuffer_{0}_[];\n", structure.name, getAnonStructureName());
//                     std::format_to(code, "#define StorageBuffer_{0}(id) StorageBuffer_{0}_[id].data\n", structure.name);
//                 },
// // -----------------------------------------------------------------------------
// //                              Push Constants
// // -----------------------------------------------------------------------------
//                 [&](const shader::PushConstants& pushConstants) {
//                     std::format_to(code, "layout(push_constant, scalar) uniform {} {{\n", getAnonStructureName());
//                     for (auto& member : pushConstants.members) {
//                         NOVA_LOGEXPR(member.type.index());
//                         std::format_to(code, "    {} {}{};\n",
//                             ShaderVarTypeToString(std::get<ShaderVarType>(member.type)), member.name, getArrayPart(member.count));
//                     }
//                     std::format_to(code, "}} {};\n", pushConstants.name);
//                 },
// // -----------------------------------------------------------------------------
// //                            Buffer Reference
// // -----------------------------------------------------------------------------
//                 [&](const shader::BufferReference& bufferReference) {
//                     u32 align = 1;
//                     for (auto& member : bufferReference.members) {
//                         align = std::max(align, GetShaderVarTypeAlign(std::get<ShaderVarType>(member.type)));
//                     }

//                     std::format_to(code, "layout(buffer_reference, scalar, buffer_reference_align = {}) buffer {} {{\n",
//                         align, bufferReference.name);
//                     for (auto& member : bufferReference.members) {
//                         std::format_to(code, "    {} {}{};\n",
//                             ShaderVarTypeToString(std::get<ShaderVarType>(member.type)), member.name, getArrayPart(member.count));
//                     }
//                     std::format_to(code, "}};\n");
//                 },
// // -----------------------------------------------------------------------------
// //                          Shader Input Variable
// // -----------------------------------------------------------------------------
//                 [&](const shader::Input& input) {
//                     std::format_to(code, "layout(location = {}) in", inputLocation);
//                     if (input.flags >= ShaderInputFlags::Flat) {
//                         std::format_to(code, " flat");
//                     }
//                     if (input.flags >= ShaderInputFlags::PerVertex) {
//                         std::format_to(code, " pervertexEXT");
//                     }
//                     std::format_to(code, " {} {}", ShaderVarTypeToString(std::get<ShaderVarType>(input.type)), input.name);
//                     if (input.flags >= ShaderInputFlags::PerVertex) {
//                         std::format_to(code, "[3]"); // TODO: Primitive vertex count?
//                     }
//                     std::format_to(code, ";\n");

//                     inputLocation += getTypeLocationWidth(std::get<ShaderVarType>(input.type));
//                 },
// // -----------------------------------------------------------------------------
// //                          Shader Output Variable
// // -----------------------------------------------------------------------------
//                 [&](const shader::Output& output) {
//                     std::format_to(code, "layout(location = {}) out {} {};\n",
//                         outputLocation, ShaderVarTypeToString(std::get<ShaderVarType>(output.type)), output.name);

//                     outputLocation += getTypeLocationWidth(std::get<ShaderVarType>(output.type));
//                 },
// // -----------------------------------------------------------------------------
// //                              GLSL Fragment
// // -----------------------------------------------------------------------------
//                 [&](const shader::Fragment& fragment) {
//                     std::format_to(code, "{}", transformGlsl(fragment.glsl));
//                 },
// // -----------------------------------------------------------------------------
// //                             Compute Kernel
// // -----------------------------------------------------------------------------
//                 [&](const shader::ComputeKernel& computeKernel) {
//                     std::format_to(code, "layout(local_size_x = {}, local_size_y = {}, local_size_z = {}) in;\nvoid main() {{\n{}\n}}\n",
//                         computeKernel.workGroups.x, computeKernel.workGroups.y, computeKernel.workGroups.z,
//                         transformGlsl(computeKernel.glsl));
//                 },
// // -----------------------------------------------------------------------------
// //                                 Kernel
// // -----------------------------------------------------------------------------
//                 [&](const shader::Kernel& kernel) {
//                     std::format_to(code, "void main() {{\n{}\n}}\n", transformGlsl(kernel.glsl));
//                 },
//             }, element);
//         }

//         // NOVA_LOG("Generated shader:\n{}", codeStr);

//         Shader shader{ impl };
//         CompileShader(shader, "generated", codeStr);
//         return shader;
//     }