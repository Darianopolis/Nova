# cmake_minimum_required(VERSION 3.26)
# project(dependencies)
# set(CMAKE_CXX_STANDARD 23)

# add_subdirectory(freetype)
# add_subdirectory(glfw)
# add_subdirectory(glslang)
# add_subdirectory(mimalloc)
# add_subdirectory(simdutf)
# add_subdirectory(unordered_dense)
# add_subdirectory(Vulkan-Headers)
# add_subdirectory(VulkanMemoryAllocator)

cmake_minimum_required(VERSION 3.26)
project(thirdparty)
set(CMAKE_CXX_STANDARD 23)

# Core

add_library(Vulkan-Headers INTERFACE)
set(VULKAN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Vulkan-Headers/include)
target_sources(Vulkan-Headers INTERFACE ${VULKAN_INCLUDE_DIR}/vulkan/vulkan.h)
target_include_directories(Vulkan-Headers INTERFACE ${VULKAN_INCLUDE_DIR})

target_compile_definitions(Vulkan-Headers INTERFACE VK_NO_PROTOTYPES)

if (ANDROID)
    target_compile_definitions(Vulkan-Headers INTERFACE VK_USE_PLATFORM_ANDROID_KHR)
elseif (WIN32)
    target_compile_definitions(Vulkan-Headers INTERFACE VK_USE_PLATFORM_WIN32_KHR)
elseif (APPLE)
    target_compile_definitions(Vulkan-Headers INTERFACE VK_USE_PLATFORM_METAL_EXT)
elseif (UNIX)
    # Choose WSI based on VKB_WSI_SELECTION
    if (VKB_WSI_SELECTION STREQUAL XCB OR VKB_WSI_SELECTION STREQUAL XLIB OR VKB_WSI_SELECTION STREQUAL WAYLAND)
        find_package(PkgConfig REQUIRED)
    endif ()
    if (VKB_WSI_SELECTION STREQUAL XCB)
        pkg_check_modules(XCB xcb REQUIRED)
        if (XCB_FOUND)
            target_compile_definitions(Vulkan-Headers INTERFACE VK_USE_PLATFORM_XCB_KHR)
        endif ()
    elseif (VKB_WSI_SELECTION STREQUAL XLIB)
        pkg_check_modules(X11 x11 REQUIRED)
        if (X11_FOUND)
            target_compile_definitions(Vulkan-Headers INTERFACE VK_USE_PLATFORM_XLIB_KHR)
        endif ()
    elseif (VKB_WSI_SELECTION STREQUAL WAYLAND)
        pkg_check_modules(WAYLAND wayland-client REQUIRED)
        if (WAYLAND_FOUND)
            target_compile_definitions(Vulkan-Headers INTERFACE VK_USE_PLATFORM_WAYLAND_KHR)
        endif ()
    elseif (VKB_WSI_SELECTION STREQUAL D2D)
        set(DIRECT_TO_DISPLAY TRUE)
        set(DIRECT_TO_DISPLAY TRUE PARENT_SCOPE)
        target_compile_definitions(Vulkan-Headers INTERFACE VK_USE_PLATFORM_DISPLAY_KHR)
    else ()
        message(FATAL_ERROR "Unknown WSI")
    endif ()
endif ()

add_library(VulkanMemoryAllocator INTERFACE)
set(VMA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator/include)
target_sources(VulkanMemoryAllocator INTERFACE ${VMA_DIR}/vk_mem_alloc.h)
target_include_directories(VulkanMemoryAllocator INTERFACE ${VMA_DIR})
target_link_libraries(VulkanMemoryAllocator INTERFACE Vulkan-Headers)

# option(ENABLE_OPT 0)
add_subdirectory(glslang)

add_subdirectory(glm)

add_subdirectory(mimalloc)

add_subdirectory(unordered_dense)


# Helpers

# add_subdirectory(freetype)

option(GLFW_BUILD_DOCS OFF)
option(GLFW_BUILD_TESTS OFF)
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_INSTALL OFF)
add_subdirectory(glfw)

# add_subdirectory(imguizmo)

# Test

# add_subdirectory(stb)

# add_subdirectory(fastgltf)