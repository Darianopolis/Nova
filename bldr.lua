if Project "nova" then
    Compile "src/**"
    Include "src"
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
    }
end

if Project "nova-example" then
    Compile "example/main.cpp"
    Import "nova"
    Artifact { "out/main", type = "Console" }
end
