#pragma once

#include <nova/core/nova_Math.hpp>

namespace nova
{
    enum class ImageFormat
    {
        Undefined,

        RGBA8,
        RGBA16_Float,
        BC1,
        BC2,
        BC3,
        BC4,
        BC5,
        BC6,
        BC7,
    };

    enum class ColorSpace
    {
        None,
        SRGB,
    };

    enum class TransferFunction
    {
        SRGB,
    };

    struct Block
    {
        Vec4 pixels[4][4];

        NOVA_FORCE_INLINE
        const Vec4& Get(u32 x, u32 y) const
        {
            return pixels[y][x];
        }

        NOVA_FORCE_INLINE
        Vec4& Get(u32 x, u32 y)
        {
            return pixels[y][x];
        }
    };

    struct ImagePos
    {
        u32 block_x;
        u32 block_y;
        u32 layer;
        u32 mip;
    };

    struct ImageDescription
    {
        ImageFormat format;
        bool is_signed;
        ColorSpace color_space;
        TransferFunction transfer_function;
        u32 width;
        u32 height;
        u32 layers;
        u32 mips;
    };

    struct ImageMipAccessor
    {
        u32 width;
        u32 height;

        u32 row_pitch;

        u32 hblocks;
        u32 vblocks;
        u32 block_row_pitch;
        u32 layer_pitch;

        u32 offset;
    };

    struct ImageAccessor
    {
        ImageDescription desc;

        void* enc_settings = nullptr;
        void(* read_func)(const ImageAccessor&, const void* data, ImagePos,       Block&);
        void(*write_func)(const ImageAccessor&,       void* data, ImagePos, const Block&);

        u32 block_pitch;

        ImageMipAccessor accessors[16] = {};

        ImageAccessor(ImageDescription desc);
        ~ImageAccessor();

        usz GetSize();

        void Read(const void* data, ImagePos pos, Block& block) const;
        void Write(void* data, ImagePos pos, const Block& block) const;
    };

    void Image_Copy(const ImageAccessor& src, const void* src_data, const ImageAccessor& dst, void* dst_data);
}