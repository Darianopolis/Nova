#include "nova_Image2D.hpp"

#include <nova/core/nova_Stack.hpp>
#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Debug.hpp>
#include <nova/core/nova_Math.hpp>

#include <stb_image.h>
#include <tinyexr.h>
#include <rdo_bc_encoder.h>
#include <rgbcx.h>
#include <cmp_core.h>

namespace nova
{
    f32 SRGB_ToNonLinear(f32 v)
    {
        return (v < 0.0031308f)
            ? v * 12.92f
            : (1.055f * std::pow(v, 1.f / 2.4f)) - 0.055f;
    }

    f32 SRGB_ToLinear(f32 v)
    {
        return (v < 0.4045f)
            ? v / 12.92f
            : std::pow((v + 0.055f) / 1.055f, 2.4f);
    }

    ImageFileFormat ImageFileFormatFromName(std::string_view filename)
    {
        if (filename.size() < 4) return ImageFileFormat::Unknown;

        NOVA_STACK_POINT();
        auto copy = NOVA_STACK_TO_CSTR(filename);
        std::transform(copy, copy + filename.size(), copy, [](char c) { return char(std::toupper(c)); });
        std::string_view str = { copy, filename.size() };

        if (str.ends_with(".PNG")) return ImageFileFormat::PNG;
        if (str.ends_with(".JPG") || str.ends_with(".JPEG"))
                                   return ImageFileFormat::JPG;
        if (str.ends_with(".TGA")) return ImageFileFormat::TGA;
        if (str.ends_with(".GIF")) return ImageFileFormat::GIF;
        if (str.ends_with(".EXR")) return ImageFileFormat::EXR;
        if (str.ends_with(".HDR")) return ImageFileFormat::HDR;
        if (str.ends_with(".DDS")) return ImageFileFormat::DDS;
        if (str.ends_with(".KTX")) return ImageFileFormat::KTX;

        return ImageFileFormat::Unknown;
    }

    std::optional<Image2D> Image2D::LoadFromFile(std::string_view filename)
    {
        NOVA_STACK_POINT();

        auto format = ImageFileFormatFromName(filename);

        switch (format) {
            break;case ImageFileFormat::PNG:
                  case ImageFileFormat::TGA:
                  case ImageFileFormat::JPG:
                  case ImageFileFormat::GIF:
                {
                    int w, h, c;
                    auto data = stbi_load(NOVA_STACK_TO_CSTR(filename), &w, &h, &c, STBI_rgb_alpha);
                    if (!data) return std::nullopt;
                    NOVA_DEFER(&) { stbi_image_free(data); };

                    auto output = Create({ u32(w), u32(h) });

                    u32 size = w * h;
                    for (u32 i = 0; i < size; ++i) {
                        output.data[i] = {
                            data[i * 4 + 0] / 255.f,
                            data[i * 4 + 1] / 255.f,
                            data[i * 4 + 2] / 255.f,
                            data[i * 4 + 3] / 255.f,
                        };
                    }

                    return output;
                }
            break;case ImageFileFormat::EXR:
                {
                    int w, h;
                    float* data = nullptr;
                    const char* err = nullptr;
                    if (LoadEXRWithLayer(&data, &w, &h, NOVA_STACK_TO_CSTR(filename), nullptr, &err) < 0) {
                        NOVA_THROW("Error loading EXR: {}", err ? err : "N/A");
                    }

                    NOVA_DEFER(&) { free(data); };

                    auto output = Create({ u32(w), u32(h) });
                    std::memcpy(output.data.data(), data, w * h * 4 * sizeof(f32));

                    return output;
                }
            break;case ImageFileFormat::HDR:
                {
                    int w, h, c;
                    auto data = stbi_loadf(NOVA_STACK_TO_CSTR(filename), &w, &h, &c, STBI_rgb_alpha);
                    if (!data) return std::nullopt;
                    NOVA_DEFER(&) { stbi_image_free(data); };

                    auto output = Create({ u32(w), u32(h) });
                    std::memcpy(output.data.data(), data, w * h * 4 * sizeof(f32));

                    return output;
                }
            break;case ImageFileFormat::DDS:
                  case ImageFileFormat::KTX:
                NOVA_THROW("TODO: Support DDS and KTX loading");
        }

        // TODO: Inspect file contents to determine/guess file type

        return std::nullopt;
    }

    Image2D Image2D::Create(Vec2U extent, Vec4 value)
    {
        return Image2D {
            .data = std::vector(extent.x * extent.y, value),
            .extent = extent,
        };
    }

    void Image2D::SwizzleChannels(Span<u8> output_channels)
    {
        u8 remaps[4] = { 0, 1, 2, 3 };
        for (u32 i = 0; i < output_channels.size(); ++i) {
            remaps[i] = output_channels[i];
        }

        u32 size = extent.x * extent.y;
        for (u32 i = 0; i < size; ++i) {
            auto v = data[i];
            data[i] = {
                v[remaps[0]],
                v[remaps[1]],
                v[remaps[2]],
                v[remaps[3]],
            };
        }
    }

    void Image2D::CopySwizzled(Image2D& source, Span<std::pair<u8, u8>> channel_copies)
    {
        if (extent != source.extent) {
            NOVA_THROW("Source extent does not match target extent");
        }

        u32 size = extent.x * extent.y;
        for (u32 i = 0; i < size; ++i) {
            for (auto copy : channel_copies) {
                data[i][copy.second] = data[i][copy.first];
            }
        }
    }

    std::vector<b8> Image2D::ConvertToPacked(Span<u8> channels, u32 channel_size, bool to_signed, u32 row_align)
    {
        u32 pixel_size = u32(channels.size()) * channel_size;
        u32 row_pitch = AlignUpPower2(extent.x * pixel_size, row_align);
        u32 out_size = row_pitch * extent.y;

        std::vector<b8> output(out_size);

        for (u32 y = 0; y < extent.y; ++y) {
            for (u32 x = 0; x < extent.x; ++x) {
                auto pixel_offset = (x * pixel_size) + (y * row_pitch);
                auto v = Get({ x, y });
                for (u32 out = 0; out < channels.size(); ++out) {
                    u32 in = channels[out];

                    if (to_signed) {
                        output[pixel_offset + out] = b8(i8(std::clamp(v[in], -1.f, 1.f) * 127.f));
                    } else {
                        output[pixel_offset + out] = b8(u8(std::clamp(v[in], 0.f, 1.f) * 255.f));
                    }
                }
            }
        }

        return output;
    }

    void Image2D::ToLinear()
    {
        u32 size = extent.x * extent.y;
        for (u32 i = 0; i < size; ++i) {
            auto v = data[i];
            data[i].r = SRGB_ToLinear(v.r);
            data[i].g = SRGB_ToLinear(v.g);
            data[i].b = SRGB_ToLinear(v.b);
        }
    }

    void Image2D::ToNonLinear()
    {
        u32 size = extent.x * extent.y;
        for (u32 i = 0; i < size; ++i) {
            auto v = data[i];
            data[i].r = SRGB_ToNonLinear(v.r);
            data[i].g = SRGB_ToNonLinear(v.g);
            data[i].b = SRGB_ToNonLinear(v.b);
        }
    }

    Image2D Image2D::GenerateNextMip()
    {
        if (extent.x <= 1 && extent.y <= 1) {
            NOVA_THROW("Image can have no further mips!");
        }

        if (std::popcount(extent.x) != 1 || std::popcount(extent.y) != 1) {
            NOVA_THROW("Image dimensions must both be powers of 2 for mips");
        }

        auto mip = Image2D::Create({
            std::max(1u, extent.x / 2u),
            std::max(1u, extent.y / 2u),
        });

        auto max_x = extent.x - 1;
        auto max_y = extent.y - 1;

        for (u32 y = 0; y < mip.extent.y; ++y) {
            for (u32 x = 0; x < mip.extent.x; ++x) {
                auto v0 = Get({ std::min(max_x, (x * 2)    ), std::min(max_y, (y * 2)    ) });
                auto v1 = Get({ std::min(max_x, (x * 2)    ), std::min(max_y, (y * 2) + 1) });
                auto v2 = Get({ std::min(max_x, (x * 2) + 1), std::min(max_y, (y * 2)    ) });
                auto v3 = Get({ std::min(max_x, (x * 2) + 1), std::min(max_y, (y * 2) + 1) });
                mip.Get({ x, y }) = (v0 + v1 + v2 + v3) * 0.25f;
            }
        }

        return mip;
    }

    void Image2D::GenerateMipChain(std::vector<Image2D>& chain)
    {
        if (chain.empty()) NOVA_THROW("Nothing to mip");

        while (chain.back().CanMip()) {
            chain.emplace_back(chain.back().GenerateNextMip());
        }
    }

    struct BCnSettings
    {
        u32 hblocks;
        u32 vblocks;
        u32 blocks;
        u32 block_size;

        u32 bcn_format;
        void* cmp_options = nullptr;
        bc7enc_compress_block_params bc7_params;

        BCnSettings(u32 bcn_format, Vec2U extent, bool use_alpha, bool use_signed, bool is_srgb_nonlinear)
        {
            // TODO: sRGB nonlinearity for rgcbx BC1/3 and bc7enc BC7?

            rgbcx::init();

            hblocks = (extent.x + 3) / 4;
            vblocks = (extent.y + 3) / 4;
            blocks = hblocks * vblocks;
            block_size = 0;

            switch (bcn_format) {
                break;case 1:
                    case 4: block_size = 8;
                break;case 2:
                    case 3:
                    case 5:
                    case 6:
                    case 7: block_size = 16;
                break;default:
                    NOVA_THROW("Invalid BCn format");
            }

            switch (bcn_format) {
                break;case 1:
                    if (use_alpha) {
                        CreateOptionsBC1(&cmp_options);
                        if (!cmp_options) NOVA_THROW("Failed to create BC1 encoder options");
                        SetSrgbBC1(cmp_options, is_srgb_nonlinear);
                        NOVA_LOG("Create BC1 settings");
                    }
                break;case 2:
                    CreateOptionsBC2(&cmp_options);
                    if (!cmp_options) NOVA_THROW("Failed to create BC2 encoder options");
                    SetSrgbBC2(cmp_options, is_srgb_nonlinear);
                break;case 6:
                    CreateOptionsBC6(&cmp_options);
                    if (!cmp_options) NOVA_THROW("Failed to create BC6 encoder options");
                    SetSignedBC6(cmp_options, use_signed);
                    SetQualityBC6(cmp_options, 0.75f);
                break;case 7:
                    bc7enc_compress_block_params_init(&bc7_params);
                    bc7enc_compress_block_init();
            }
        }

        ~BCnSettings()
        {
            switch (bcn_format) {
                break;case 1: DestroyOptionsBC1(cmp_options);
                break;case 2: DestroyOptionsBC2(cmp_options);
                break;case 6: DestroyOptionsBC6(cmp_options);
            }
        }
    };

    void Image2D::ReadFromBCn(u32 bcn_format, const void* bcn_data, bool use_alpha, bool use_signed, bool is_srgb_nonlinear)
    {
        BCnSettings settings(bcn_format, extent, use_alpha, use_signed, is_srgb_nonlinear);

#pragma omp parallel for
        for (u32 by = 0; by < settings.vblocks; ++by) {
            for (u32 bx = 0; bx < settings.hblocks; ++bx) {
                auto x = bx * 4, y = by * 4;

                union {
                    struct {
                        u8 source_u8x1  [4][4];
                        u8 source_u8x1_2[4][4];
                    };
                    struct {
                        i8 source_i8x1  [4][4];
                        i8 source_i8x1_2[4][4];
                    };
                    u8   source_u8x2[4][4][2];
                    u8   source_u8x4[4][4][4];
                    u16 source_f16x3[4][4][3];
                };

                const void* bcn_block = static_cast<const b8*>(bcn_data) + (by * settings.hblocks + bx) * settings.block_size;

                switch (bcn_format) {
                    break;case 1:
                        DecompressBlockBC1(
                            static_cast<const unsigned char*>(bcn_block),
                            reinterpret_cast<unsigned char*>(&source_u8x4),
                            settings.cmp_options);
                    break;case 2:
                        DecompressBlockBC2(
                            reinterpret_cast<const unsigned char*>(bcn_block),
                            reinterpret_cast<unsigned char*>(&source_u8x4),
                            settings.cmp_options);
                    break;case 3:
                        DecompressBlockBC3(
                            reinterpret_cast<const unsigned char*>(bcn_block),
                            reinterpret_cast<unsigned char*>(&source_u8x4),
                            settings.cmp_options);
                    break;case 4:
                        if (use_signed) {
                            DecompressBlockBC4S(
                                reinterpret_cast<const unsigned char*>(bcn_block),
                                reinterpret_cast<char*>(&source_i8x1),
                                settings.cmp_options);
                        } else {
                            DecompressBlockBC4(
                                reinterpret_cast<const unsigned char*>(bcn_block),
                                reinterpret_cast<unsigned char*>(&source_u8x1),
                                settings.cmp_options);
                        }
                    break;case 5:
                        if (use_signed) {
                            DecompressBlockBC5S(
                                reinterpret_cast<const unsigned char*>(bcn_block),
                                reinterpret_cast<char*>(&source_i8x1),
                                reinterpret_cast<char*>(&source_i8x1_2),
                                settings.cmp_options);
                        } else {
                            DecompressBlockBC5(
                                reinterpret_cast<const unsigned char*>(bcn_block),
                                reinterpret_cast<unsigned char*>(&source_u8x1),
                                reinterpret_cast<unsigned char*>(&source_u8x1_2),
                                settings.cmp_options);
                        }
                    break;case 6:
                        DecompressBlockBC6(
                            static_cast<const unsigned char*>(bcn_block),
                            reinterpret_cast<unsigned short*>(&source_f16x3),
                            settings.cmp_options);
                    break;case 7:
                        DecompressBlockBC7(
                            reinterpret_cast<const unsigned char*>(bcn_block),
                            reinterpret_cast<unsigned char*>(&source_u8x4),
                            settings.cmp_options);
                }

                u32 end_dx = std::min(4u, extent.x - x);
                u32 end_dy = std::min(4u, extent.y - y);

                for (u32 dy = 0; dy < end_dx; ++dy) {
                    for (u32 dx = 0; dx < end_dy; ++dx) {
                        auto& res = Get({ x + dx, y + dy });

                        switch (bcn_format) {
                            break;case 7:
                                  case 3:
                                  case 2:
                                  case 1:
                                res.r = f32(source_u8x4[dy][dx][0]) / 255.f;
                                res.g = f32(source_u8x4[dy][dx][1]) / 255.f;
                                res.b = f32(source_u8x4[dy][dx][2]) / 255.f;
                                res.a = f32(source_u8x4[dy][dx][3]) / 255.f;
                            break;case 6:
                                res.r = glm::detail::toFloat32(source_f16x3[dy][dx][0]);
                                res.g = glm::detail::toFloat32(source_f16x3[dy][dx][1]);
                                res.b = glm::detail::toFloat32(source_f16x3[dy][dx][2]);
                                res.a = 1.f;
                            break;case 5:
                                if (use_signed) {
                                    res.r = std::clamp(f32(source_i8x1  [dy][dx]) / 127.f, -1.f, 1.f);
                                    res.g = std::clamp(f32(source_i8x1_2[dy][dx]) / 127.f, -1.f, 1.f);
                                } else {
                                    res.r = f32(source_u8x1  [dy][dx]) / 255.f;
                                    res.g = f32(source_u8x1_2[dy][dx]) / 255.f;
                                }
                                res.b = 0.f;
                                res.a = 1.f;
                            break;case 4:
                                if (use_signed) {
                                    res.r = std::clamp(f32(source_i8x1  [dy][dx]) / 127.f, -1.f, 1.f);
                                } else {
                                    res.r = f32(source_u8x1  [dy][dx]) / 255.f;
                                }
                                res.g = 0.f;
                                res.b = 0.f;
                                res.a = 1.f;
                        }
                    }
                }
            }
        }
    }

    static
    std::vector<b8> ConvertToBCn(Image2D& image, u32 bcn_format, bool use_alpha, bool use_signed, bool is_srgb_nonlinear)
    {
        BCnSettings settings(bcn_format, image.extent, use_alpha, use_signed, is_srgb_nonlinear);

        std::vector<b8> output(settings.blocks * settings.block_size);

#pragma omp parallel for
        for (u32 by = 0; by < settings.vblocks; ++by) {
            for (u32 bx = 0; bx < settings.hblocks; ++bx) {
                auto x = bx * 4, y = by * 4;

                union {
                    struct {
                        u8 source_u8x1  [4][4];
                        u8 source_u8x1_2[4][4];
                    };
                    struct {
                        i8 source_i8x1  [4][4];
                        i8 source_i8x1_2[4][4];
                    };
                    u8   source_u8x2[4][4][2];
                    u8   source_u8x4[4][4][4];
                    u16 source_f16x3[4][4][3];
                };

                for (u32 dy = 0; dy < 4; ++dy) {
                    for (u32 dx = 0; dx < 4; ++dx) {
                        auto pos = image.Get({
                            std::min(x + dx, image.extent.x - 1),
                            std::min(y + dy, image.extent.y - 1),
                        });

                        switch (bcn_format) {
                            break;case 7:
                                  case 3:
                                  case 2:
                                  case 1:
                                source_u8x4[dy][dx][0] = u8(std::clamp(pos.r, 0.f, 1.f) * 255.f);
                                source_u8x4[dy][dx][1] = u8(std::clamp(pos.g, 0.f, 1.f) * 255.f);
                                source_u8x4[dy][dx][2] = u8(std::clamp(pos.b, 0.f, 1.f) * 255.f);
                                source_u8x4[dy][dx][3] = u8(std::clamp(pos.a, 0.f, 1.f) * 255.f);
                                // source_u8x4[dy][dx][3] = 0;
                            break;case 6:
                                source_f16x3[dy][dx][0] = glm::detail::toFloat16(pos.r);
                                source_f16x3[dy][dx][1] = glm::detail::toFloat16(pos.g);
                                source_f16x3[dy][dx][2] = glm::detail::toFloat16(pos.b);
                            break;case 5:
                                if (use_signed) {
                                    source_i8x1  [dy][dx] = i8(std::clamp(pos.r, -1.f, 1.f) * 127.f);
                                    source_i8x1_2[dy][dx] = i8(std::clamp(pos.g, -1.f, 1.f) * 127.f);
                                } else {
                                    source_u8x2[dy][dx][0] = u8(pos.r * 255.f);
                                    source_u8x2[dy][dx][1] = u8(pos.g * 255.f);
                                }
                            break;case 4:
                                if (use_signed) {
                                    source_i8x1[dy][dx] = i8(std::clamp(pos.r, -1.f, 1.f) * 127.f);
                                } else {
                                    source_u8x1[dy][dx] = u8(std::clamp(pos.r, 0.f, 1.f) * 255.f);
                                }
                        }
                    }
                }

                void* output_block = output.data() + (by * settings.hblocks + bx) * settings.block_size;

                switch (bcn_format) {
                    break;case 1:
                        if (use_alpha) {
                            CompressBlockBC1(
                                reinterpret_cast<const unsigned char*>(&source_u8x4), 16,
                                static_cast<unsigned char*>(output_block),
                                settings.cmp_options);
                        } else {
                            // rgbcx::encode_bc1(rgbcx::MAX_LEVEL,
                            //     output_block,
                            //     reinterpret_cast<const u8*>(&source_u8x4),
                            //     true, false);
                        }
                    break;case 2:
                        CompressBlockBC2(
                            reinterpret_cast<const unsigned char*>(&source_u8x4), 16,
                            reinterpret_cast<unsigned char*>(output_block),
                            settings.cmp_options);
                    break;case 3:
                        rgbcx::encode_bc3_hq(rgbcx::MAX_LEVEL,
                            output_block,
                            reinterpret_cast<const u8*>(&source_u8x4));
                    break;case 4:
                        if (use_signed) {
                            CompressBlockBC4S(
                                reinterpret_cast<const char*>(&source_i8x1), 4,
                                reinterpret_cast<unsigned char*>(output_block),
                                settings.cmp_options);
                        } else {
                            rgbcx::encode_bc4_hq(
                                output_block,
                                reinterpret_cast<const u8*>(&source_u8x1), 1);
                        }
                    break;case 5:
                        if (use_signed) {
                            CompressBlockBC5S(
                                reinterpret_cast<const char*>(&source_i8x1), 1,
                                reinterpret_cast<const char*>(&source_i8x1_2), 1,
                                reinterpret_cast<unsigned char*>(output_block),
                                settings.cmp_options);
                        } else {
                            rgbcx::encode_bc5_hq(
                                output_block,
                                reinterpret_cast<const u8*>(&source_u8x2),
                                0, 1, 2);
                        }
                    break;case 6:
                        CompressBlockBC6(
                            reinterpret_cast<const unsigned short*>(&source_f16x3), 12,
                            static_cast<unsigned char*>(output_block),
                            settings.cmp_options);
                    break;case 7:
                        bc7enc_compress_block(
                            output_block,
                            reinterpret_cast<const void*>(&source_u8x4),
                            &settings.bc7_params);
                }
            }
        }

        return output;
    }

    std::vector<b8> Image2D::ConvertToBC1(bool use_alpha, bool is_srgb_nonlinear)
    {
        return ConvertToBCn(*this, 1, use_alpha, false, is_srgb_nonlinear);
    }

    std::vector<b8> Image2D::ConvertToBC2(bool is_srgb_nonlinear)
    {
        return ConvertToBCn(*this, 2, false, false, is_srgb_nonlinear);
    }

    std::vector<b8> Image2D::ConvertToBC3(bool is_srgb_nonlinear)
    {
        return ConvertToBCn(*this, 3, false, false, is_srgb_nonlinear);
    }

    std::vector<b8> Image2D::ConvertToBC4(bool use_signed)
    {
        return ConvertToBCn(*this, 4, false, use_signed, false);
    }

    std::vector<b8> Image2D::ConvertToBC5(bool use_signed)
    {
        return ConvertToBCn(*this, 5, false, use_signed, false);
    }

    std::vector<b8> Image2D::ConvertToBC6(bool use_signed)
    {
        return ConvertToBCn(*this, 6, false, use_signed, false);
    }

    std::vector<b8> Image2D::ConvertToBC7(bool is_srgb_nonlinear, bool reduce_entropy)
    {
        if (reduce_entropy) {
            auto data = ConvertToPacked({ 0, 1, 2, 3 }, 1, false);

            utils::image_u8 src_image{ extent.x, extent.y };
            std::memcpy(src_image.get_pixels().data(), data.data(), data.size());

            rdo_bc::rdo_bc_params params;
            params.m_bc7enc_reduce_entropy = true;

            rdo_bc::rdo_bc_encoder encoder;
            encoder.init(src_image, params);
            encoder.encode();

            data.resize(encoder.get_total_blocks_size_in_bytes());
            std::memcpy(data.data(), encoder.get_blocks(), data.size());

            return data;
        } else {
            return ConvertToBCn(*this, 7, false, false, is_srgb_nonlinear);
        }
    }
}