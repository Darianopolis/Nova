#include "nova_Image.hpp"

#include <nova/core/nova_Debug.hpp>

#include <rdo_bc_encoder.h>
#include <rgbcx.h>
#include <cmp_core.h>
#include <encoders/bcn_common_kernel.h>

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

        template<class ChannelT, f32 Scale, bool Clamp, f32 Min, f32 Max, u32 ... Channels>
        void ImageBlockReadPacked(const ImageAccessor& accessor, const void* data, ImagePos pos, Block& output)
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
                    out = { 0.f, 0.f, 0.f, 1.f };
                    if constexpr (Clamp) {
                        ((out[Channels] = std::clamp(f32(reinterpret_cast<const ChannelT*>(pixel_ptr)[Channels]) * Scale, Min, Max)), ...);
                    } else {
                        ((out[Channels] = f32(reinterpret_cast<const ChannelT*>(pixel_ptr)[Channels]) * Scale), ...);
                    }
                }
            }
        }

        template<class ChannelT, f32 Scale, bool Clamp, f32 Min, f32 Max, u32 ... Channels>
        void ImageBlockWritePacked(const ImageAccessor& accessor, void* data, ImagePos pos, const Block& input)
        {
            constexpr usz PixelPitch = sizeof(ChannelT) * sizeof...(Channels);

            const auto& mip_accessor = accessor.accessors[pos.mip];
            b8* block_ptr = static_cast<b8*>(data) + ComputeBlockOffset(accessor, pos);
            u32 max_x = std::min(4u, (pos.block_x * 4) - mip_accessor.width ) - 1;
            u32 max_y = std::min(4u, (pos.block_y * 4) - mip_accessor.height) - 1;
            for (u32 y = 0; y <= max_y; ++y) {
                for (u32 x = 0; x <= max_x; ++x) {
                    b8* pixel_ptr = block_ptr + (mip_accessor.row_pitch * y) + (PixelPitch * x);
                    const auto& source = input.Get(x, y);
                    if constexpr (Clamp) {
                        ((reinterpret_cast<ChannelT *>(pixel_ptr)[Channels] = ChannelT(std::clamp(source[Channels], Min, Max) * Scale)), ...);
                    } else {
                        ((reinterpret_cast<ChannelT *>(pixel_ptr)[Channels] = ChannelT(source[Channels] * Scale)), ...);
                    }
                }
            }
        }

        template<class ChannelT, class FnChannelT, f32 Scale, bool Clamp, f32 Min, f32 Max, class DecompressFnT, DecompressFnT DecompressFn, u32 ... Channels>
        void ImageBlockReadBC12367(const ImageAccessor& accessor, const void* data, ImagePos pos, Block& output)
        {
            const auto& mip_accessor = accessor.accessors[pos.mip];
            const uc8* block_ptr = static_cast<const uc8*>(data) + ComputeBlockOffset(accessor, pos);
            ChannelT decoded[4][4][sizeof...(Channels)];
            DecompressFn(block_ptr, (FnChannelT*)&decoded[0][0][0], accessor.read_payload);
            for (u32 y = 0; y < 4; ++y) {
                for (u32 x = 0; x < 4; ++x) {
                    auto& out = output.Get(x, y);
                    out.a = 1.f;
                    if constexpr (Clamp) {
                        ((out[Channels] = std::clamp(f32(decoded[y][x][Channels]) * Scale, Min, Max)), ...);
                    } else {
                        ((out[Channels] =            f32(decoded[y][x][Channels]) * Scale           ), ...);
                    }
                }
            }
        }

        template<class ChannelT, class FnChannelT, f32 Scale, bool Clamp, f32 Min, f32 Max, class CompressFnT, CompressFnT CompressFn, u32 ... Channels>
        void ImageBlockWriteBC12367(const ImageAccessor& accessor, void* data, ImagePos pos, const Block& input)
        {
            const auto& mip_accessor = accessor.accessors[pos.mip];
            uc8* block_ptr = static_cast<uc8*>(data) + ComputeBlockOffset(accessor, pos);
            ChannelT decoded[4][4][sizeof...(Channels)];
            for (u32 y = 0; y < 4; ++y) {
                for (u32 x = 0; x < 4; ++x) {
                    auto& source = input.Get(x, y);
                    if constexpr (Clamp) {
                        ((decoded[y][x][Channels] = ChannelT(std::clamp(source[Channels], Min, Max) * Scale)), ...);
                    } else {
                        ((decoded[y][x][Channels] = ChannelT(           source[Channels]            * Scale)), ...);
                    }
                }
            }
            CompressFn((FnChannelT*)&decoded[0][0][0], sizeof...(Channels) * 4, block_ptr, accessor.write_payload);
        }

        template<class ChannelT, class FnChannelT, f32 Scale, bool Clamp, f32 Min, f32 Max, class DecompressFnT, DecompressFnT DecompressFn, u32 ... Channels>
        void ImageBlockReadBC45(const ImageAccessor& accessor, const void* data, ImagePos pos, Block& output)
        {
            const auto& mip_accessor = accessor.accessors[pos.mip];
            const uc8* block_ptr = static_cast<const uc8*>(data) + ComputeBlockOffset(accessor, pos);
            ChannelT decoded[sizeof...(Channels)][4][4];
            if constexpr (sizeof...(Channels) == 2) {
                DecompressFn(block_ptr, (FnChannelT*)&decoded[0][0][0], (FnChannelT*)&decoded[1][0][0], accessor.read_payload);
            } else {
                DecompressFn(block_ptr, (FnChannelT*)&decoded[0][0][0], accessor.read_payload);
            }
            for (u32 y = 0; y < 4; ++y) {
                for (u32 x = 0; x < 4; ++x) {

                    auto& out = output.Get(x, y);
                    out = { 0.f, 0.f, 0.f, 1.f };
                    if constexpr (Clamp) {
                        ((out[Channels] = std::clamp(f32(decoded[Channels][y][x]) * Scale, Min, Max)), ...);
                    } else {
                        ((out[Channels] =            f32(decoded[Channels][y][x]) * Scale           ), ...);
                    }
                }
            }
        }

        template<class ChannelT, class FnChannelT, f32 Scale, bool Clamp, f32 Min, f32 Max, class CompressFnT, CompressFnT CompressFn, u32 ... Channels>
        void ImageBlockWriteBC45(const ImageAccessor& accessor, void* data, ImagePos pos, const Block& input)
        {
            const auto& mip_accessor = accessor.accessors[pos.mip];
            uc8* block_ptr = static_cast<uc8*>(data) + ComputeBlockOffset(accessor, pos);
            ChannelT decoded[sizeof...(Channels)][4][4];
            for (u32 y = 0; y < 4; ++y) {
                for (u32 x = 0; x < 4; ++x) {
                    auto& source = input.Get(x, y);
                    if constexpr (Clamp) {
                        ((decoded[Channels][y][x] = ChannelT(std::clamp(source[Channels], Min, Max) * Scale)), ...);
                    } else {
                        ((decoded[Channels][y][x] = ChannelT(           source[Channels]            * Scale)), ...);
                    }
                }
            }
            if constexpr (sizeof...(Channels) == 2) {
                CompressFn((FnChannelT*)&decoded[0][0][0], 4, (FnChannelT*)&decoded[1][0][0], 4, block_ptr, accessor.write_payload);
            } else {
                CompressFn((FnChannelT*)&decoded[0][0][0], 4, block_ptr, accessor.write_payload);
            }
        }

        struct Float16
        {
            u16 data;

            Float16() = default;

            Float16(float value) noexcept
                : data(glm::detail::toFloat16(value))
            {}

            operator f32() const noexcept
            {
                return glm::detail::toFloat32(data);
            }
        };
    }

// -----------------------------------------------------------------------------

    ImageAccessor::ImageAccessor(ImageDescription _desc)
        : desc(_desc)
    {
        if (desc.width == 0 || desc.height == 0 || desc.layers == 0 || desc.mips == 0) {
            NOVA_THROW("Image dimensions must be non-zero");
        }

        if (desc.mips > 16) {
            NOVA_THROW("Too many mips requested");
        }

        // Initialize accessor functions

        switch (desc.format) {
            break;case ImageFormat::RGBA8:
                read_func  = &ImageBlockReadPacked <u8, 1.f / 255.f, false, 0.f, 1.f, 0, 1, 2, 3>;
                write_func = &ImageBlockWritePacked<u8,       255.f, true,  0.f, 1.f, 0, 1, 2, 3>;
            break;case ImageFormat::RGBA16_Float:
                read_func  = &ImageBlockReadPacked <Float16, 1.f, false, -FLT_MAX, FLT_MAX, 0, 1, 2, 3>;
                write_func = &ImageBlockWritePacked<Float16, 1.f, false, -FLT_MAX, FLT_MAX, 0, 1, 2, 3>;
            break;case ImageFormat::RGBA32_Float:
                read_func  = &ImageBlockReadPacked <f32, 1.f, false, -FLT_MAX, FLT_MAX, 0, 1, 2, 3>;
                write_func = &ImageBlockWritePacked<f32, 1.f, false, -FLT_MAX, FLT_MAX, 0, 1, 2, 3>;
            break;case ImageFormat::BC1:
                CreateOptionsBC1(&read_payload);
                write_payload = read_payload;
                static_cast<CMP_BC15Options*>(read_payload)->m_bUseAlpha = true;
                read_func  = &ImageBlockReadBC12367 <u8, uc8, 1.f / 255.f, false, 0.f, 1.f, decltype(&DecompressBlockBC1), &DecompressBlockBC1, 0, 1, 2, 3>;
                write_func = &ImageBlockWriteBC12367<u8, uc8,       255.f, true,  0.f, 1.f, decltype(&CompressBlockBC1),    CompressBlockBC1,   0, 1, 2, 3>;
            break;case ImageFormat::BC2:
                read_func  = &ImageBlockReadBC12367 <u8, uc8, 1.f / 255.f, false, 0.f, 1.f, decltype(&DecompressBlockBC2), &DecompressBlockBC2, 0, 1, 2, 3>;
                write_func = &ImageBlockWriteBC12367<u8, uc8,       255.f, true,  0.f, 1.f, decltype(&CompressBlockBC2),    CompressBlockBC2,   0, 1, 2, 3>;
            break;case ImageFormat::BC3:
                read_func  = &ImageBlockReadBC12367 <u8, uc8, 1.f / 255.f, false, 0.f, 1.f, decltype(&DecompressBlockBC3), &DecompressBlockBC3, 0, 1, 2, 3>;
                write_func = &ImageBlockWriteBC12367<u8, uc8,       255.f, true,  0.f, 1.f, decltype(&CompressBlockBC3),    CompressBlockBC3,   0, 1, 2, 3>;
            break;case ImageFormat::BC4:
                if (desc.is_signed) {
                    read_func  = &ImageBlockReadBC45 <i8, c8, 1.f / 127.f, true, -1.f, 1.f, decltype(&DecompressBlockBC4S), &DecompressBlockBC4S, 0>;
                    write_func = &ImageBlockWriteBC45<i8, c8,       127.f, true, -1.f, 1.f, decltype(&CompressBlockBC4S),    CompressBlockBC4S,   0>;
                } else {
                    read_func  = &ImageBlockReadBC45 <u8, uc8, 1.f / 255.f, false, 0.f, 1.f, decltype(&DecompressBlockBC4), &DecompressBlockBC4, 0>;
                    write_func = &ImageBlockWriteBC45<u8, uc8,       255.f, true,  0.f, 1.f, decltype(&CompressBlockBC4),    CompressBlockBC4,   0>;
                }
            break;case ImageFormat::BC5:
                if (desc.is_signed) {
                    read_func  = &ImageBlockReadBC45 <i8, c8, 1.f / 127.f, true, -1.f, 1.f, decltype(&DecompressBlockBC5S), &DecompressBlockBC5S, 0, 1>;
                    write_func = &ImageBlockWriteBC45<i8, c8,       127.f, true, -1.f, 1.f, decltype(&CompressBlockBC5S),    CompressBlockBC5S,   0, 1>;
                } else {
                    read_func  = &ImageBlockReadBC45 <u8, uc8, 1.f / 255.f, false, 0.f, 1.f, decltype(&DecompressBlockBC5), &DecompressBlockBC5, 0, 1>;
                    write_func = &ImageBlockWriteBC45<u8, uc8,       255.f, true,  0.f, 1.f, decltype(&CompressBlockBC5),    CompressBlockBC5,   0, 1>;
                }
            break;case ImageFormat::BC6:
                CreateOptionsBC6(&read_payload);
                write_payload = read_payload;
                SetQualityBC6(read_payload, 0.75f);
                if (desc.is_signed) {
                    SetSignedBC6(read_payload, desc.is_signed);
                    read_func  = &ImageBlockReadBC12367 <Float16, u16, 1.f, false, -FLT_MAX, FLT_MAX, decltype(&DecompressBlockBC6), &DecompressBlockBC6, 0, 1, 2>;
                    write_func = &ImageBlockWriteBC12367<Float16, u16, 1.f, false, -FLT_MAX, FLT_MAX, decltype(&CompressBlockBC6),    CompressBlockBC6,   0, 1, 2>;
                } else {
                    read_func  = &ImageBlockReadBC12367 <Float16, u16, 1.f, true, 0.f, FLT_MAX, decltype(&DecompressBlockBC6), &DecompressBlockBC6, 0, 1, 2>;
                    write_func = &ImageBlockWriteBC12367<Float16, u16, 1.f, true, 0.f, FLT_MAX, decltype(&CompressBlockBC6),    CompressBlockBC6,   0, 1, 2>;
                }
            break;case ImageFormat::BC7:
                bc7enc_compress_block_init();
                auto* params = new bc7enc_compress_block_params;
                bc7enc_compress_block_params_init(params);
                write_payload = params;

                read_func  = &ImageBlockReadBC12367 <u8, uc8, 1.f / 255.f, false, 0.f, 1.f, decltype(&DecompressBlockBC7), &DecompressBlockBC7, 0, 1, 2, 3>;
                write_func = [](const ImageAccessor& accessor, void* data, ImagePos pos, const Block& input)
                {
                    // TODO: Deduplicate this

                    const auto& mip_accessor = accessor.accessors[pos.mip];
                    uc8* block_ptr = static_cast<uc8*>(data) + ComputeBlockOffset(accessor, pos);
                    u8 decoded[4][4][4];
                    for (u32 y = 0; y < 4; ++y) {
                        for (u32 x = 0; x < 4; ++x) {
                            auto& source = input.Get(x, y);
                            decoded[y][x][0] = u8(std::clamp(source[0], 0.f, 1.f) * 255.f);
                            decoded[y][x][1] = u8(std::clamp(source[1], 0.f, 1.f) * 255.f);
                            decoded[y][x][2] = u8(std::clamp(source[2], 0.f, 1.f) * 255.f);
                            decoded[y][x][3] = u8(std::clamp(source[3], 0.f, 1.f) * 255.f);
                        }
                    }
                    bc7enc_compress_block(block_ptr, &decoded[0][0][0], (const bc7enc_compress_block_params*)accessor.write_payload);
                };
        }

        u32 pixel_pitch = 0;

        switch (desc.format) {
            break;case ImageFormat::RGBA8:
                pixel_pitch = 4;
            break;case ImageFormat::RGBA16_Float:
                pixel_pitch = 8;
            break;case ImageFormat::RGBA32_Float:
                pixel_pitch = 16;
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
                DestroyOptionsBC1(read_payload);
            break;case ImageFormat::BC6:
                DestroyOptionsBC6(read_payload);
            break;case ImageFormat::BC7:
                delete (bc7enc_compress_block_params*)write_payload;
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