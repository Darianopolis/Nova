if Project "nova" then

--------------------------------------------------------------------------------
--                               Platform
--------------------------------------------------------------------------------

    if Os "Win32" then
        Define "NOVA_PLATFORM_WINDOWS"
        Define {
            "WINVER=0x011A00",
            "_WIN32_WINNT=0x011A00",
        }
    else
        Error "Nova currently only supported on Windows"
    end

--------------------------------------------------------------------------------
--                               Components
--------------------------------------------------------------------------------

    Include "src"

-------- Core ------------------------------------------------------------------

    Compile "src/nova/core/**"
    Import {
        "ankerl-maps",
        "glm",
        "mimalloc",
        "eastl",
    }

-------- DB --------------------------------------------------------------------

    Compile "src/nova/db/**"
    Import { "sqlite3" }

-------- OS --------------------------------------------------------------------

    Compile "src/nova/os/**"

-------- RHI -------------------------------------------------------------------

    Compile "src/nova/rhi/**"
    Import {
        "vulkan",
        "VulkanMemoryAllocator",
        "glslang",
        "volk"
    }

-------- UI --------------------------------------------------------------------

    Compile "src/nova/ui/**"
    Import {
        "freetype",
        "glfw",
        "imgui",
        "imgui-glfw",
        "imgui-vulkan",
        "imguizmo",
    }
end

--------------------------------------------------------------------------------
--                               Examples
--------------------------------------------------------------------------------

if Project "nova-examples" then
    Compile "examples/**"
    Import { "stb", "nova" }
    Artifact { "out/example", type = "Console" }
end