if Project "nova" then
    Compile "src/**"
    Include "src"
    Define "NOVA_PLATFORM_WINDOWS"
    Import {
        "glfw",
        "vulkan",
        "VulkanMemoryAllocator",
        "glslang",
        "glm",
        "cgltf",
        "stb",
        "imgui",
        "imgui-glfw",
        "imgui-vulkan",
        "meshoptimizer",
        "volk",
        "ankerl-maps",
        "imguizmo",
        "jolt",
        "Compressonator",
        "mimalloc",
        "freetype",
    }

    -- Compile "example/MultiPresentTest.cpp"
    Compile "example/BoxOverlayTest.cpp"

    Artifact { "out/main", type = "Console" }
end

-- if Project "nova-example" then
--     Compile "example/MultiPresentTest.cpp"
--     -- Compile "example/BoxOverlayTest.cpp"
--     Import "nova"
--     Artifact { "out/main", type = "Console" }
-- end
