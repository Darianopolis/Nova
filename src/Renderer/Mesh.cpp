#include "Mesh.hpp"

static byte* GltfGetBase(cgltf_accessor* accessor)
{
    byte* data = (byte*)accessor->buffer_view->data;
    if (!data)
        data = (byte*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset;

    return data + accessor->offset;
}

namespace pyr
{
    GltfMesh LoadMesh(Context& ctx, const char* file, const char* baseDir)
    {
        GltfMesh loadedMesh;

        PYR_LOG("Loading mesh: {}", file);

        PYR_TIMEIT_RESET();

        cgltf_options options = {};
        cgltf_data* data = nullptr;
        if (auto res = cgltf_parse_file(&options, file, &data); res != cgltf_result_success)
            PYR_THROW("Failed to load glTF file: {}", int(res));
        PYR_ON_SCOPE_EXIT(&) { cgltf_free(data); };
        PYR_TIMEIT("glTF: Loaded file");

        if (auto res = cgltf_load_buffers(&options, data, file); res != cgltf_result_success)
            PYR_THROW("Failed to load buffers for glTF: {}", int(res));
        PYR_TIMEIT("glTF: Loaded buffers");

        if (auto res = cgltf_validate(data); res != cgltf_result_success)
            PYR_THROW("Failed to validate glTF data: {}", int(res));
        PYR_TIMEIT("glTF: Validated");

// -----------------------------------------------------------------------------

        loadedMesh.images.resize(data->textures_count);

        auto intermediate = ctx.CreateImage(uvec3(4096, 4096, 0), 0, VK_FORMAT_R8G8B8A8_UNORM);

        std::cout << "Textures loaded: 0 / " << data->textures_count;
        uint32_t texturesLoaded = 0;

#pragma omp parallel for
        for (uint32_t textureId = 0; textureId < data->textures_count; ++textureId)
        {
            // PYR_LOG("Textures[{}] = {}", textureId, data->textures[textureId].image->uri);

            auto& texture = data->textures[textureId];

            if (texture.image->uri)
            {
                int width, height, channels;
                auto imageData = stbi_load(std::format("{}/{}", baseDir, texture.image->uri).c_str(), &width, &height, &channels, STBI_rgb_alpha);


                // {
                //     void* BC7Options;
                //     CreateOptionsBC7(&BC7Options);

                //     SetQualityBC7(BC7Options, 0.05f);
                //     SetMaskBC7(BC7Options, 0b01000010);

                //     auto blocks = std::make_unique<unsigned char[]>(width * height);
                //     for (u32 x = 0; x < width; x += 4)
                //     {
                //         for (u32 y = 0; y < height; y += 4)
                //         {
                //             u32 source[4][4];
                //             for (u32 ly = 0; ly < 4; ly++)
                //                 std::memcpy(source[ly], (u32*)imageData + (x + (y + ly) * width), 16);

                //             CompressBlockBC7((const unsigned char*)source, 16, &blocks[16 * (x/4 + (y/4 * width/4))], BC7Options);
                //         }
                //     }

                //     DestroyOptionsBC7(BC7Options);
                // }

                Ref<Image> loadedImage;
#pragma omp critical
                {
                    // if (width == 4096 && height == 4096)
                    // {
                    //     uint32_t newSize = 2048;
                    //     loadedImage = ctx.CreateImage({ newSize, newSize, 0u }, VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R8G8B8A8_UNORM, ImageFlags::Mips);
                    //     ctx.CopyToImage(intermediate, imageData, width * height * 4);
                    //     ctx.Transition(ctx.transferCmd, intermediate, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                    //     ctx.Transition(ctx.transferCmd, loadedImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                    //     vkCmdBlitImage(ctx.transferCmd, intermediate.image, intermediate.layout, loadedImage.image, loadedImage.layout, 1, Temp(VkImageBlit {
                    //         .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                    //         .srcOffsets = { VkOffset3D{}, VkOffset3D{(int32_t)intermediate.extent.x, (int32_t)intermediate.extent.y, 1} },
                    //         .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                    //         .dstOffsets = { VkOffset3D{}, VkOffset3D{(int32_t)loadedImage.extent.x, (int32_t)loadedImage.extent.y, 1} },
                    //     }), VK_FILTER_LINEAR);
                    //     ctx.GenerateMips(loadedImage);
                    // }
                    // else
                    {
                        loadedImage = ctx.CreateImage({ uint32_t(width), uint32_t(height), 0u }, VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R8G8B8A8_UNORM, ImageFlags::Mips);
                        ctx.CopyToImage(*loadedImage, imageData, width * height * 4);
                        ctx.GenerateMips(*loadedImage);

                        // loadedImage = ctx.CreateImage({ uint32_t(width), uint32_t(height), 0u }, VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R8G8B8A8_UNORM);
                        // ctx.CopyToImage(loadedImage, imageData, width * height * 4);
                    }

                    ctx.Transition(ctx.transferCmd, *loadedImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    // ctx.Flush(ctx.transferCmd);
                    ctx.transferCommands->Flush();
                    ctx.transferCmd = ctx.transferCommands->Allocate();

                    std::cout << "\r\x1b[KTextures loaded: " << ++texturesLoaded << " / " << data->textures_count << " [" << data->textures[textureId].image->uri << "]";
                }
                stbi_image_free(imageData);

                loadedMesh.images[textureId] = loadedImage;
            }
        }

        std::cout << "\r\x1b[KTextures loaded: " << data->textures_count << '\n';

        PYR_TIMEIT("glTF: Loaded textures");

// -----------------------------------------------------------------------------

        for (u32 nodeIdx = 0; nodeIdx < data->nodes_count; ++nodeIdx)
        {
            auto& node = data->nodes[nodeIdx];
            auto mesh = node.mesh;
            if (!mesh) // Non mesh nodes
                continue;

            mat4 tform(1.f);
            if (node.has_matrix)
            {
                std::memcpy(&tform, node.matrix, 16 * sizeof(float));
            }
            else
            {
                if (node.has_translation)
                    tform *= glm::translate(mat4(1.f), { node.translation[0], node.translation[1], node.translation[2] });
                if (node.has_rotation)
                    tform *= mat4_cast(quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]));
                if (node.has_scale)
                    tform *= glm::scale(mat4(1.f), { node.scale[0], node.scale[1], node.scale[2] });
            }

            for (u32 primitiveId = 0; primitiveId < mesh->primitives_count; ++primitiveId)
            {
                // PYR_LOG("Loading primitive, node = {}, prim = {}", nodeIdx, primitiveId);

                auto& primitive = mesh->primitives[primitiveId];

                u32 textureId = 0;
                mat3 texTransform(1.f);
                u32 targetTexcoordSet = UINT32_MAX;

                if (primitive.material)
                {
                    auto& tview = primitive.material->pbr_metallic_roughness.base_color_texture;
                    if (tview.texture)
                    {
                        textureId = u32(tview.texture - data->textures);
                        targetTexcoordSet = tview.texcoord;

                        if (tview.has_transform)
                        {
                            PYR_LOG("  Found transform, offset = ({}, {}), scale = ({}, {})",
                                tview.transform.offset[0], tview.transform.offset[1],
                                tview.transform.scale[0], tview.transform.scale[1]);
                        }
                    }
                }

                u32 vertexBaseOffset = (u32)loadedMesh.vertices.size();

                u32 currentTexcoordSet = 0;

                for (u32 attributeId = 0; attributeId < primitive.attributes_count; ++attributeId)
                {
                    auto& attribute = primitive.attributes[attributeId];
                    auto base = GltfGetBase(attribute.data);

                    if (vertexBaseOffset + attribute.data->count > loadedMesh.vertices.size())
                        loadedMesh.vertices.resize(vertexBaseOffset + attribute.data->count);

                    switch (attribute.type)
                    {
                    break;case cgltf_attribute_type_position:
                        for (u32 i = 0; i < attribute.data->count; ++i)
                        {
                            float* values = (float*)base + i * 3;
                            loadedMesh.vertices[vertexBaseOffset + i].position
                                = vec3(tform * vec4(values[0], values[1], values[2], 1.f));
                        }
                    break;case cgltf_attribute_type_texcoord:
                        if (targetTexcoordSet == currentTexcoordSet++)
                        {
                            for (u32 i = 0; i < attribute.data->count; ++i)
                            {
                                float* values = (float*)base + i * 2;
                                loadedMesh.vertices[vertexBaseOffset + i].texCoord
                                    = { values[0], values[1] };
                            }
                        }
                    }
                }

                for (u32 i = vertexBaseOffset; i < loadedMesh.vertices.size(); ++i)
                    loadedMesh.vertices[i].texIndex = textureId;

                size_t indexBaseOffset = loadedMesh.indices.size();
                auto indexBufferStart = GltfGetBase(primitive.indices);

                switch (primitive.indices->component_type)
                {
                break;case cgltf_component_type_r_16u:
                    for (u32 i = 0; i < primitive.indices->count; ++i)
                        loadedMesh.indices.push_back((u32)((u16*)indexBufferStart)[i] + vertexBaseOffset);
                break;case cgltf_component_type_r_32u:
                    loadedMesh.indices.resize(indexBaseOffset + primitive.indices->count);
                    for (u32 i = 0; i < primitive.indices->count; ++i)
                        loadedMesh.indices.push_back(((u32*)indexBufferStart)[i] + vertexBaseOffset);
                }
            }
        }

        PYR_TIMEIT("glTF: Processed vertex attributes");

        {
            std::vector<f32> summedAreas;
            summedAreas.resize(loadedMesh.vertices.size());

            auto updateNormalTangent = [&](u32 i, vec3 normal, vec3 tangent, f32 area) {
                f32 lastArea = summedAreas[i];
                summedAreas[i] += area;

                f32 lastWeight = lastArea / (lastArea + area);
                f32 newWeight = 1.f - lastWeight;

                auto& v = loadedMesh.vertices[i];
                v.normal = (lastWeight * v.normal) + (newWeight * normal);
                // v.tangent = (lastWeight * v.tangent) + (newWeight * tangent);
            };

            std::vector<b8> keepPrim(loadedMesh.indices.size() / 3);

            for (u32 i = 0; i < loadedMesh.indices.size(); i += 3)
            {
                u32 v1i = loadedMesh.indices[i + 0];
                u32 v2i = loadedMesh.indices[i + 1];
                u32 v3i = loadedMesh.indices[i + 2];

                auto& v1 = loadedMesh.vertices[v1i];
                auto& v2 = loadedMesh.vertices[v2i];
                auto& v3 = loadedMesh.vertices[v3i];

                auto v12 = v2.position - v1.position;
                auto v13 = v3.position - v1.position;

                auto u12 = v2.texCoord - v1.texCoord;
                auto u13 = v3.texCoord - v1.texCoord;

                f32 f = 1.f / (u12.x * u13.y - u13.x * u12.y);
                vec3 tangent = f * vec3 {
                    u13.y * v12.x - u12.y * v13.x,
                    u13.y * v12.y - u12.y * v13.y,
                    u13.y * v12.z - u12.y * v13.z,
                };

                auto cross = glm::cross(v12, v13);
                auto area = glm::length(0.5f * cross);
                auto normal = glm::normalize(cross);

                if (area)
                {
                    keepPrim[i / 3] = true;

                    updateNormalTangent(v1i, normal, tangent, area);
                    updateNormalTangent(v2i, normal, tangent, area);
                    updateNormalTangent(v3i, normal, tangent, area);
                }
            }

            u32 newVertexIndex = 0;
            std::vector<u32> vertexRemap(loadedMesh.vertices.size());
            auto culledVertices = std::erase_if(loadedMesh.vertices, [&](auto& v) {
                usz i = (&v - loadedMesh.vertices.data());
                b8 keep = summedAreas[i] > 0;
                if (keep)
                    vertexRemap[i] = newVertexIndex++;
                return !keep;
            });

            auto culledIndices = std::erase_if(loadedMesh.indices, [&](auto& i) {
                i = vertexRemap[i];
                return !keepPrim[(&i - loadedMesh.indices.data()) / 3];
            });

            PYR_LOG("glTF: Cleaned up mesh, removed {} indices and {} vertices", culledIndices, culledVertices);
        }

        PYR_LOG("glTF: Mesh loaded, vertices = {} triangles = {}", loadedMesh.vertices.size(), loadedMesh.indices.size() / 3);

        return std::move(loadedMesh);
    }
}
