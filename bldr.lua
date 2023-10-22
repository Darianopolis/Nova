if Project "nova" then

--------------------------------------------------------------------------------
--                               Platform
--------------------------------------------------------------------------------

    if Os "Win32" then
        Define "NOVA_PLATFORM_WINDOWS"
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
        "simdutf",
    }

-------- DB --------------------------------------------------------------------

    Compile "src/nova/db/**"
    Import { "sqlite3" }

-------- OS --------------------------------------------------------------------

    -- Compile "src/nova/os/**"
    -- Import "glfw"

-------- Lang ------------------------------------------------------------------

    -- Compile "src/nova/lang/**"

-------- RHI -------------------------------------------------------------------

    Compile "src/nova/rhi/**"
    Import {
        "vulkan",
        "volk",
        "VulkanMemoryAllocator",
        "glslang",
        "DXC",
    }

-------- UI --------------------------------------------------------------------

    Compile "src/nova/ui/**"
    Import {
        "freetype",
        "glfw",
        "imgui",
        "imgui-glfw",
        "imguizmo",
    }
end

--------------------------------------------------------------------------------
--                               Examples
--------------------------------------------------------------------------------

if Project "nova-examples" then
    Compile "examples/**"
    Import {
        "stb",
        "nova",
        "bc7enc",
    }
    Artifact { "out/example", type = "Console" }
end