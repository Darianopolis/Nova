#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

#include <nova/core/nova_Files.hpp>
#include <nova/core/nova_Strings.hpp>

#include <simdutf.h>

#include "Unknwnbase.h"
#include "dxcapi.h"

namespace nova
{
    template<class T>
    struct ComPtr
    {
        T* t = nullptr;

        ~ComPtr()
        {
            if (t) {
                t->Release();
            }
        }

        operator bool() { return t;  }
        T** operator&() { return &t; }
        T* operator->() { return t;  }
        T* GetPtr()     { return t;  }
    };

    struct HlslIncluder : public IDxcIncludeHandler
    {
        IDxcUtils* utils;

        std::unordered_set<std::filesystem::path> included;

    public:
        virtual ~HlslIncluder() = default;

        virtual HRESULT STDMETHODCALLTYPE LoadSource(
            _In_z_ LPCWSTR filename,
            _COM_Outptr_result_maybenull_ IDxcBlob** include_source)
        {
            auto target = std::filesystem::path(filename);

            if (std::filesystem::exists(target)) {
                if (included.contains(target)) {
                    // Return empty string blob if this file has been included before
                    constexpr const char* emptyStr = "";
                    IDxcBlobEncoding* blob = nullptr;
                    utils->CreateBlobFromPinned(emptyStr, 1, DXC_CP_ACP, &blob);
                    *include_source = blob;
                    return S_OK;
                }

                included.insert(target);

                IDxcBlobEncoding* blob = nullptr;
                HRESULT hres = utils->LoadFile(target.wstring().c_str(), nullptr, &blob);
                if (SUCCEEDED(hres)) {
                    *include_source = blob;
                    return S_OK;
                }

                return E_FAIL;
            }

            return E_FAIL;
        };

// -----------------------------------------------------------------------------

        virtual HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**) { return E_NOINTERFACE; }
        virtual ULONG   STDMETHODCALLTYPE AddRef()  { return 1; }
        virtual ULONG   STDMETHODCALLTYPE Release() { return 1; }
    };

    std::vector<u32> Vulkan_CompileHlslToSpirv(
            ShaderStage          stage,
            StringView           entry,
            StringView        filename,
            Span<StringView> fragments)
    {
        // Init

        ComPtr<IDxcLibrary> library;
        if (FAILED(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library)))) {
            NOVA_THROW("Could not init DXC Library");
        }

        ComPtr<IDxcCompiler3> compiler;
        if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)))) {
            NOVA_THROW("Could not init DXC Compiler");
        }

        ComPtr<IDxcUtils> utils;
        if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)))) {
            NOVA_THROW("Could not init DXC Utiliy");
        }

        // Load

        std::string hlsl;
        if (fragments.size()) {
            for (auto& f : fragments) {
                hlsl.append_range(f);
            }
        } else {
            hlsl = files::ReadTextFile(filename);
        }

        // Arguments

#define NOVA_HLSL_SM L"_6_7"
        const wchar_t* target_profile;
        switch (stage) {
            break;case nova::ShaderStage::Vertex:       target_profile = L"vs"  NOVA_HLSL_SM;
            break;case nova::ShaderStage::TessControl:  target_profile = L"hs"  NOVA_HLSL_SM;
            break;case nova::ShaderStage::TessEval:     target_profile = L"ds"  NOVA_HLSL_SM;
            break;case nova::ShaderStage::Geometry:     target_profile = L"gs"  NOVA_HLSL_SM;
            break;case nova::ShaderStage::Fragment:     target_profile = L"ps"  NOVA_HLSL_SM;
            break;case nova::ShaderStage::Compute:      target_profile = L"cs"  NOVA_HLSL_SM;
            break;case nova::ShaderStage::RayGen:       target_profile = L"lib" NOVA_HLSL_SM;
            break;case nova::ShaderStage::AnyHit:       target_profile = L"lib" NOVA_HLSL_SM;
            break;case nova::ShaderStage::ClosestHit:   target_profile = L"lib" NOVA_HLSL_SM;
            break;case nova::ShaderStage::Miss:         target_profile = L"lib" NOVA_HLSL_SM;
            break;case nova::ShaderStage::Intersection: target_profile = L"lib" NOVA_HLSL_SM;
            break;case nova::ShaderStage::Callable:     target_profile = L"lib" NOVA_HLSL_SM;
            break;case nova::ShaderStage::Task:         target_profile = L"as"  NOVA_HLSL_SM;
            break;case nova::ShaderStage::Mesh:         target_profile = L"ms"  NOVA_HLSL_SM;

            break;default: NOVA_THROW("Unknown stage: {}", int(stage));
        }
#undef NOVA_HLSL_SM

        auto wfilename = ToUtf16(filename);
        auto wentry = ToUtf16(entry);

        auto arguments = std::to_array<const wchar_t*>({

            // Input
            wfilename.c_str(),
            L"-E", wentry.c_str(),

            // Include
            L"-I", L".",

            // Env
            L"-T", target_profile,
            L"-spirv",
            L"-fspv-target-env=vulkan1.3",
            L"-HV", L"2021",

            // Features
            L"-enable-16bit-types",

            // Layout
            L"-fvk-use-scalar-layout", // scalar
            L"-Zpc",                   // column-major

            // Optimization
            L"-res-may-alias",
            L"-ffinite-math-only",
        });

        // Compile

        DxcBuffer buffer{};
        buffer.Encoding = DXC_CP_ACP;
        buffer.Ptr = hlsl.data();
        buffer.Size = hlsl.size();

        HlslIncluder include_handler;
        include_handler.utils = utils.GetPtr();

        ComPtr<IDxcResult> result;
        auto hres = compiler->Compile(
            &buffer,
            arguments.data(),
            u32(arguments.size()),
            &include_handler,
            IID_PPV_ARGS(&result));

        if (SUCCEEDED(hres)) {
            result->GetStatus(&hres);
        }

        // Report Errors

        ComPtr<IDxcBlobEncoding> error_blob;
        if (result
                && SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&error_blob), nullptr))
                && error_blob
                && error_blob->GetBufferSize() > 0) {
            NOVA_THROW("Shader compilation failed:\n\n{}", static_cast<const char*>(error_blob->GetBufferPointer()));
        } else if (FAILED(hres)) {
            NOVA_THROW("Shader compilation failed: Message unavailable");
        }

        // Get SPIR-V

        ComPtr<IDxcBlob> code;
        result->GetResult(&code);

        std::vector<u32> spirv;
        spirv.resize(code->GetBufferSize() / sizeof(u32));
        std::memcpy(spirv.data(), code->GetBufferPointer(), code->GetBufferSize());

        return spirv;
    }
}