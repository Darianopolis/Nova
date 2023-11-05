where /q wget
goto wget_missing_%errorlevel%
:wget_missing_1
    echo Missing wget, install and try again
    GOTO:eof
:wget_missing_0

where /q 7z
goto 7z_missing_%errorlevel%
:7z_missing_1
    echo Missing 7z, install and try again
    GOTO:eof
:7z_missing_0

where /q python
goto python_missing_%errorlevel%
:python_missing_1
    echo Missing python, install and try again
    GOTO:eof
:python_missing_0

mkdir build\vendor
cd build\vendor

    git clone https://github.com/martinus/unordered_dense.git
    cd unordered_dense
        cmake . -DCMAKE_INSTALL_PREFIX=build/install -B build
        cmake --build build --config Release --target install
    cd ..

    git clone https://github.com/g-truc/glm.git

    git clone https://github.com/simdutf/simdutf.git
    cd simdutf
        cmake . -DCMAKE_INSTALL_PREFIX=build/install -B build
        cmake --build build --config Release --target install
    cd ..

    wget https://www.sqlite.org/2023/sqlite-amalgamation-3430200.zip
    7z x sqlite-amalgamation-3430200.zip -osqlite3
    del /Q /F sqlite-amalgamation-3430200.zip
    cd sqlite3
        move sqlite-amalgamation-3430200\* .
        rmdir sqlite-amalgamation-3430200
    cd ..

    git clone https://github.com/KhronosGroup/Vulkan-Loader.git
    cd Vulkan-Loader
        cmake . -DUPDATE_DEPS=ON -DCMAKE_INSTALL_PREFIX=build/install -B build
        cmake --build build --config Release --target install
    cd ..

    git clone https://github.com/KhronosGroup/Vulkan-Headers
    cd Vulkan-Headers
        cmake . -DCMAKE_INSTALL_PREFIX=build/install -B build
        cmake --build build --config Release --target install
    cd ..

    git clone https://github.com/zeux/volk.git

    git clone https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    cd VulkanMemoryAllocator
        cmake . -DCMAKE_INSTALL_PREFIX=build/install -B build
        cmake --build build --config Release --target install
    cd ..

    git clone https://github.com/KhronosGroup/glslang.git
    cd glslang
        python update_glslang_sources.py
        cmake . -DCMAKE_INSTALL_PREFIX=build/install -B build
        cmake --build build --config Release --target install
    cd ..

    wget https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.7.2308/dxc_2023_08_14.zip
    7z x dxc_2023_08_14.zip -oDXC -y
    del /Q /F dxc_2023_08_14.zip

    git clone https://github.com/freetype/freetype.git
    cd freetype
        cmake . -DFT_DISABLE_ZLIB=ON -DCMAKE_INSTALL_PREFIX=build/install -B build
        cmake --build build --config Release --target install
    cd ..

    git clone https://github.com/glfw/glfw.git
    cd glfw
        cmake . -DCMAKE_INSTALL_PREFIX=build/install -B build
        cmake --build build --config Release --target install
    cd ..

    git clone https://github.com/ocornut/imgui.git
    cd imgui
        git checkout docking
        git pull
    cd ..

    git clone https://github.com/CedricGuillemet/ImGuizmo.git

    git clone https://github.com/nothings/stb.git

    git clone https://github.com/richgel999/bc7enc_rdo.git

cd ..\..

bldr make -clean sqlite3 ^
    VulkanMemoryAllocator ^
    imgui ^
    imgui-glfw ^
    imgui-vulkan ^
    imguizmo ^
    stb ^
    bc7enc

bldr ide nova-examples