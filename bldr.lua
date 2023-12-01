if Project "nova" then
    Include "src"
    Compile "src/**"
    Import {
        "ankerl-maps",
        "mimalloc",
        "glm",
        "simdutf",

        "sqlite3",

        "vulkan",
        "VulkanMemoryAllocator",
        "glslang",
        "DXC",

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

end

--------------------------------------------------------------------------------
--                               Examples
--------------------------------------------------------------------------------

if Project "nova-examples" then
    Compile "examples/**"
    Import "nova"
    Artifact { "out/example", type = "Console" }
end