#include "nova_VulkanHlsl.hpp"

#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

#include <nova/core/nova_Files.hpp>

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
            _In_z_ LPCWSTR pFilename,
            _COM_Outptr_result_maybenull_ IDxcBlob **ppIncludeSource)
        {
            auto target = std::filesystem::path(pFilename);

            if (std::filesystem::exists(target)) {
                if (included.contains(target)) {
                    // Return empty string blob if this file has been included before
                    constexpr const char* emptyStr = "";
                    IDxcBlobEncoding* blob = nullptr;
                    utils->CreateBlobFromPinned(emptyStr, 1, DXC_CP_ACP, &blob);
                    *ppIncludeSource = blob;
                    return S_OK;
                }

                included.insert(target);

                IDxcBlobEncoding* blob = nullptr;
                HRESULT hres = utils->LoadFile(target.wstring().c_str(), nullptr, &blob);
                if (SUCCEEDED(hres)) {
                    *ppIncludeSource = blob;
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

    std::vector<uint32_t> hlsl::Compile(
            ShaderStage stage,
            std::string_view entry,
            const std::string& filename,
            Span<std::string_view> fragments)
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
                hlsl.append(f);
            }
        } else {
            hlsl = files::ReadTextFile(filename);
        }

        // Arguments

#define NOVA_HLSL_SM L"_6_7"
        const wchar_t* targetProfile;
        switch (GetVulkanShaderStage(stage)) {
            break;case VK_SHADER_STAGE_VERTEX_BIT:                  targetProfile = L"vs" NOVA_HLSL_SM;
            break;case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:    targetProfile = L"hs" NOVA_HLSL_SM;
            break;case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: targetProfile = L"ds" NOVA_HLSL_SM;
            break;case VK_SHADER_STAGE_GEOMETRY_BIT:                targetProfile = L"gs" NOVA_HLSL_SM;
            break;case VK_SHADER_STAGE_FRAGMENT_BIT:                targetProfile = L"ps" NOVA_HLSL_SM;
            break;case VK_SHADER_STAGE_COMPUTE_BIT:                 targetProfile = L"cs" NOVA_HLSL_SM;
            break;case VK_SHADER_STAGE_RAYGEN_BIT_KHR:              targetProfile = L"lib" NOVA_HLSL_SM;
            break;case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:             targetProfile = L"lib" NOVA_HLSL_SM;
            break;case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:         targetProfile = L"lib" NOVA_HLSL_SM;
            break;case VK_SHADER_STAGE_MISS_BIT_KHR:                targetProfile = L"lib" NOVA_HLSL_SM;
            break;case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:        targetProfile = L"lib" NOVA_HLSL_SM;
            break;case VK_SHADER_STAGE_CALLABLE_BIT_KHR:            targetProfile = L"lib" NOVA_HLSL_SM;
            break;case VK_SHADER_STAGE_TASK_BIT_EXT:                targetProfile = L"as" NOVA_HLSL_SM;
            break;case VK_SHADER_STAGE_MESH_BIT_EXT:                targetProfile = L"ms" NOVA_HLSL_SM;

            break;default: NOVA_THROW("Unknown stage: {}", int(stage));
        }
#undef NOVA_HLSL_SM

        auto toWide = [](std::string_view str) -> std::wstring {
            std::wstring out;
            out.resize(simdutf::utf16_length_from_utf8(str.data(), str.size()));
            simdutf::convert_valid_utf8_to_utf16(str.data(), str.size(), reinterpret_cast<char16_t*>(out.data()));
            return out;
        };

        auto wFilename = toWide(filename);
        auto wEntry = toWide(entry);

        auto arguments = std::to_array<const wchar_t*>({

            // Input
            wFilename.c_str(),
            L"-E", wEntry.c_str(),

            // Include
            L"-I", L".",

            // Env
            L"-T", targetProfile,
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

        HlslIncluder includeHandler;
        includeHandler.utils = utils.GetPtr();

        ComPtr<IDxcResult> result;
        auto hres = compiler->Compile(
            &buffer,
            arguments.data(),
            u32(arguments.size()),
            &includeHandler,
            IID_PPV_ARGS(&result));

        if (SUCCEEDED(hres)) {
            result->GetStatus(&hres);
        }

        // Report Errors

        ComPtr<IDxcBlobEncoding> errorBlob;
        if (result
                && SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errorBlob), nullptr))
                && errorBlob
                && errorBlob->GetBufferSize() > 0) {
            NOVA_THROW("Shader compilation failed:\n\n{}", static_cast<const char*>(errorBlob->GetBufferPointer()));
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