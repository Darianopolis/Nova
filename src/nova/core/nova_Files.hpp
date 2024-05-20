#pragma once

#include "nova_Core.hpp"
#include "nova_Strings.hpp"

namespace nova
{
    class File
    {
    public:
        enum class Position : int
        {
            Start = SEEK_SET,
            Current = SEEK_CUR,
            End = SEEK_END,
        };
        using enum Position;

    public:
        FILE* file;

    public:
        File(const char* path, bool write = false);
        ~File();

        template<typename T>
        T Read()
        {
            T t;
            Read(&t, sizeof(T));
            return t;
        }

        template<typename T>
        void Read(T& t)
        {
            Read(reinterpret_cast<char*>(&t), sizeof(T));
        }

        template<typename T>
        void Write(const T& t)
        {
            Write(reinterpret_cast<const char*>(&t), sizeof(T));
        }

        void Read(void* out, size_t bytes);

        void Write(const void* in, size_t bytes);

        void Seek(int64_t offset, Position location = Start);

        int64_t GetOffset();
    };

    namespace files {
        std::vector<char> ReadBinaryFile(StringView filename);
        std::string ReadTextFile(StringView filename);
    }
}