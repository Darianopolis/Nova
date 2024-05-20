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
        "spdlog",

        "sqlite3",

        "freetype",
        "harfbuzz",

        "imgui",

        "assimp",
        "fastgltf",
        "fast-obj",
        "ufbx",

        "stb_image",
        "tinyexr",
        "dds-ktx",
        "bc7enc",
        "Compressonator",
        "meshoptimizer",

        "ms-gdk",
        "wooting-sdk",

        "slang",
    }

    Compile {
        "src/nova/core/*",
        "src/nova/db/*",
        "src/nova/ui/*",
        "src/nova/asset/*",
        "src/nova/vfs/*",
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