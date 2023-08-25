#include "nova_Files.hpp"

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
}