#include "nova_Image.hpp"

#include <nova/core/nova_Stack.hpp>
#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Debug.hpp>
#include <nova/core/nova_Files.hpp>

#include <stb_image.h>
#include <tinyexr.h>
#include <dds-ktx.h>

namespace nova
{
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

    void Image_Load(ImageDescription* desc, ImageLoadData* output, std::string_view filename)
    {
        if (!desc) NOVA_THROW("Expected desc");
        if (!output) NOVA_THROW("Expected output");

        NOVA_STACK_POINT();

        auto format = ImageFileFormatFromName(filename);

        switch (format) {
            break;case ImageFileFormat::PNG:
                  case ImageFileFormat::TGA:
                  case ImageFileFormat::JPG:
                  case ImageFileFormat::GIF:
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
            break;case ImageFileFormat::EXR:
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
            break;case ImageFileFormat::HDR:
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
            break;case ImageFileFormat::DDS:
                  case ImageFileFormat::KTX:
                {
                    output->deleter = [](void* data) { free(data); };

                    {
                        std::ifstream file(NOVA_STACK_TO_CSTR(filename), std::ios::ate | std::ios::binary);
                        output->size = file.tellg();
                        file.seekg(0);
                        output->data = malloc(output->size);
                        file.read((char*)output->data, output->size);
                    }

                    ddsktx_texture_info info = {};
                    ddsktx_error error = {};
                    if (!ddsktx_parse(&info, output->data, output->size, &error)) {
                        NOVA_THROW("Error parsing DDS/KTX file: {:256s}", error.msg);
                    }

                    switch (info.format) {
                        break;case DDSKTX_FORMAT_RGBA8:
                            desc->format = nova::ImageFormat::RGBA8;
                        break;case DDSKTX_FORMAT_RGBA16F:
                            desc->format = nova::ImageFormat::RGBA16_Float;
                        break;case DDSKTX_FORMAT_BC1:
                            desc->format = nova::ImageFormat::BC1;
                        break;case DDSKTX_FORMAT_BC2:
                            desc->format = nova::ImageFormat::BC2;
                        break;case DDSKTX_FORMAT_BC3:
                            desc->format = nova::ImageFormat::BC3;
                        break;case DDSKTX_FORMAT_BC4:
                            desc->format = nova::ImageFormat::BC4;
                        break;case DDSKTX_FORMAT_BC5:
                            desc->format = nova::ImageFormat::BC5;
                        break;case DDSKTX_FORMAT_BC6H:
                            desc->format = nova::ImageFormat::BC6;
                        break;case DDSKTX_FORMAT_BC7:
                            desc->format = nova::ImageFormat::BC7;
                        break;default:
                            NOVA_THROW("Unsupported DDS/KTX format: {}", int(info.format));
                    }

                    desc->width = u32(info.width);
                    desc->height = u32(info.height);
                    desc->is_signed = false;
                    desc->layers = u32(info.num_layers);
                    desc->mips = u32(info.num_mips);

                    NOVA_LOG("Loaded data = {}, size = {}, offset = {}", output->data, output->size, info.data_offset);
                    NOVA_LOG("  width = {}, height = {}, layers = {}, mips = {}", info.width, info.height, info.num_layers, info.num_mips);

                    output->data = (b8*)output->data + info.data_offset;
                    output->offset = usz(info.data_offset);
                }
        }
    }
}