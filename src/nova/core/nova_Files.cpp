#include "nova_Files.hpp"

#include "nova_Debug.hpp"
#include "nova_Stack.hpp"

namespace nova
{
    File::File(const char* path, bool write)
    {
        if (fopen_s(&file, path, write ? "wb" : "rb")) {
            NOVA_THROW("Failed to open file: {}", path);
        }
    }

    File::~File()
    {
        fflush(file);
        fclose(file);
    }

    void File::Read(void* out, size_t bytes)
    {
        fread(out, 1, bytes, file);
    }

    void File::Write(const void* in, size_t bytes)
    {
        fwrite(in, 1, bytes, file);
    }

    void File::Seek(int64_t offset, Position location)
    {
        _fseeki64(file, offset, int(location));
    }

    int64_t File::GetOffset()
    {
        return ftell(file);
    }

// -----------------------------------------------------------------------------

    std::vector<char> files::ReadBinaryFile(StringView filename)
    {
        std::ifstream file(filename.CStr(), std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            NOVA_THROW("Failed to open file: [{}]", filename);
        }

        auto file_size = size_t(file.tellg());
        std::vector<char> buffer(file_size);

        file.seekg(0);
        file.read(buffer.data(), file_size);

        file.close();
        return buffer;
    }

    std::string files::ReadTextFile(StringView filename)
    {
        std::ifstream file(filename.CStr(), std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            NOVA_THROW("Failed to open file: [{}]", filename);
        }

        std::string output;
        output.reserve((size_t)file.tellg());
        file.seekg(0);
        output.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        return output;
    }
}