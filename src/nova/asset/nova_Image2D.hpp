#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Math.hpp>
#include <nova/core/nova_Containers.hpp>

namespace nova
{
    f32 SRGB_ToNonLinear(f32);
    f32 SRGB_ToLinear(f32);

    // TODO:
    //   Store images in compacted format
    //     Variable format, channel count, planar layout, encoding (e.g. storing BCn)
    //   Let user control all allocation
    //     Functions that require scratch space request space from user instead of dynamically allocating
    //     Provide allocator customization for images
    //   SIMD optimizations?

    enum class ImageFileFormat
    {
        Unknown,

        PNG,
        JPG,
        TGA,
        GIF,
        EXR,
        HDR,
        DDS,
        KTX,
    };

    ImageFileFormat ImageFileFormatFromName(std::string_view filename);

    struct Image2D
    {
        std::vector<Vec4> data;
        Vec2U           extent;

        // TODO: Loading from memory
        // TODO: Loading BCn images without a decoding step
        static std::optional<Image2D> LoadFromFile(std::string_view);
        static Image2D Create(Vec2U extent, Vec4 value = { 0.f, 0.f, 0.f, 1.f });

        NOVA_FORCE_INLINE
        usz ToIndex(Vec2U pos) const { return pos.x + pos.y * extent.x; }

        decltype(auto) operator[](this auto&& self, Vec2U pos) { return self.data[self.ToIndex(pos)]; }
        decltype(auto)        Get(this auto&& self, Vec2U pos) { return self.data[self.ToIndex(pos)]; }

        void SwizzleChannels(Span<u8> output_channels);
        void CopySwizzled(Image2D& source, Span<std::pair<u8, u8>> channel_copies);
        void ToLinear();
        void ToNonLinear();

        Image2D GenerateNextMip();
        bool CanMip() { return extent.x > 1 && extent.y > 1 && std::popcount(extent.x) == 1 && std::popcount(extent.y) == 1; }

        static
        void GenerateMipChain(std::vector<Image2D>& chain);

        std::vector<b8> ConvertToPacked(Span<u8> channels, u32 channel_size, bool to_signed, u32 row_align = 1);

        std::vector<b8> ConvertToBC1(bool use_alpha, bool is_srgb_nonlinear = true);
        std::vector<b8> ConvertToBC2(bool is_srgb_nonlinear = true);
        std::vector<b8> ConvertToBC3(bool is_srgb_nonlinear = true);
        std::vector<b8> ConvertToBC4(bool use_signed);
        std::vector<b8> ConvertToBC5(bool use_signed);
        std::vector<b8> ConvertToBC6(bool use_signed);
        std::vector<b8> ConvertToBC7(bool is_srgb_nonlinear = true, bool reduce_entropy = false);
    };
}