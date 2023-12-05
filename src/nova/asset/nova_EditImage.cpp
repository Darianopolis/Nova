#include "nova_EditImage.hpp"

#include <nova/core/nova_Stack.hpp>
#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Debug.hpp>
#include <nova/core/nova_Math.hpp>

#include <stb_image.h>

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

    std::optional<EditImage> EditImage::LoadFromFile(std::string_view filename)
    {
        NOVA_STACK_POINT();

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

    EditImage EditImage::Create(Vec2U extent, Vec4 value)
    {
        return EditImage {
            .data = std::vector(extent.x * extent.y, value),
            .extent = extent,
        };
    }

    void EditImage::SwizzleChannels(Span<u8> output_channels)
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

    void EditImage::CopySwizzled(EditImage& source, Span<std::pair<u8, u8>> channel_copies)
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

    std::vector<b8> EditImage::ConvertToPacked(Span<u8> channels, u32 channel_size, bool to_signed, u32 row_align)
    {
        u32 pixel_size = u32(channels.size()) * channel_size;
        u32 row_pitch = AlignUpPower2(extent.x * pixel_size, row_align);
        u32 out_size = row_pitch * extent.y;

        u32 in_size = extent.x * extent.y;

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

    void EditImage::ToLinear()
    {
        u32 size = extent.x * extent.y;
        for (u32 i = 0; i < size; ++i) {
            auto v = data[i];
            data[i].r = SRGB_ToLinear(v.r);
            data[i].g = SRGB_ToLinear(v.g);
            data[i].b = SRGB_ToLinear(v.b);
        }
    }

    void EditImage::ToNonLinear()
    {
        u32 size = extent.x * extent.y;
        for (u32 i = 0; i < size; ++i) {
            auto v = data[i];
            data[i].r = SRGB_ToNonLinear(v.r);
            data[i].g = SRGB_ToNonLinear(v.g);
            data[i].b = SRGB_ToNonLinear(v.b);
        }
    }

    static
    std::vector<b8> ConvertToBCn(EditImage& image, u32 bcn_format, bool use_alpha, bool use_signed)
    {
        rgbcx::init();

        u32 hblocks = (image.extent.x + 3) / 4;
        u32 vblocks = (image.extent.y + 3) / 4;
        u32 blocks = hblocks * vblocks;
        u32 block_size = 0;

        switch (bcn_format) {
            break;case 1:
                  case 4: block_size = 8;
            break;case 3:
                  case 5:
                  case 6:
                  case 7: block_size = 16;
        }

        std::vector<b8> output(blocks * block_size);

        void* cmp_options = nullptr;
        bc7enc_compress_block_params bc7_params;

        switch (bcn_format) {
            break;case 1:
                if (use_alpha) {
                    CreateOptionsBC1(&cmp_options);
                    if (!cmp_options) NOVA_THROW("Failed to create BC1 encoder options");
                }
            break;case 4:
                if (use_signed) {
                    CreateOptionsBC4(&cmp_options);
                    if (!cmp_options) NOVA_THROW("Failed to create BC4 encoder options");
                }
            break;case 5:
                if (use_signed) {
                    CreateOptionsBC5(&cmp_options);
                    if (!cmp_options) NOVA_THROW("Failed to create BC5 encoder options");
                }
            break;case 6:
                CreateOptionsBC6(&cmp_options);
                if (!cmp_options) NOVA_THROW("Failed to create BC6 encoder options");
                SetSignedBC6(cmp_options, use_signed);
            break;case 7:
                bc7enc_compress_block_params_init(&bc7_params);
                bc7enc_compress_block_init();
        }
        NOVA_DEFER(&) {
            switch (bcn_format) {
                break;case 1: DestroyOptionsBC1(cmp_options);
                break;case 4: DestroyOptionsBC4(cmp_options);
                break;case 5: DestroyOptionsBC5(cmp_options);
                break;case 6: DestroyOptionsBC6(cmp_options);
            }
        };

#pragma omp parallel for
        for (u32 by = 0; by < vblocks; ++by) {
            for (u32 bx = 0; bx < hblocks; ++bx) {
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
                            break;default:
                                source_u8x4[dy][dx][0] = u8(std::clamp(pos.r, 0.f, 1.f) * 255.f);
                                source_u8x4[dy][dx][1] = u8(std::clamp(pos.g, 0.f, 1.f) * 255.f);
                                source_u8x4[dy][dx][2] = u8(std::clamp(pos.b, 0.f, 1.f) * 255.f);
                                source_u8x4[dy][dx][3] = u8(std::clamp(pos.a, 0.f, 1.f) * 255.f);
                        }
                    }
                }

                void* output_block = output.data() + (by * hblocks + bx) * block_size;

                switch (bcn_format) {
                    break;case 1:
                        if (use_alpha) {
                            CompressBlockBC1(
                                reinterpret_cast<const unsigned char*>(&source_u8x4), 16,
                                static_cast<unsigned char*>(output_block),
                                cmp_options);
                        } else {
                            rgbcx::encode_bc1(rgbcx::MAX_LEVEL,
                                output_block,
                                reinterpret_cast<const u8*>(&source_u8x4),
                                true, false);
                        }
                    break;case 3:
                        rgbcx::encode_bc3_hq(rgbcx::MAX_LEVEL,
                            output_block,
                            reinterpret_cast<const u8*>(&source_u8x4));
                    break;case 4:
                        if (use_signed) {
                            CompressBlockBC4S(
                                reinterpret_cast<char*>(&source_i8x1), 4,
                                reinterpret_cast<unsigned char*>(output_block),
                                cmp_options);
                        } else {
                            rgbcx::encode_bc4_hq(
                                output_block,
                                reinterpret_cast<const u8*>(&source_u8x1), 1);
                        }
                    break;case 5:
                        if (use_signed) {
                            CompressBlockBC5S(
                                reinterpret_cast<char*>(&source_i8x1), 1,
                                reinterpret_cast<char*>(&source_i8x1_2), 1,
                                reinterpret_cast<unsigned char*>(output_block),
                                cmp_options);
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
                            cmp_options);
                    break;case 7:
                        bc7enc_compress_block(
                            output_block,
                            reinterpret_cast<const void*>(&source_u8x4),
                            &bc7_params);
                }
            }
        }

        return output;
    }

    std::vector<b8> EditImage::ConvertToBC1(bool use_alpha)
    {
        return ConvertToBCn(*this, 1, use_alpha, false);
    }

    std::vector<b8> EditImage::ConvertToBC3()
    {
        return ConvertToBCn(*this, 3, true, false);
    }

    std::vector<b8> EditImage::ConvertToBC4(bool use_signed)
    {
        return ConvertToBCn(*this, 4, false, use_signed);
    }

    std::vector<b8> EditImage::ConvertToBC5(bool use_signed)
    {
        return ConvertToBCn(*this, 5, false, use_signed);
    }

    std::vector<b8> EditImage::ConvertToBC6(bool use_signed)
    {
        return ConvertToBCn(*this, 6, false, use_signed);
    }

    std::vector<b8> EditImage::ConvertToBC7(bool reduce_entropy)
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
            return ConvertToBCn(*this, 7, true, false);
        }
    }
}