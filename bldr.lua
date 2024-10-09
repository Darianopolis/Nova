if Project "nova" then
    Include "src"

    Import {
        -- Core
        "ankerl-maps",
        "mimalloc",
        "glm",
        "simdutf",
        "fmt",
        "yyjson",

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
        "src/nova/sqlite/*",
        "src/nova/gui/*",
        "src/nova/image/*",
        "src/nova/filesystem/*",
        "src/nova/window/*",
    }

    Embed "assets/fonts/CONSOLA.TTF"

    Compile {
        "src/nova/gpu/*",
        "src/nova/gpu/vulkan/*",
        "src/nova/gpu/vulkan/dxgi/*",
        "src/nova/gpu/vulkan/khr/*",
    }

    if Platform "Win32" then
        Define "NOVA_PLATFORM_WINDOWS"
        Compile {
            "src/nova/core/win32/**",
            "src/nova/window/win32/**",
            "src/nova/gpu/vulkan/win32/*",
            "src/nova/gpu/vulkan/gdi/*",
        }
    end
end

--------------------------------------------------------------------------------
--                         Shader compiler modules
--------------------------------------------------------------------------------

if Project "nova-slang" then
    Compile {
        "src/nova/gpu/slang/**",
        "src/nova/gpu/vulkan/slang/**",
    }
    Import { "nova", "slang" }
end

if Project "nova-glsl" then
    Compile "src/nova/gpu/vulkan/glsl/**"
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
