if Project "nova" then

--------------------------------------------------------------------------------
--                               Components
--------------------------------------------------------------------------------

    Include "src"

-------- Core ------------------------------------------------------------------

    Compile "src/nova/core/**"
    Import {
        "ankerl-maps",
        "glm",
        "simdutf",
        "mimalloc",
    }

-------- DB --------------------------------------------------------------------

    Compile "src/nova/db/**"
    Import { "sqlite3" }

-------- RHI -------------------------------------------------------------------

    Compile "src/nova/rhi/**"
    Import {
        "vulkan",
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


-------- Window ----------------------------------------------------------------

    Compile "src/nova/window/**"

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