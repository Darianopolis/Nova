if Project "ankerl-maps" then
    Dir "vendor/unordered_dense"
    Include "build/install/include"
end

if Project "glm" then
    Dir "vendor/glm"
    Include "."
end

if Project "simdutf" then
    Dir "vendor/simdutf"
    Include "build/install/include"
    Link "build/install/lib/*.lib"
end

--------------------------------------------------------------------------------

if Project "sqlite3" then
    Dir "vendor/sqlite3"
    Compile "sqlite3.c"
    Include "."
end

--------------------------------------------------------------------------------

if Project "vulkan" then
    Include "vendor/Vulkan-Headers/include"
    Link "vendor/Vulkan-Loader/build/install/lib/vulkan-1.lib"
end

if Project "VulkanMemoryAllocator" then
    Dir "vendor/VulkanMemoryAllocator"
    Import "vulkan"
    Include "build/install/include"
    Define {
        "VMA_DYNAMIC_VULKAN_FUNCTIONS=1",
        "VMA_STATIC_VULKAN_FUNCTIONS=0",
    }
    Define { "VMA_IMPLEMENTATION", scope = "build" }
    Compile { "build/install/include/vk_mem_alloc.h", type = "cpp" }
end

if Project "glslang" then
    Dir "vendor/glslang"

    Include "build/install/include"
    Link {
        "build/install/lib/GenericCodeGen.lib",
        "build/install/lib/glslang.lib",
        "build/install/lib/glslang-default-resource-limits.lib",
        "build/install/lib/HLSL.lib",
        "build/install/lib/MachineIndependent.lib",
        "build/install/lib/OGLCompiler.lib",
        "build/install/lib/OSDependent.lib",
        "build/install/lib/SPIRV.lib",
        "build/install/lib/SPVRemapper.lib",
        "build/install/lib/SPIRV-Tools.lib",
        "build/install/lib/SPIRV-Tools-diff.lib",
        "build/install/lib/SPIRV-Tools-link.lib",
        "build/install/lib/SPIRV-Tools-lint.lib",
        "build/install/lib/SPIRV-Tools-opt.lib",
        "build/install/lib/SPIRV-Tools-reduce.lib",
    }
end

if Project "DXC" then
    Dir "vendor/DXC"
    Include "inc"
    Link "lib/x64/*.lib"
    Dynamic "bin/x64/*.dll"
end

--------------------------------------------------------------------------------

if Project "freetype" then
    Dir "vendor/freetype"
    Include "build/install/include/freetype2"
    Link "build/install/lib/*.lib"
end

if Project "glfw" then
    Dir "vendor/glfw"
    Define { "GLFW_INCLUDE_NONE", "GLFW_EXPOSE_NATIVE_WIN32", "NOMINMAX" }
    Include "build/install/include"
    Link "build/install/lib/glfw3.lib"
end

if Project "imgui" then
    Dir "vendor/imgui"
    Compile "./*"
    Define {
        "IMGUI_DISABLE_OBSOLETE_FUNCTIONS",
        "IMGUI_DISABLE_OBSOLETE_KEYIO",
    }
    Include { ".", "backends" }
end

if Project "imgui-glfw" then
    Dir "vendor/imgui"
    Compile "backends/imgui_impl_glfw.cpp"
    Define "GLFW_HAS_VULKAN"
    Import { "imgui", "glfw" }
end

if Project "imgui-vulkan" then
    Dir "vendor/imgui"
    Compile "backends/imgui_impl_vulkan.cpp"
    Import { "imgui", "vulkan" }
end

if Project "imguizmo" then
    Dir "vendor/ImGuizmo"
    Compile "./*"
    Include "."
    Import "imgui"
end

--------------------------------------------------------------------------------

if Project "stb" then
    Dir "vendor/stb"
    Include "."
    Define { "STB_IMAGE_IMPLEMENTATION", scope = "build" }
    Compile { "stb_image.h", type = "cpp" }
end

if Project "bc7enc" then
    Dir "vendor/bc7enc_rdo"
    Compile {
        "bc7enc.cpp",
        "rdo_bc_encoder.cpp",
        "rgbcx.cpp",
        "utils.cpp",
        "bc7decomp_ref.cpp",
        "bc7decomp.cpp",
        "ert.cpp",
        "lodepng.cpp",
    }
    Include "."
end