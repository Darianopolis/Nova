if Project "nova" then

    local win32 = true
    local vkdxgi = false

--------------------------------------------------------------------------------

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
        "ms-gdk",
        "wooting-sdk",

        -- RHI
        "slang",
        "vulkan",
        "VulkanMemoryAllocator",
        "glslang",
    }

    Compile {
        "src/nova/core/*",
        "src/nova/db/*",
        "src/nova/ui/*",
        "src/nova/asset/*",
        "src/nova/vfs/*",
        "src/nova/rhi/slang/**",
    }

--------------------------------------------------------------------------------

    if win32 then
        Define "NOVA_PLATFORM_WINDOWS"
        Compile {
            "src/nova/core/win32/**",
            "src/nova/window/win32/**",
        }
    end

--------------------------------------------------------------------------------

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

if Project "nova-pack" then
    Compile "src/nova/rhi-shader-gen/**"
    Import "nova"
    Artifact { "out/pack", type = "Console" }
end

if Project "nova-pack-output" then
    Compile ".vfs-pack-output/*"
    Import "nova"
end

--------------------------------------------------------------------------------
--                               Examples
--------------------------------------------------------------------------------

if Project "nova-examples" then
    Compile "examples/**"
    Import { "nova", "nova-pack-output" }
    Artifact { "out/example", type = "Console" }
end