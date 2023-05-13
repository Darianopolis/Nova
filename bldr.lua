if Project "pyrite-v4" then
    Compile {
        "src/**",
    }
    Include {
        "src",
    }
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
    Artifact { "out/main", type = "Console" }
end
