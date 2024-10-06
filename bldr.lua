if Project "nova" then
    Include "src"

    Import {
        -- Core
        "ankerl-maps",
        "mimalloc",
        "glm",
        "simdutf",
        "fmt",

        -- Database
        "sqlite3",

        -- UI
        "freetype",
        "harfbuzz",
        "imgui",

        -- Asset
        "stb_image",
        "tinyexr",
        "dds-ktx",
        "bc7enc",
        "Compressonator",

        -- Window
        "GameInput",
        "wooting-sdk",

        -- RHI
        "vulkan",
        "VulkanMemoryAllocator",
    }

    Compile {
        "src/nova/core/*",
        "src/nova/db/*",
        "src/nova/ui/*",
        "src/nova/asset/*",
        "src/nova/vfs/*",
        "src/nova/window/*",
    }

    Embed "assets/fonts/CONSOLA.TTF"

    Compile {
        "src/nova/rhi/*",
        "src/nova/rhi/vulkan/*",
        "src/nova/rhi/vulkan/dxgi/*",
        "src/nova/rhi/vulkan/khr/*",
    }

    if Platform "Win32" then
        Define "NOVA_PLATFORM_WINDOWS"
        Compile {
            "src/nova/core/win32/**",
            "src/nova/window/win32/**",
            "src/nova/rhi/vulkan/win32/*",
            "src/nova/rhi/vulkan/gdi/*",
        }
    end
end

--------------------------------------------------------------------------------
--                         Shader compiler modules
--------------------------------------------------------------------------------

if Project "nova-slang" then
    Compile {
        "src/nova/rhi/slang/**",
        "src/nova/rhi/vulkan/slang/**",
    }
    Import { "nova", "slang" }
end

if Project "nova-glsl" then
    Compile "src/nova/rhi/vulkan/glsl/**"
    Import { "nova", "glslang" }
end

--------------------------------------------------------------------------------
--                               Examples
--------------------------------------------------------------------------------

if Project "nova-examples" then
    Compile "examples/**"
    Embed "assets/fonts/arial.ttf"
    Import "nova"
    Artifact { "out/example", type = "Console" }
end
