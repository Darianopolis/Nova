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
end

if Project "nova-present-test" then
    Compile "example/MultiPresentTest.cpp"
    Import "nova"
    Artifact { "out/present-test", type = "Console" }
end

if Project "nova-draw-test" then
    Compile "example/BoxOverlayTest.cpp"
    Import "nova"
    Artifact { "out/draw-test", type = "Console" }
end
