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
        "simdutf",
    }

-------- DB --------------------------------------------------------------------

    Compile "src/nova/db/**"
    Import { "sqlite3" }

-------- OS --------------------------------------------------------------------

    Compile "src/nova/os/**"
    Import "glfw"

-------- Lang ------------------------------------------------------------------

    Compile "src/nova/lang/**"

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
    -- Compile {
    --     "examples/example_Main.cpp",
    --     "examples/example_Compute.cpp",
    --     "examples/example_MultiPresent.cpp",
    --     "examples/example_TriangleMeshShader.cpp",
    --     "examples/example_TriangleExtended.cpp",
    --     "examples/example_TriangleMinimal.cpp",
    -- }
    Import { "stb", "nova" }
    Artifact { "out/example", type = "Console" }
end