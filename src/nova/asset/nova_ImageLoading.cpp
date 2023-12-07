#include "nova_Image.hpp"

#include <nova/core/nova_Stack.hpp>
#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Debug.hpp>

#include <stb_image.h>
#include <tinyexr.h>

namespace nova
{
    ImageFileFormat_ ImageFileFormatFromName(std::string_view filename)
    {
        if (filename.size() < 4) return ImageFileFormat_::Unknown;

        NOVA_STACK_POINT();
        auto copy = NOVA_STACK_TO_CSTR(filename);
        std::transform(copy, copy + filename.size(), copy, [](char c) { return char(std::toupper(c)); });
        std::string_view str = { copy, filename.size() };

        if (str.ends_with(".PNG")) return ImageFileFormat_::PNG;
        if (str.ends_with(".JPG") || str.ends_with(".JPEG"))
                                   return ImageFileFormat_::JPG;
        if (str.ends_with(".TGA")) return ImageFileFormat_::TGA;
        if (str.ends_with(".GIF")) return ImageFileFormat_::GIF;
        if (str.ends_with(".EXR")) return ImageFileFormat_::EXR;
        if (str.ends_with(".HDR")) return ImageFileFormat_::HDR;
        if (str.ends_with(".DDS")) return ImageFileFormat_::DDS;
        if (str.ends_with(".KTX")) return ImageFileFormat_::KTX;

        return ImageFileFormat_::Unknown;
    }

    void Image_Load(ImageDescription* desc, ImageLoadData* output, std::string_view filename)
    {
        if (!desc) NOVA_THROW("Expected desc");
        if (!output) NOVA_THROW("Expected output");

        NOVA_STACK_POINT();

        auto format = ImageFileFormatFromName(filename);

        switch (format) {
            break;case ImageFileFormat_::PNG:
                  case ImageFileFormat_::TGA:
                  case ImageFileFormat_::JPG:
                  case ImageFileFormat_::GIF:
                {
                    int w, h, c;
                    output->data = stbi_load(NOVA_STACK_TO_CSTR(filename), &w, &h, &c, STBI_rgb_alpha);
                    if (!output->data) NOVA_THROW("Failed to load image");

                    output->deleter = [](void* data) { stbi_image_free(data); };

                    desc->width = u32(w);
                    desc->height = u32(h);
                    desc->format = nova::ImageFormat::RGBA8;
                    desc->is_signed = false;
                    desc->layers = 1;
                    desc->mips = 1;
                }
            break;case ImageFileFormat_::EXR:
                {
                    int w, h;
                    output->data = nullptr;
                    const char* err = nullptr;
                    if (LoadEXRWithLayer((float**)&output->data, &w, &h, NOVA_STACK_TO_CSTR(filename), nullptr, &err) < 0) {
                        NOVA_THROW("Error loading EXR: {}", err ? err : "N/A");
                    }

                    output->deleter = [](void* data) { free(data); };

                    desc->width = u32(w);
                    desc->height = u32(h);
                    desc->format = nova::ImageFormat::RGBA32_Float;
                    desc->is_signed = false;
                    desc->layers = 1;
                    desc->mips = 1;
                }
            break;case ImageFileFormat_::HDR:
                {
                    int w, h, c;
                    output->data = stbi_loadf(NOVA_STACK_TO_CSTR(filename), &w, &h, &c, STBI_rgb_alpha);
                    if (!output->data) NOVA_THROW("Failed to load HDR image");

                    output->deleter = [](void* data) { stbi_image_free(data); };

                    desc->width = u32(w);
                    desc->height = u32(h);
                    desc->format = nova::ImageFormat::RGBA32_Float;
                    desc->is_signed = false;
                    desc->layers = 1;
                    desc->mips = 1;
                }
            break;case ImageFileFormat_::DDS:
                  case ImageFileFormat_::KTX:
                NOVA_THROW("TODO: Support DDS and KTX loading");
        }
    }
}