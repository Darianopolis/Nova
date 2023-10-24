cmake_minimum_required(VERSION 3.26)
project(Nova)
set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp src/*.h)
file(GLOB_RECURSE EXAMPLE_FILES examples/*.cpp examples/*.h)

add_compile_options(
    /Zc:preprocessor
    /arch:AVX512
    /EHsc
    /MD
    /openmp:llvm
    /Zc:__cplusplus
    /fp:fast
    /utf-8
    /std:c++latest
    )

add_library(${PROJECT_NAME} STATIC
        ${SOURCE_FILES}
        ${EXAMPLE_FILES}

        #--------------------------------------------------------------------
        # Dear ImGui
        #--------------------------------------------------------------------
        ${CMAKE_SOURCE_DIR}/dependencies/imgui/imconfig.h
        ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui.cpp
        ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui.h
        ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui_demo.cpp
        ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui_draw.cpp
        ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui_internal.h
        ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui_widgets.cpp
        ${CMAKE_SOURCE_DIR}/dependencies/imgui/imstb_rectpack.h
        ${CMAKE_SOURCE_DIR}/dependencies/imgui/imstb_textedit.h
        ${CMAKE_SOURCE_DIR}/dependencies/imgui/imgui_tables.cpp

        ${CMAKE_SOURCE_DIR}/dependencies/ImGuizmo/GraphEditor.cpp
        ${CMAKE_SOURCE_DIR}/dependencies/ImGuizmo/ImCurveEdit.cpp
        ${CMAKE_SOURCE_DIR}/dependencies/ImGuizmo/ImGradient.cpp
        ${CMAKE_SOURCE_DIR}/dependencies/ImGuizmo/ImGuizmo.cpp
        ${CMAKE_SOURCE_DIR}/dependencies/ImGuizmo/ImSequencer.cpp

        ${CMAKE_SOURCE_DIR}/dependencies/stb/stb_image.h
        )

target_include_directories(${PROJECT_NAME} PUBLIC
        src
        ${CMAKE_SOURCE_DIR}/dependencies
        ${CMAKE_SOURCE_DIR}/dependencies/imgui
        ${CMAKE_SOURCE_DIR}/dependencies/ImGuizmo
        ${CMAKE_SOURCE_DIR}/dependencies/stb
        ${CMAKE_SOURCE_DIR}/dependencies/glfw/include
        ${CMAKE_SOURCE_DIR}/dependencies/mimalloc/include
        )

target_link_libraries(${PROJECT_NAME} PUBLIC
        # Core
        Vulkan-Headers
        VulkanMemoryAllocator
        glslang
        glslang-default-resource-limits
        SPIRV
        MachineIndependent
        GenericCodeGen
        glm::glm
        mimalloc
        unordered_dense
        # dxc (directx compiler)

        # Helpers
        # freetype
        # glfw
        # imguizmo

        # Test
        # stb
        glfw

        # Misc
        # sqlite3
        )

add_definitions(
    -DNOVA_CMAKE_BUILD
    -D_UNICODE
    -DUNICODE
    -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS
    -DNOMINMAX
    -DVMA_STATIC_VULKAN_FUNCTIONS=0
    -DVMA_STATIC_DYNAMIC_FUNCTIONS=1
    -DNOVA_PLATFORM_WINDOWS
    )

add_subdirectory(dependencies)

add_executable(nova-test ${EXAMPLE_FILES})
target_link_libraries(nova-test ${PROJECT_NAME})