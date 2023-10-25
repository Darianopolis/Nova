mkdir vendor
cd vendor

@REM call "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvarsx86_amd64.bat"

@REM git clone https://github.com/martinus/unordered_dense.git
@REM cd unordered_dense
@REM cmake . -DCMAKE_INSTALL_PREFIX=build/install -B build
@REM cmake --build build --config Release --target install
@REM cd ..

@REM git clone https://github.com/g-truc/glm.git

@REM git clone https://github.com/simdutf/simdutf.git
@REM cd simdutf
@REM cmake . -DCMAKE_INSTALL_PREFIX=build/install -B build
@REM cmake --build build --config Release --target install
@REM cd ..

@REM ..\wget https://www.sqlite.org/2023/sqlite-amalgamation-3430200.zip
@REM ..\7z x sqlite-amalgamation-3430200.zip -osqlite3
@REM del /Q /F sqlite-amalgamation-3430200.zip
@REM cd sqlite3
@REM move sqlite-amalgamation-3430200\* .
@REM rmdir sqlite-amalgamation-3430200
@REM mkdir build
@REM cd build
@REM cl /c ..\sqlite3.c
@REM cd ..\..

@REM git clone https://github.com/KhronosGroup/Vulkan-Headers
@REM cd Vulkan-Headers
@REM cmake . -DCMAKE_INSTALL_PREFIX=build/install -B build
@REM cmake --build build --config Release --target install
@REM cd ..
@REM git clone https://github.com/zeux/volk.git
@REM cd volk
@REM mkdir build
@REM cd build
@REM cl /c -I..\..\Vulkan-Headers\build\install\include ..\volk.c
@REM cd ..\..

@REM git clone https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
@REM cd VulkanMemoryAllocator
@REM cmake . -DCMAKE_INSTALL_PREFIX=build/install -B build
@REM cmake --build build --config Release --target install
@REM mkdir build\install\lib
@REM cd build\install\lib
@REM cl /c -I..\..\..\..\Vulkan-Headers\build\install\include -DVMA_IMPLEMENTATION /Tp ..\include\vk_mem_alloc.h
@REM cd ..\..\..\..

@REM git clone https://github.com/KhronosGroup/glslang.git
@REM cd glslang
@REM cmake . -DCMAKE_INSTALL_PREFIX=build/install -DENABLE_OPT=0 -B build
@REM cmake --build build --config Release --target install
@REM cd ..

@REM ..\wget https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.7.2308/dxc_2023_08_14.zip
@REM ..\7z x dxc_2023_08_14.zip -oDirectXShaderCompiler
@REM del /Q /F dxc_2023_08_14.zip

@REM git clone https://github.com/freetype/freetype.git
@REM cd freetype
@REM cmake . -DCMAKE_INSTALL_PREFIX=build/install -B build
@REM cmake --build build --config Release --target install
@REM cd ..

@REM git clone https://github.com/glfw/glfw.git
@REM cd glfw
@REM cmake . -DCMAKE_INSTALL_PREFIX=build/install -B build
@REM cmake --build build --config Release --target install
@REM cd ..

@REM git clone https://github.com/ocornut/imgui.git
@REM cd imgui
@REM git checkout docking
@REM git pull
@REM mkdir build
@REM cd build
@REM cl /c -I..\..\glfw\build\install\include -I..\. ..\imgui.cpp ..\backends\imgui_impl_glfw.cpp
@REM cd ..\..

@REM git clone https://github.com/CedricGuillemet/ImGuizmo.git
@REM cd ImGuizmo
@REM mkdir build
@REM cd build
@REM cl /c -I..\..\imgui ..\ImGuizmo.cpp
@REM cd ..\..

@REM git clone https://github.com/nothings/stb.git
@REM cd stb
@REM mkdir build
@REM cd build
@REM cl /c -DSTB_IMAGE_IMPLEMENTATION /Tp ..\stb_image.h
@REM cd ..\..

@REM git clone https://github.com/richgel999/bc7enc_rdo.git
@REM cd bc7enc_rdo
@REM mkdir build
@REM cd build
@REM cl /c /MP /EHsc ..\bc7enc.cpp ..\rdo_bc_encoder.cpp ..\rgbcx.cpp ..\utils.cpp ^
@REM ..\bc7decomp_ref.cpp ..\bc7decomp.cpp ..\ert.cpp ..\lodepng.cpp
@REM cd ..\..

@REM @REM git clone https://github.com/KhronosGroup/Vulkan-ValidationLayers.git
@REM @REM cd Vulkan-ValidationLayers
@REM @REM cmake -S . -B build -D UPDATE_DEPS=ON -D BUILD_WERROR=ON -D BUILD_TESTS=ON -D CMAKE_BUILD_TYPE=Debug
@REM @REM cmake --build build --config Debug
@REM @REM cd ..

cd ..

mkdir build
cd build

@REM cl /c ^
@REM     /MP ^
@REM     /nologo ^
@REM     /arch:AVX512 ^
@REM     /EHsc ^
@REM     /MD ^
@REM     /openmp:llvm ^
@REM     /Zc:preprocessor ^
@REM     /Zc:__cplusplus ^
@REM     /fp:fast ^
@REM     /std:c++latest ^
@REM     /D_CRT_SECURE_NO_WARNINGS ^
@REM     /W4 ^
@REM     /WX ^
@REM     /we4289 /w14242 /w14254 /w14263 ^
@REM     /w14265 /w14287 /w14296 /w14311 ^
@REM     /w14545 /w14546 /w14547 /w14549 ^
@REM     /w14555 /w14640 /w14826 /w14905 ^
@REM     /w14906 /w14928 /wd4324 /wd4505 ^
@REM     /permissive- ^
@REM     /O2 ^
@REM     /INCREMENTAL ^
@REM     /DEBUG ^
@REM     /Z7 ^
@REM     /DIMGUI_DISABLE_OBSOLETE_KEYIO ^
@REM     /D_UNICODE ^
@REM     /DIMGUI_DISABLE_OBSOLETE_FUNCTIONS ^
@REM     /DNOMINMAX ^
@REM     /DVK_USE_PLATFORM_WIN32_KHR ^
@REM     /DGLFW_HAS_VULKAN ^
@REM     /DGLFW_EXPOSE_NATIVE_WIN32 ^
@REM     /DVMA_STATIC_VULKAN_FUNCTIONS=0 ^
@REM     /DNOVA_PLATFORM_WINDOWS ^
@REM     /DGLFW_INCLUDE_NONE ^
@REM     /DVMA_DYNAMIC_VULKAN_FUNCTIONS=1 ^
@REM     /DUNICODE ^
@REM     /I..\src ^
@REM     /I..\vendor\glfw\build\install\include ^
@REM     /I..\vendor\imgui\backends ^
@REM     /I..\vendor\imgui ^
@REM     /I..\vendor\sqlite3 ^
@REM     /I..\vendor\unordered_dense\build\install\include ^
@REM     /I..\vendor\glslang\build\install\include ^
@REM     /I..\vendor\glm ^
@REM     /I..\vendor\ImGuizmo ^
@REM     /I..\vendor\DirectXShaderCompiler\inc ^
@REM     /I..\vendor\volk. ^
@REM     /I..\vendor\Vulkan-Headers\build\install\include ^
@REM     /I..\vendor\simdutf\build\install\include ^
@REM     /I..\vendor\VulkanMemoryAllocator\build\install\include ^
@REM     /I..\vendor\freetype\build\install\include\freetype2 ^
@REM     ..\src\nova\core\nova_Files.cpp ^
@REM     ..\src\nova\db\nova_Sqlite.cpp ^
@REM     ..\src\nova\rhi\vulkan\nova_VulkanAccelerationStructures.cpp ^
@REM     ..\src\nova\rhi\vulkan\nova_VulkanEnums.cpp ^
@REM     ..\src\nova\rhi\vulkan\nova_VulkanCompute.cpp ^
@REM     ..\src\nova\rhi\vulkan\nova_VulkanQueue.cpp ^
@REM     ..\src\nova\rhi\vulkan\nova_VulkanFence.cpp ^
@REM     ..\src\nova\rhi\vulkan\nova_VulkanPipelines.cpp ^
@REM     ..\src\nova\rhi\vulkan\nova_VulkanRayTracing.cpp ^
@REM     ..\src\nova\rhi\vulkan\nova_VulkanCommands.cpp ^
@REM     ..\src\nova\rhi\vulkan\nova_VulkanTexture.cpp ^
@REM     ..\src\nova\rhi\vulkan\nova_VulkanDrawing.cpp ^
@REM     ..\src\nova\rhi\vulkan\nova_VulkanSwapchain.cpp ^
@REM     ..\src\nova\rhi\vulkan\nova_VulkanBuffer.cpp ^
@REM     ..\src\nova\rhi\vulkan\nova_VulkanDescriptors.cpp ^
@REM     ..\src\nova\rhi\vulkan\nova_VulkanContext.cpp ^
@REM     ..\src\nova\rhi\vulkan\nova_VulkanShaders.cpp ^
@REM     ..\src\nova\rhi\vulkan\hlsl\nova_VulkanHlsl.cpp ^
@REM     ..\src\nova\rhi\vulkan\glsl\nova_VulkanGlsl.cpp ^
@REM     ..\src\nova\ui\nova_ImDraw2D.cpp ^
@REM     ..\src\nova\ui\nova_ImGui.cpp

cd ..