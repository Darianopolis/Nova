#pragma once

#include "nova_Core.hpp"

namespace nova
{
    inline
    std::string ReadFileToString(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open())
            NOVA_THROW("Failed to open file: {}", filename);

        std::string output;
        output.reserve((size_t)file.tellg());
        file.seekg(0);
        output.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        return output;
    }
}