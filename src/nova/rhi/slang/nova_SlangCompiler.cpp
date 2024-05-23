#include "nova_SlangCompiler.hpp"

#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Files.hpp>

#include <nova/vfs/nova_VirtualFilesystem.hpp>

namespace nova
{
#define NOVA_SLANG_IUNKNOWN_IMPL \
    std::atomic_uint32_t m_refCount{1}; \
    ISlangUnknown* getInterface(SlangUUID const&) { return nullptr; } \
    SLANG_IUNKNOWN_QUERY_INTERFACE \
    SLANG_IUNKNOWN_ADD_REF \
    SLANG_IUNKNOWN_RELEASE

#define NOVA_SLANG_CASTABLE_IMPL \
    virtual SLANG_NO_THROW void* SLANG_MCALL castAs(const SlangUUID&) final override { return nullptr; }

    struct BasicSlangBlob : ISlangBlob
    {
        NOVA_SLANG_IUNKNOWN_IMPL

        std::vector<b8> data;

        BasicSlangBlob(std::vector<b8> data)
            : data(data)
        {}

        virtual ~BasicSlangBlob() = default;

        virtual SLANG_NO_THROW void const* SLANG_MCALL getBufferPointer() final override
        {
            return data.data();
        };

        virtual SLANG_NO_THROW size_t SLANG_MCALL getBufferSize() final override
        {
            return data.size();
        };

    };

    struct SlangVirtualFilesystem : ISlangFileSystem
    {
        NOVA_SLANG_IUNKNOWN_IMPL
        NOVA_SLANG_CASTABLE_IMPL

        std::unordered_set<std::string> loaded_paths;

        virtual SLANG_NO_THROW SlangResult SLANG_MCALL loadFile(
            char const*     path,
            ISlangBlob** outBlob) final override
        {
            try {
                auto data = vfs::LoadStringMaybe(path);
                if (data) {
                    loaded_paths.emplace(path);
                    *outBlob = new BasicSlangBlob{std::vector<b8>((const b8*)data->Data(), (const b8*)data->Data() + data->Size())};
                    return SLANG_OK;
                } else {
                    return SLANG_E_NOT_FOUND;
                }
            } catch (...)
            {
                nova::Log("Critical error loading file..");
                return SLANG_FAIL;
            }
        }

        virtual ~SlangVirtualFilesystem() = default;
    };

    // TODO: DXC Flags that need to copied over?
    // -spv-target-env=vulkan1.3
    // -HV 2021
    // -enable-16bit-types
    // -res-may-alias
    // -ffinite-math-only

    SlangCompiler::SlangCompiler()
    {
        if (SLANG_FAILED(slang::createGlobalSession(slang_global_session.writeRef()))) {
            NOVA_THROW("slang failed creating global session");
        }
    }

    SlangSession SlangCompiler::CreateSession(bool use_vfs, Span<StringView> search_dirs)
    {
        slang::TargetDesc target_desc {
            .format = SLANG_SPIRV,
            .profile = slang_global_session->findProfile("glsl460"),
            .flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY,
            .forceGLSLScalarBufferLayout = true,
        };

        std::vector<std::string> search_paths;

        if (search_dirs.empty()) {
            search_paths.push_back(".");
        } else {
            for (auto& search_dir : search_dirs) search_paths.emplace_back(search_dir);
        }

        std::vector<const char*> search_path_cstrs;
        for (auto& path : search_paths) search_path_cstrs.emplace_back(path.c_str());

        auto vfs = use_vfs ? std::make_unique<SlangVirtualFilesystem>() : nullptr;

        std::vector<slang::CompilerOptionEntry> compiler_options;

        slang::SessionDesc session_desc {
            .targets = &target_desc,
            .targetCount = 1,
            .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
            .searchPaths = search_path_cstrs.data(),
            .searchPathCount = SlangInt(search_path_cstrs.size()),
            .fileSystem = vfs.get(),
            .compilerOptionEntries = compiler_options.data(),
            .compilerOptionEntryCount = uint32_t(compiler_options.size()),
        };

        Slang::ComPtr<slang::ISession> session;
        if (SLANG_FAILED(slang_global_session->createSession(session_desc, session.writeRef()))) {
            NOVA_THROW("slang failed created local session");
        }

        return SlangSession {
            .vfs = std::move(vfs),
            .session = std::move(session),
        };
    }

    SlangSession::~SlangSession() = default;

    std::vector<std::string> SlangSession::GetLoadedPaths()
    {
        std::vector<std::string> loaded;
        loaded.reserve(vfs->loaded_paths.size());
        std::ranges::copy(vfs->loaded_paths, std::back_insert_iterator(loaded));
        return loaded;
    }

    SlangModule SlangSession::Load(StringView module_name, StringView src)
    {
        // Load module source

        if (src.Size() == 0) {
            src = vfs::LoadString(module_name);
        }

        slang::IModule* slang_module = {};
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            slang_module = session->loadModuleFromSourceString(module_name.CStr(), module_name.CStr(), src.CStr(), diagnostics_blob.writeRef());

            if (diagnostics_blob) {
                Log("{}", reinterpret_cast<const char*>(diagnostics_blob->getBufferPointer()));
            }

            if (!slang_module) {
                NOVA_THROW("Slang module could not be created");
            }
        }

        return SlangModule {
            .session = this,
            .slang_module = slang_module,
        };
    }

    std::vector<std::string> SlangModule::GetEntryPointNames()
    {
        // Find all entry points

        std::vector<Slang::ComPtr<slang::IEntryPoint>> entry_points;

        auto entry_point_count = u32(slang_module->getDefinedEntryPointCount());
        for (u32 i = 0; i < entry_point_count; ++i) {
            Slang::ComPtr<slang::IEntryPoint> entry_point;
            if (SLANG_FAILED(slang_module->getDefinedEntryPoint(i, entry_point.writeRef()))) {
                NOVA_THROW("slang failed to read defined entry point");
            }

            entry_points.push_back(std::move(entry_point));
        }

        // Create composed program including all entry points

        std::vector<slang::IComponentType*> component_types;
        component_types.push_back(slang_module);
        for (auto& entry_point : entry_points) component_types.push_back(entry_point);

        Slang::ComPtr<slang::IComponentType> composed_program;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            auto result = session->session->createCompositeComponentType(
                component_types.data(), component_types.size(),
                composed_program.writeRef(),
                diagnostics_blob.writeRef());

            if (diagnostics_blob) {
                Log("{}", (const char*)diagnostics_blob->getBufferPointer());
            }

            if (SLANG_FAILED(result)) {
                NOVA_THROW("Slang failed composing program");
            }
        }

        // Reflect on module entry points

        auto layout = composed_program->getLayout();
        std::vector<std::string> entry_point_names;
        {
            entry_point_count = u32(layout->getEntryPointCount());
            for (u32 i = 0; i < entry_point_count; ++i) {
                auto entry_point = layout->getEntryPointByIndex(i);
                entry_point_names.emplace_back(entry_point->getName());
            }
        }

        return entry_point_names;
    }

    std::vector<u32> SlangModule::GenerateCode(StringView entry_point_name)
    {

        Slang::ComPtr<slang::IEntryPoint> entry_point;
        if (SLANG_FAILED(slang_module->findEntryPointByName(entry_point_name.CStr(), entry_point.writeRef()))) {
            NOVA_THROW("slang failed to find entry point: {}", entry_point_name);
        }

        std::vector<slang::IComponentType*> component_types;
        component_types.push_back(slang_module);
        component_types.push_back(entry_point);

        Slang::ComPtr<slang::IComponentType> composed_program;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            auto result = session->session->createCompositeComponentType(
                component_types.data(),
                component_types.size(),
                composed_program.writeRef(),
                diagnostics_blob.writeRef());

            if (diagnostics_blob) {
                Log("{}", (const char*)diagnostics_blob->getBufferPointer());
            }

            if (SLANG_FAILED(result)) {
                NOVA_THROW("slang failed composing shader");
            }
        }

        Slang::ComPtr<slang::IBlob> spirv_code;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            auto result = composed_program->getEntryPointCode(0, 0, spirv_code.writeRef(), diagnostics_blob.writeRef());
            if (diagnostics_blob) {
                Log("{}", (const char*)diagnostics_blob->getBufferPointer());
            }
            if (SLANG_FAILED(result)) {
                NOVA_THROW("slang failed compiling shader");
            }
        }

        std::vector<u32> output(spirv_code->getBufferSize() / 4);
        std::memcpy(output.data(), spirv_code->getBufferPointer(), spirv_code->getBufferSize());

        return output;
    }
}