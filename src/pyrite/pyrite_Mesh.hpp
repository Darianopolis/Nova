#pragma once

#include "Pyrite_Core.hpp"

namespace pyr
{
    using namespace nova::types;

    struct GltfVertex
    {
        Vec3 position = Vec3(0.f);
        Vec3 normal = Vec3(0.f);
        Vec2 texCoord = Vec2(0.f);
        u32 texIndex = 0;
    };

    struct GltfMesh
    {
        std::vector<GltfVertex> vertices;
        std::vector<u32> indices;
        std::vector<nova::ImageRef> images;
    };

    GltfMesh LoadMesh(nova::Context& ctx, const char* file, const char* baseDir);
}