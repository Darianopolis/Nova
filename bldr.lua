if Project "nova" then

    local win32 = true

    local vulkan = true
    local vkdxgi = false

--------------------------------------------------------------------------------

    Include "src"

    Import {
        "ankerl-maps",
        "mimalloc",
        "glm",
        "simdutf",

        "sqlite3",

        "freetype",
        "harfbuzz",

        "imgui",
        "imguizmo",

        "assimp",
        "fastgltf",
        "fast-obj",
        "ufbx",

        "stb",
        "bc7enc",
        "meshoptimizer",
    }

    Compile {
        "src/nova/core/*",
        "src/nova/db/*",
        "src/nova/ui/*",
        "src/nova/asset/*",
    }

--------------------------------------------------------------------------------

    if win32 then
        Define "NOVA_PLATFORM_WINDOWS"
        Compile {
            "src/nova/core/win32/*",
            "src/nova/window/win32/*",
        }
    end

--------------------------------------------------------------------------------

    if vulkan then
        Import {
            "vulkan",
            "VulkanMemoryAllocator",
            "glslang",
            "DXC",
        }

        Compile {
            "src/nova/rhi/vulkan/*",
        }

        if win32 then
            Compile "src/nova/rhi/vulkan/win32/*"
        end

        if vkdxgi then
            Compile "src/nova/rhi/vulkan/dxgi/*"
        else
            Compile "src/nova/rhi/vulkan/khr/*"
        end
    end

end

--------------------------------------------------------------------------------
--                               Examples
--------------------------------------------------------------------------------

if Project "nova-examples" then
    Compile "examples/**"
    Import "nova"
    Artifact { "out/example", type = "Console" }
end