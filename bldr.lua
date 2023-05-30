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
        "sqlite3",
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

if Project "nova-rt-test" then
    Compile "example/HelloTriangleRT.cpp"
    Import "nova"
    Artifact { "out/rt-test", type = "Console" }
end

if Project "nova-tri-test" then
    Compile "example/HelloTriangle.cpp"
    Import "nova"
    Artifact { "out/tri-test", type = "Console" }
end