#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Files.hpp>

#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>
#include <nova/vfs/nova_VirtualFilesystem.hpp>

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>

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

    struct SlangVFS : ISlangFileSystem
    {
        NOVA_SLANG_IUNKNOWN_IMPL
        NOVA_SLANG_CASTABLE_IMPL

        virtual SLANG_NO_THROW SlangResult SLANG_MCALL loadFile(
            char const*     path,
            ISlangBlob** outBlob) final override
        {
            std::string path_str = path;

            try {
                auto data = vfs::LoadStringMaybe(path_str);
                if (data) {
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
    };

    struct SlangSession
    {
        Slang::ComPtr<slang::IGlobalSession> slang_global_session;
        Slang::ComPtr<slang::ISession>                    session;

        SlangSession()
        {
            CreateSlangSession();
        }

        void CreateSlangSession()
        {
            if (SLANG_FAILED(slang::createGlobalSession(slang_global_session.writeRef()))) {
                NOVA_THROW("slang failed creating global session");
            }

            slang::TargetDesc target_desc {
                .format = SLANG_SPIRV,
                .profile = slang_global_session->findProfile("glsl460"),
                .flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY,
                .forceGLSLScalarBufferLayout = true,
            };

            // TODO: DXC Flags that need to copied over?
            // -spv-target-env=vulkan1.3
            // -HV 2021
            // -enable-16bit-types
            // -res-may-alias
            // -ffinite-math-only

            // slang::CompilerOptionEntry slang_entry;
            // slang_entry.name = slang::CompilerOptionName::Capability;
            // slang_entry.value.kind = slang::CompilerOptionValueKind::String;
            // slang_entry.value.stringValue0 = "spirv_1_5";

            std::vector<std::string> search_paths;
            search_paths.push_back(".");

            std::vector<const char*> search_path_cstrs;
            for (auto& path : search_paths) search_path_cstrs.emplace_back(path.c_str());

            slang::SessionDesc session_desc {
                .targets = &target_desc,
                .targetCount = 1,
                .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
                .searchPaths = search_path_cstrs.data(),
                .searchPathCount = SlangInt(search_path_cstrs.size()),
                .fileSystem = new SlangVFS(),
            };

            if (SLANG_FAILED(slang_global_session->createSession(session_desc, session.writeRef()))) {
                NOVA_THROW("slang failed created local session");
            }
        }

        void LoadModule(StringView path)
        {
            nova::Log("Loading slang module [{}]...", path);
            auto path_cstr = path.CStr();

            auto source = vfs::LoadString(path);

            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            auto module = session->loadModuleFromSourceString(path_cstr, path_cstr,
                source.CStr(), diagnostics_blob.writeRef());

            if (diagnostics_blob) {
                Log("Error loading module[{}]\n{}", path, (const char*)diagnostics_blob->getBufferPointer());
            }

            if (!module) {
                NOVA_THROW("Failed to load slang module [{}]", path);
            }
        }
    };

    std::vector<uint32_t> Vulkan_CompileSlangToSpirv(
            ShaderStage       /* stage */,
            StringView           entry,
            StringView        filename,
            Span<StringView> fragments)
    {
        static SlangSession slang_session;
        auto* session = slang_session.session.get();

        std::string str;
        if (auto loaded = vfs::LoadStringMaybe(filename)) {
            str = loaded.value();
        }

        NOVA_ASSERT(str.empty() || fragments.empty(), "Cannot load from VFS *and* include fragments");

        for (auto fragment : fragments) {
            str.append_range(fragment);
        }

        slang::IModule* slang_module = {};
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            slang_module = session->loadModuleFromSourceString(filename.CStr(), filename.CStr(), str.data(), diagnostics_blob.writeRef());
            if (diagnostics_blob) {
                Log("{}", (const char*)diagnostics_blob->getBufferPointer());
            }
            if (!slang_module) {
                NOVA_THROW("Slang module [{}] not found", filename);
            }
        }

        Slang::ComPtr<slang::IEntryPoint> entry_point;
        if (SLANG_FAILED(slang_module->findEntryPointByName(entry.CStr(), entry_point.writeRef()))) {
            NOVA_THROW("slang failed to find entry point: {}", entry);
        }

        std::vector<slang::IComponentType*> component_types;
        component_types.push_back(slang_module);
        component_types.push_back(entry_point);

        Slang::ComPtr<slang::IComponentType> composed_program;
        {
            Slang::ComPtr<slang::IBlob> diagnostics_blob;
            auto result = session->createCompositeComponentType(
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

        std::vector<u32> spirv;
        spirv.resize(spirv_code->getBufferSize() / sizeof(u32));
        std::memcpy(spirv.data(), spirv_code->getBufferPointer(), spirv_code->getBufferSize());

        return spirv;
    }
}