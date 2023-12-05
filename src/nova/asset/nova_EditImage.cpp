#include "nova_EditImage.hpp"

#include <nova/core/nova_Stack.hpp>
#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Debug.hpp>
#include <nova/core/nova_Math.hpp>

#include <stb_image.h>

#include <rdo_bc_encoder.h>

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

    std::vector<b8> EditImage::ConvertToUnorm(Span<u8> channels, u32 channel_size, u32 row_align)
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

                    output[pixel_offset + out] = b8(u8(v[in] * 255.f));
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

    std::vector<b8> EditImage::ConvertToBC7()
    {
        auto data = ConvertToUnorm({ 0, 1, 2, 3 }, 1);

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
    }
}