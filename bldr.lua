if Project "nova" then
    Compile "src/**"
    Include "src"
    Define "NOVA_PLATFORM_WINDOWS"
    Define {
        "WINVER=0x011A00",
        "_WIN32_WINNT=0x011A00",
    }
    Import {
        -- Core
        "vulkan",
        "VulkanMemoryAllocator",
        "glslang",
        "volk",
        "glm",
        "mimalloc",
        "ankerl-maps",

        -- Helpers
        "freetype",
        "glfw",
        "imgui",
        "imgui-glfw",
        "imgui-vulkan",

        -- Test
        "stb",
        "fastgltf",
        "micromesh",

        -- Miscellaneous
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

if Project "nova-compute-test" then
    Compile "example/HelloCompute.cpp"
    Import "nova"
    Artifact { "out/compute-test", type = "Console" }
end

if Project "nova-job-test" then
    Compile "example/JobSystemTest.cpp"
    Import "nova"
    Artifact { "out/job-test", type = "Console" }
end