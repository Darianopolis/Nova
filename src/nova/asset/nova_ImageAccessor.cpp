#include "nova_ImageAccessor.hpp"

#include <nova/core/nova_Debug.hpp>

#include <stb_image.h>
#include <tinyexr.h>
#include <rdo_bc_encoder.h>
#include <rgbcx.h>
#include <cmp_core.h>

namespace nova
{
    namespace {
        usz ComputeBlockOffset(const ImageAccessor& accessor, ImagePos pos)
        {
            const auto& mip_accessor = accessor.accessors[pos.mip];

            usz offset = mip_accessor.offset;
            offset += mip_accessor.layer_pitch * pos.layer;
            offset += mip_accessor.block_row_pitch * pos.block_y;
            offset += accessor.block_pitch * pos.block_x;
            return offset;
        }
    }

    template<class ChannelT, f32 Scale, u32 ... Channels>
    void ImageBlockReadPixelIndexed(const ImageAccessor& accessor, const void* data, ImagePos pos, Block& output)
    {
        constexpr usz PixelPitch = sizeof(ChannelT) * sizeof...(Channels);

        const auto& mip_accessor = accessor.accessors[pos.mip];
        const b8* block_ptr = static_cast<const b8*>(data) + ComputeBlockOffset(accessor, pos);

        u32 max_x = std::min(4u, (pos.block_x * 4) - mip_accessor.width ) - 1;
        u32 max_y = std::min(4u, (pos.block_y * 4) - mip_accessor.height) - 1;
        for (u32 y = 0; y < 4; ++y) {
            for (u32 x = 0; x < 4; ++x) {
                const b8* pixel_ptr = block_ptr
                    + (mip_accessor.row_pitch * std::min(y, max_y))
                    + (PixelPitch * std::min(x, max_x));

                auto& out = output.Get(x, y);
                ((out[Channels] = reinterpret_cast<ChannelT*>(pixel_ptr)[Channels] * Scale), ...);
            }
        }
    }

    ImageAccessor::ImageAccessor(ImageDescription _desc)
        : desc(_desc)
    {
        // Validation

        if (desc.width == 0 || desc.height == 0 || desc.layers == 0 || desc.mips == 0) {
            NOVA_THROW("Image dimensions must be non-zero");
        }

        if (desc.mips > 16) {
            NOVA_THROW("Too many mips requested");
        }

        // Initialize accessor functions

        switch (desc.format) {
            break;case ImageFormat::BC1:
                CreateOptionsBC1(&enc_settings);
                SetAlphaThresholdBC1(enc_settings, 240);
        }

        switch (desc.format) {
            break;case ImageFormat::RGBA8:
                {
                    constexpr u32 PixelPitch = 4;
                    read_func = [](const ImageAccessor& accessor, const void* data, ImagePos pos, Block& output) {
                        const auto& mip_accessor = accessor.accessors[pos.mip];
                        const b8* block_ptr = static_cast<const b8*>(data) + ComputeBlockOffset(accessor, pos);
                        u32 max_x = std::min(4u, (pos.block_x * 4) - mip_accessor.width ) - 1;
                        u32 max_y = std::min(4u, (pos.block_y * 4) - mip_accessor.height) - 1;
                        for (u32 y = 0; y < 4; ++y) {
                            for (u32 x = 0; x < 4; ++x) {
                                const b8* pixel_ptr = block_ptr
                                    + (mip_accessor.row_pitch * std::min(y, max_y))
                                    + (PixelPitch * std::min(x, max_x));
                                output.Get(x, y) = {
                                    u8(pixel_ptr[0]) / 255.f,
                                    u8(pixel_ptr[1]) / 255.f,
                                    u8(pixel_ptr[2]) / 255.f,
                                    u8(pixel_ptr[3]) / 255.f,
                                };
                            }
                        }
                    };
                    write_func = [](const ImageAccessor& accessor, void* data, ImagePos pos, const Block& input) {
                        const auto& mip_accessor = accessor.accessors[pos.mip];
                        b8* block_ptr = static_cast<b8*>(data) + ComputeBlockOffset(accessor, pos);
                        u32 max_x = std::min(4u, (pos.block_x * 4) - mip_accessor.width ) - 1;
                        u32 max_y = std::min(4u, (pos.block_y * 4) - mip_accessor.height) - 1;
                        for (u32 y = 0; y <= max_y; ++y) {
                            for (u32 x = 0; x <= max_x; ++x) {
                                b8* pixel_ptr = block_ptr + (mip_accessor.row_pitch * y) + (PixelPitch * x);
                                const auto& source = input.Get(x, y);
                                pixel_ptr[0] = b8(u8(std::clamp(source.r, 0.f, 1.f) * 255.f));
                                pixel_ptr[1] = b8(u8(std::clamp(source.g, 0.f, 1.f) * 255.f));
                                pixel_ptr[2] = b8(u8(std::clamp(source.b, 0.f, 1.f) * 255.f));
                                pixel_ptr[3] = b8(u8(std::clamp(source.a, 0.f, 1.f) * 255.f));
                            }
                        }
                    };
                }
            break;case ImageFormat::BC1:
                  case ImageFormat::BC2:
                  case ImageFormat::BC3:
                  case ImageFormat::BC7:
                read_func = [](const ImageAccessor& accessor, const void* data, ImagePos pos, Block& output) {
                    const auto& mip_accessor = accessor.accessors[pos.mip];
                    const b8* block_ptr = static_cast<const b8*>(data) + ComputeBlockOffset(accessor, pos);
                    u8 decoded[4][4][4];
                    switch (accessor.desc.format) {
                        break;case ImageFormat::BC1:
                            DecompressBlockBC1(
                                reinterpret_cast<const unsigned char*>(block_ptr),
                                reinterpret_cast<unsigned char*>(decoded),
                                accessor.enc_settings);
                        break;case ImageFormat::BC2:
                            DecompressBlockBC2(
                                reinterpret_cast<const unsigned char*>(block_ptr),
                                reinterpret_cast<unsigned char*>(decoded),
                                accessor.enc_settings);
                        break;case ImageFormat::BC3:
                            DecompressBlockBC3(
                                reinterpret_cast<const unsigned char*>(block_ptr),
                                reinterpret_cast<unsigned char*>(decoded),
                                accessor.enc_settings);
                        break;case ImageFormat::BC7:
                            DecompressBlockBC7(
                                reinterpret_cast<const unsigned char*>(block_ptr),
                                reinterpret_cast<unsigned char*>(decoded),
                                accessor.enc_settings);
                    }
                    for (u32 y = 0; y < 4; ++y) {
                        for (u32 x = 0; x < 4; ++x) {
                            output.Get(x, y) = {
                                decoded[y][x][0] / 255.f,
                                decoded[y][x][1] / 255.f,
                                decoded[y][x][2] / 255.f,
                                decoded[y][x][3] / 255.f,
                            };
                        }
                    }
                };
                write_func = [](const ImageAccessor& accessor, void* data, ImagePos pos, const Block& input) {
                    const auto& mip_accessor = accessor.accessors[pos.mip];
                    b8* block_ptr = static_cast<b8*>(data) + ComputeBlockOffset(accessor, pos);
                    u8 to_encode[4][4][4];
                    for (u32 y = 0; y < 4; ++y) {
                        for (u32 x = 0; x < 4; ++x) {
                            const auto& source = input.Get(x, y);
                            to_encode[y][x][0] = u8(std::clamp(source.r, 0.f, 1.f) * 255.f);
                            to_encode[y][x][1] = u8(std::clamp(source.g, 0.f, 1.f) * 255.f);
                            to_encode[y][x][2] = u8(std::clamp(source.b, 0.f, 1.f) * 255.f);
                            to_encode[y][x][3] = u8(std::clamp(source.a, 0.f, 1.f) * 255.f);
                        }
                    }
                    switch (accessor.desc.format) {
                        break;case ImageFormat::BC1:
                            CompressBlockBC1(
                                reinterpret_cast<const unsigned char*>(to_encode), 16,
                                reinterpret_cast<unsigned char*>(block_ptr),
                                accessor.enc_settings);
                        break;case ImageFormat::BC2:
                            CompressBlockBC2(
                                reinterpret_cast<const unsigned char*>(to_encode), 16,
                                reinterpret_cast<unsigned char*>(block_ptr),
                                accessor.enc_settings);
                        break;case ImageFormat::BC3:
                            CompressBlockBC3(
                                reinterpret_cast<const unsigned char*>(to_encode), 16,
                                reinterpret_cast<unsigned char*>(block_ptr),
                                accessor.enc_settings);
                        break;case ImageFormat::BC7:
                            CompressBlockBC7(
                                reinterpret_cast<const unsigned char*>(to_encode), 16,
                                reinterpret_cast<unsigned char*>(block_ptr),
                                accessor.enc_settings);
                    }
                };
        }

        u32 pixel_pitch = 0;

        switch (desc.format) {
            break;case ImageFormat::RGBA8:
                pixel_pitch = 4;
            break;case ImageFormat::RGBA16_Float:
                pixel_pitch = 8;
            break;case ImageFormat::BC1:
                case ImageFormat::BC4:
                block_pitch = 8;
            break;case ImageFormat::BC2:
                case ImageFormat::BC3:
                case ImageFormat::BC5:
                case ImageFormat::BC6:
                case ImageFormat::BC7:
                block_pitch = 16;
        }

        if (pixel_pitch) {
            block_pitch = pixel_pitch * 4;
        }

        u32 width = desc.width;
        u32 height = desc.height;
        usz offset = 0;

        for (u32 i = 0; i < desc.mips; ++i) {
            auto& accessor = accessors[i];

            accessor.offset = offset;

            accessor.width = std::max(1u, width);
            accessor.height = std::max(1u, height);

            // Compute pixel / block indexing

            accessor.hblocks = (accessor.width  + 3) / 4;
            accessor.vblocks = (accessor.height + 3) / 4;

            if (pixel_pitch) {
                accessor.row_pitch = pixel_pitch * width;
                accessor.block_row_pitch = accessor.row_pitch * 4;
                accessor.layer_pitch = accessor.row_pitch * height;
            } else {
                accessor.row_pitch = 0;
                accessor.block_row_pitch = block_pitch * accessor.hblocks;
                accessor.layer_pitch = accessor.block_row_pitch * accessor.vblocks;
            }

            // Adjust for next mip

            offset += accessor.layer_pitch * desc.layers;
            width /= 2;
            height /= 2;
        }
    }

    ImageAccessor::~ImageAccessor()
    {
        switch (desc.format) {
            break;case ImageFormat::BC1:
                DestroyOptionsBC1(enc_settings);
        }
    }

    usz ImageAccessor::GetSize()
    {
        auto& last_accessor = accessors[desc.mips - 1];
        return last_accessor.offset + (last_accessor.layer_pitch + desc.layers);
    }

    void ImageAccessor::Read(const void* data, ImagePos pos, Block& block) const
    {
        read_func(*this, data, pos, block);
    }

    void ImageAccessor::Write(void* data, ImagePos pos, const Block& block) const
    {
        write_func(*this, data, pos, block);
    }

    void Image_Copy(const ImageAccessor& src, const void* src_data, const ImageAccessor& dst, void* dst_data)
    {
        if (src.desc.width != dst.desc.width
                || src.desc.height != dst.desc.height
                || src.desc.layers != dst.desc.layers
                || src.desc.mips != dst.desc.mips) {
            NOVA_THROW("Copy dimension mismatch");
        }

        for (u32 mip = 0; mip < src.desc.mips; ++mip) {
            for (u32 layer = 0; layer < src.desc.layers; ++layer) {
                const auto& mip_accessor = src.accessors[mip];
                if (mip_accessor.hblocks > mip_accessor.vblocks) {
#pragma omp parallel for
                    for (u32 x = 0; x < mip_accessor.hblocks; ++x) {
                        for (u32 y = 0; y < mip_accessor.vblocks; ++y) {
                            Block block = {};
                            src.Read(src_data, {x, y, layer, mip}, block);
                            dst.Write(dst_data, {x, y, layer, mip}, block);
                        }
                    }
                } else {
#pragma omp parallel for
                    for (u32 y = 0; y < mip_accessor.vblocks; ++y) {
                        for (u32 x = 0; x < mip_accessor.hblocks; ++x) {
                            Block block = {};
                            src.Read(src_data, {x, y, layer, mip}, block);
                            dst.Write(dst_data, {x, y, layer, mip}, block);
                        }
                    }
                }
            }
        }
    }
}