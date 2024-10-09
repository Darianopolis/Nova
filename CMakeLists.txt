cmake_minimum_required(VERSION 3.30)
project(nova)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_options(
        /Zc:preprocessor
        /Zc:__cplusplus
        /utf-8)
include(FetchContent)
# ------------------------------------------------------------------------------
fetchcontent_declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git)
fetchcontent_makeavailable(glm)
# ------------------------------------------------------------------------------
fetchcontent_declare(mimalloc
        GIT_REPOSITORY https://github.com/microsoft/mimalloc.git)
fetchcontent_makeavailable(mimalloc)
# ------------------------------------------------------------------------------
fetchcontent_declare(simdutf
        GIT_REPOSITORY https://github.com/simdutf/simdutf.git)
fetchcontent_makeavailable(simdutf)
# ------------------------------------------------------------------------------
fetchcontent_declare(unordered_dense
        GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
        GIT_TAG main)
fetchcontent_makeavailable(unordered_dense)
# ------------------------------------------------------------------------------
fetchcontent_declare(fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git)
fetchcontent_makeavailable(fmt)
# ------------------------------------------------------------------------------
fetchcontent_declare(VMA
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git)
fetchcontent_makeavailable(VMA)
add_library(novadep-vma)
target_sources(novadep-vma
        PRIVATE
        ${VMA_SOURCE_DIR}/include/vk_mem_alloc.h)
set_source_files_properties(
        ${VMA_SOURCE_DIR}/include/vk_mem_alloc.h PROPERTIES LANGUAGE CXX)
target_compile_definitions(novadep-vma
        PUBLIC
        VMA_DYNAMIC_VULKAN_FUNCTIONS=1
        VMA_STATIC_VULKAN_FUNCTIONS=0
        PRIVATE
        VMA_IMPLEMENTATION)
target_link_libraries(novadep-vma
        PUBLIC
        Vulkan-Headers)
# ------------------------------------------------------------------------------
fetchcontent_declare(VULKAN_HEADERS
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git)
fetchcontent_makeavailable(VULKAN_HEADERS)
# ------------------------------------------------------------------------------
add_library(game-input INTERFACE)
if(MSVC)
    fetchcontent_declare(game-input
        URL https://www.nuget.org/api/v2/package/Microsoft.GameInput/0.2303.22621.3038
        URL_HASH MD5=e86dc113c4d4638f496564977da63c1f)
    fetchcontent_makeavailable(game-input)
    target_link_libraries(game-input
        INTERFACE
        ${game-input_SOURCE_DIR}/native/lib/x64/GameInput.lib)
    target_include_directories(game-input
        INTERFACE
        ${game-input_SOURCE_DIR}/native/include)
endif()
# ------------------------------------------------------------------------------
fetchcontent_declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking)
fetchcontent_makeavailable(imgui)
add_library(novadep-imgui)
target_sources(novadep-imgui
        PRIVATE
        ${imgui_SOURCE_DIR}/imgui.h
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp)
target_include_directories(novadep-imgui
        PUBLIC
        ${imgui_SOURCE_DIR})
# ------------------------------------------------------------------------------
fetchcontent_declare(freetype
        GIT_REPOSITORY https://github.com/freetype/freetype.git)
fetchcontent_makeavailable(freetype)
# ------------------------------------------------------------------------------
fetchcontent_declare(stb
        GIT_REPOSITORY https://github.com/nothings/stb.git)
fetchcontent_makeavailable(stb)
add_library(stb-image       ${stb_SOURCE_DIR}/stb_image.h)
set_source_files_properties(${stb_SOURCE_DIR}/stb_image.h PROPERTIES LANGUAGE C)
target_include_directories(stb-image PUBLIC ${stb_SOURCE_DIR})
target_compile_definitions(stb-image PRIVATE STB_IMAGE_IMPLEMENTATION=1)
# ------------------------------------------------------------------------------
fetchcontent_declare(tinyexr
        GIT_REPOSITORY https://github.com/syoyo/tinyexr.git
        GIT_TAG release)
fetchcontent_makeavailable(tinyexr)
target_include_directories(tinyexr PUBLIC ${tinyexr_SOURCE_DIR})
# ------------------------------------------------------------------------------
fetchcontent_declare(dds-ktx
        GIT_REPOSITORY https://github.com/septag/dds-ktx.git)
fetchcontent_makeavailable(dds-ktx)
add_library(dds-ktx         ${dds-ktx_SOURCE_DIR}/dds-ktx.h)
set_source_files_properties(${dds-ktx_SOURCE_DIR}/dds-ktx.h PROPERTIES LANGUAGE CXX)
target_include_directories(dds-ktx PUBLIC ${dds-ktx_SOURCE_DIR})
target_compile_definitions(dds-ktx PRIVATE DDSKTX_IMPLEMENT=1)
# ------------------------------------------------------------------------------
fetchcontent_declare(bc7enc
        GIT_REPOSITORY https://github.com/richgel999/bc7enc_rdo.git)
fetchcontent_makeavailable(bc7enc)
add_library(novadep-bc7enc)
target_sources(novadep-bc7enc
        PRIVATE
        ${bc7enc_SOURCE_DIR}/bc7enc.cpp
        ${bc7enc_SOURCE_DIR}/rdo_bc_encoder.cpp
        ${bc7enc_SOURCE_DIR}/rgbcx.cpp
        ${bc7enc_SOURCE_DIR}/bc7decomp_ref.cpp
        ${bc7enc_SOURCE_DIR}/bc7decomp.cpp
        ${bc7enc_SOURCE_DIR}/ert.cpp
        ${bc7enc_SOURCE_DIR}/lodepng.cpp)
target_include_directories(novadep-bc7enc
        PUBLIC
        ${bc7enc_SOURCE_DIR})
# ------------------------------------------------------------------------------
set(OPTION_ENABLE_ALL_APPS OFF)
set(LIB_BUILD_CORE ON)
fetchcontent_declare(compressonator
        GIT_REPOSITORY https://github.com/GPUOpen-Tools/compressonator.git)
fetchcontent_makeavailable(compressonator)
# ------------------------------------------------------------------------------
add_library(wooting-sdk INTERFACE)
if(MSVC)
    fetchcontent_declare(wooting-sdk
        URL https://github.com/WootingKb/wooting-analog-sdk/releases/download/v0.7.5/wooting-analog-sdk-v0.7.5-x86_64-pc-windows-msvc.tar.gz
        URL_HASH MD5=9ef7ba5ae54a35c963633dd581adfed0)
    fetchcontent_makeavailable(wooting-sdk)
    target_include_directories(wooting-sdk INTERFACE ${wooting-sdk_SOURCE_DIR}/wrapper/includes-cpp)
    target_link_libraries(wooting-sdk INTERFACE ${wooting-sdk_SOURCE_DIR}/wrapper/lib/wooting_analog_wrapper.lib)
endif()
# ------------------------------------------------------------------------------
fetchcontent_declare(yyjson
        GIT_REPOSITORY https://github.com/ibireme/yyjson.git)
fetchcontent_makeavailable(yyjson)
# ----------------------------------------------------------------------------------------------------------------------
fetchcontent_declare(xxhash
        GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
        GIT_TAG dev)
fetchcontent_makeavailable(xxhash)
add_library(xxhash)
target_sources(xxhash PRIVATE ${xxhash_SOURCE_DIR}/xxhash.c)
target_include_directories(xxhash PUBLIC ${xxhash_SOURCE_DIR})
# ------------------------------------------------------------------------------
set(SLANG_USE_SYSTEM_MINIZ ON)
set(SLANG_USE_SYSTEM_VULKAN_HEADERS ON)
set(SLANG_USE_SYSTEM_UNORDERED_DENSE ON)
set(SLANG_ENABLE_EXAMPLES OFF)
set(SLANG_ENABLE_GFX OFF)
fetchcontent_declare(slang
        GIT_REPOSITORY https://github.com/shader-slang/slang.git)
fetchcontent_makeavailable(slang)
# TODO: Hack
target_include_directories(slang PUBLIC ${slang_SOURCE_DIR}/include)
# ------------------------------------------------------------------------------
# ------------------------------------------------------------------------------
# ------------------------------------------------------------------------------
add_library(nova-core)
file(GLOB nova-core-sources
        src/nova/core/*.hpp)
target_sources(nova-core PUBLIC ${nova-core-sources})
# TODO: Move VFS to separate project
target_sources(nova-core
        PRIVATE
        src/nova/core/json.cpp
        src/nova/filesystem/VirtualFileSystem.hpp
        src/nova/filesystem/VirtualFileSystem.cpp)
if(MSVC)
    target_sources(nova-core
        PRIVATE
            src/nova/core/win32/Win32Files.cpp
            src/nova/core/win32/Win32.cpp
            src/nova/core/win32/Win32.hpp)
    target_compile_definitions(nova-core
        PUBLIC
        NOVA_PLATFORM_WINDOWS)
    target_link_libraries(nova-core
        PUBLIC
        onecore.lib
        winmm.lib)
endif()
target_include_directories(nova-core PUBLIC src)
target_link_libraries(nova-core
        PUBLIC
        glm
        mimalloc
        simdutf
        unordered_dense
        fmt
        yyjson)
# ------------------------------------------------------------------------------
add_executable(nova-build)
target_sources(nova-build
        PRIVATE
        src/nova/build/Build.cpp
        src/nova/build/BuildScan.cpp
        src/nova/build/BuildCLI.cpp
        src/nova/build/BuildConfigure.cpp

        src/nova/build/generators/CMakeGenerator.cpp)
if(MSVC)
    target_sources(nova-build
        PRIVATE
            src/nova/build/backend/ClangCLBackend.cpp
            src/nova/build/backend/MsvcBackend.cpp
            src/nova/build/backend/MsvcCommon.cpp)
endif()
target_link_libraries(nova-build
        PUBLIC
        nova-core
        xxhash)
add_custom_command(TARGET nova-build POST_BUILD COMMAND
        ${CMAKE_COMMAND} -E copy $<TARGET_FILE:nova-build> ${CMAKE_SOURCE_DIR}/out/build.exe)
# ------------------------------------------------------------------------------
add_library(nova-window)
if(MSVC)
    file(GLOB nova-window-sources-win32
            src/nova/window/win32/*.cpp
            src/nova/window/win32/*.hpp)
    target_sources(nova-window PRIVATE ${nova-window-sources-win32})
endif()
target_link_libraries(nova-window
        PUBLIC
        nova-core
        game-input
        novadep-imgui # TODO: DELETEME - This is required by debug GameInput code in Win32Input
        )
# ------------------------------------------------------------------------------
add_library(nova-gpu)
file(GLOB nova-gpu-sources
        src/nova/gpu/vulkan/*.hpp
        src/nova/gpu/vulkan/*.cpp
        src/nova/gpu/vulkan/khr/*.hpp
        src/nova/gpu/vulkan/khr/*.cpp)
target_sources(nova-gpu PUBLIC ${nova-gpu-sources})
if(MSVC)
    target_sources(nova-gpu
            PUBLIC
            src/nova/gpu/vulkan/dcomp/VulkanDCompSwapchain.cpp
            src/nova/gpu/vulkan/dcomp/VulkanDCompSwapchain.hpp
            src/nova/gpu/vulkan/dxgi/VulkanDXGISwapchain.cpp
            src/nova/gpu/vulkan/dxgi/VulkanDXGISwapchain.hpp
            src/nova/gpu/vulkan/gdi/VulkanGDISwapchain.cpp
            src/nova/gpu/vulkan/gdi/VulkanGDISwapchain.hpp
            src/nova/gpu/vulkan/win32/VulkanWin32.cpp)
    target_link_libraries(nova-gpu
            PUBLIC
            D3D12.lib
            DXGI.lib
            dcomp.lib
            d3d11.lib)
endif()
target_link_libraries(nova-gpu
        PUBLIC
        nova-core
        nova-window
        novadep-vma
        Vulkan-Headers
        VulkanMemoryAllocator)
# ------------------------------------------------------------------------------
add_library(nova-slang)
target_sources(nova-slang
        PUBLIC
        src/nova/gpu/slang/Compiler.cpp
        src/nova/gpu/vulkan/slang/VulkanSlang.cpp)
target_link_libraries(nova-slang
        PUBLIC
        nova-core
        nova-gpu
        slang)
# ------------------------------------------------------------------------------
add_library(nova-glslang)
target_sources(nova-glslang
        PUBLIC
        src/nova/gpu/vulkan/glsl/VulkanGlsl.cpp)
target_link_libraries(nova-glslang
        PUBLIC
        nova-core
        nova-gpu)
# ------------------------------------------------------------------------------
add_library(nova-gui)
file(GLOB nova-gui-sources
        src/nova/gui/*.cpp
        src/nova/gui/*.hpp)
target_sources(nova-gui PRIVATE ${nova-gui-sources})
target_link_libraries(nova-gui
        PUBLIC
        nova-core
        novadep-imgui
        freetype)
# ------------------------------------------------------------------------------
add_library(nova-image)
file(GLOB nova-image-sources
        src/nova/image/*.cpp
        src/nova/image/*.hpp)
target_sources(nova-image PRIVATE ${nova-image-sources})
target_link_libraries(nova-image
        PUBLIC
        nova-core
        stb-image
        tinyexr
        dds-ktx
        novadep-bc7enc
        CMP_Core)
# ------------------------------------------------------------------------------
add_executable(nova-examples)
target_sources(nova-examples
        PRIVATE
        examples/Allocator.cpp
        examples/Blur.cpp
        examples/CommandLists.cpp
        examples/Compute.cpp
        examples/CopyTest.cpp
        examples/DrawTest.cpp
        examples/ImGui.cpp
        examples/Input.cpp
        examples/MinContext.cpp
        examples/MultiPresent.cpp
        examples/MultiView.cpp
        examples/Quads.cpp
        examples/RayTracing.cpp
        examples/RoundRect.cpp
        examples/TriangleExtended.cpp
        examples/TriangleMeshShader.cpp
        examples/TriangleRayTrace.cpp
        examples/Triangle.cpp
        examples/Wooting.cpp
        examples/MinContext.cpp
        examples/main/Main.cpp
        examples/main/Main.hpp)
target_link_libraries(nova-examples
        PRIVATE
        nova-core
        nova-gpu
        nova-window
        nova-gui
        nova-image
        nova-slang
        wooting-sdk)
add_custom_command(TARGET nova-examples POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
        -t ${nova_BINARY_DIR}
        ${mimalloc_BINARY_DIR}/mimalloc-debug.dll
        ${mimalloc_BINARY_DIR}/mimalloc-redirect.dll)
