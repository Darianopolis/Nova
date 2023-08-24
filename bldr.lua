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

    local components = { 
        { 
            name = "core", 
            dependencies = { 
                "ankerl-maps",
                "glm",
                "mimalloc",
            } 
        },
        { name = "db", dependencies = { "sqlite3" } },
        { name = "os" },
        { 
            name = "rhi", 
            dependencies = {
                "vulkan",
                "VulkanMemoryAllocator",
                "glslang",
                "volk",
            } 
        },
        { 
            name = "ui",
            dependencies = {
                "freetype",
                "glfw",
                "imgui",
                "imgui-glfw",
                "imgui-vulkan",
                "imguizmo",
            }
        }
    }

    Include "src"
    for _, component in ipairs(components) do
        Compile { "src/nova/"..(component.name).."/**" }
        Import(component.dependencies or {})
    end

--------------------------------------------------------------------------------
--                               Examples
--------------------------------------------------------------------------------

    Compile "examples/**"
    Import { "stb" }
    Artifact { "out/example", type = "Console" }
end
