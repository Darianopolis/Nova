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
add_library(imgui)
target_sources(imgui
        PRIVATE
        ${imgui_SOURCE_DIR}/imgui.h
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp)
target_include_directories(imgui
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
# ------------------------------------------------------------------------------
# ------------------------------------------------------------------------------
add_library(nova-core)
file(GLOB nova-core-sources
        src/nova/core/*.hpp)
target_sources(nova-core PUBLIC ${nova-core-sources})
# TODO: Move VFS to separate project
target_sources(nova-core
        PRIVATE
        src/nova/vfs/nova_VirtualFilesystem.hpp
        src/nova/vfs/nova_VirtualFilesystem.cpp)
if(MSVC)
    target_sources(nova-core
        PRIVATE
        src/nova/core/win32/nova_FilesWin32.cpp
        src/nova/core/win32/nova_Win32.cpp
        src/nova/core/win32/nova_Win32.hpp)
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
        fmt)
# ------------------------------------------------------------------------------
add_library(nova-window)
target_include_directories(nova-window
        PUBLIC
        src/nova/window)
if(MSVC)
    file(GLOB nova-window-sources-win32
            src/nova/window/win32/*.cpp
            src/nova/window/win32/*.hpp)
    target_sources(nova-window PRIVATE ${nova-window-sources-win32})
endif()
target_link_libraries(nova-window
        PUBLIC
        nova-core
        nova-rhi
        game-input
        imgui # TODO: DELETEME - This is required by debug GameInput code in Win32Input
        )
# ------------------------------------------------------------------------------
add_library(nova-rhi)
file(GLOB nova-rhi-sources
        src/nova/rhi/vulkan/*.hpp
        src/nova/rhi/vulkan/*.cpp
        src/nova/rhi/vulkan/khr/*.hpp
        src/nova/rhi/vulkan/khr/*.cpp)
target_sources(nova-rhi PUBLIC ${nova-rhi-sources})
if(MSVC)
    target_sources(nova-rhi
            PUBLIC
            # src/nova/rhi/vulkan/dcomp/nova_VulkanDCompSwapchain.cpp
            # src/nova/rhi/vulkan/dcomp/nova_VulkanDCompSwapchain.hpp
            src/nova/rhi/vulkan/dxgi/nova_VulkanDXGISwapchain.cpp
            src/nova/rhi/vulkan/dxgi/nova_VulkanDXGISwapchain.hpp
            src/nova/rhi/vulkan/gdi/nova_VulkanGDISwapchain.cpp
            src/nova/rhi/vulkan/gdi/nova_VulkanGDISwapchain.hpp
            src/nova/rhi/vulkan/win32/nova_VulkanWin32.cpp)
    target_link_libraries(nova-rhi
            PUBLIC
            D3D12.lib
            DXGI.lib
            dcomp.lib
            d3d11.lib)
endif()
target_link_libraries(nova-rhi
        PUBLIC
        nova-core
        nova-window
        novadep-vma
        Vulkan-Headers
        VulkanMemoryAllocator)
# ------------------------------------------------------------------------------
add_library(nova-ui)
file(GLOB nova-ui-sources
        src/nova/ui/*.cpp
        src/nova/ui/*.hpp)
target_sources(nova-ui PRIVATE ${nova-ui-sources})
target_link_libraries(nova-ui
        PUBLIC
        nova-core
        imgui
        freetype)
# ------------------------------------------------------------------------------
add_library(nova-assets)
file(GLOB nova-assets-sources
        src/nova/asset/*.cpp
        src/nova/asset/*.hpp)
target_sources(nova-assets PRIVATE ${nova-assets-sources})
target_link_libraries(nova-assets
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
        examples/example_AllocatorTest.cpp
        examples/example_Blur.cpp
        examples/example_CommandListTest.cpp
        examples/example_Compute.cpp
        examples/example_Copy.cpp
        examples/example_Draw.cpp
        examples/example_ImGui.cpp
        examples/example_InputTest.cpp
        examples/example_MinContext.cpp
        examples/example_MultiPresent.cpp
        examples/example_MultiView.cpp
        examples/example_QuadTest.cpp
        examples/example_RayTracing.cpp
        examples/example_RoundRectTest.cpp
        examples/example_TriangleExtended.cpp
        examples/example_TriangleMeshShader.cpp
        examples/example_TriangleRayTraced.cpp
        examples/example_WootingTest.cpp
        examples/example_MinContext.cpp
        examples/main/example_Main.cpp
        examples/main/example_Main.hpp)
target_link_libraries(nova-examples
        PRIVATE
        nova-core
        nova-rhi
        nova-window
        nova-ui
        nova-assets
        wooting-sdk)
add_custom_command(TARGET nova-examples POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
        -t ${nova_BINARY_DIR}
        ${mimalloc_BINARY_DIR}/mimalloc-debug.dll
        ${mimalloc_BINARY_DIR}/mimalloc-redirect.dll)
