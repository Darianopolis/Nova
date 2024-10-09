#pragma once

#include "Core.hpp"

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
        File(StringView path, bool write = false)
        {
            if (fopen_s(&file, path.CStr(), write ? "wb" : "rb")) {
                NOVA_THROW("Failed to open file: {}", path);
            }
        }

        ~File()
        {
            fflush(file);
            fclose(file);
        }

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

        void Read(void* out, size_t bytes)
        {
            fread(out, 1, bytes, file);
        }

        void Write(const void* in, size_t bytes)
        {
            fwrite(in, 1, bytes, file);
        }

        void Seek(int64_t offset, Position location = Start)
        {
            _fseeki64(file, offset, int(location));
        }

        int64_t GetOffset()
        {
            return ftell(file);
        }
    };

    namespace files {
        inline
        std::vector<char> ReadBinaryFile(StringView filename)
        {
            // TODO: Result instead of exception

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

        inline
        std::string ReadTextFile(StringView filename)
        {
            // TODO: Result instead of exception

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

    struct MappedFile : Handle<MappedFile>
    {
        static MappedFile Open(StringView path, bool write = false);
        void Destroy();

        void* GetAddress() const;
        usz GetSize() const;

        void Seek(usz offset) const;
        usz GetOffset() const;

        void Write(const void* data, usz size) const;
        void Read(void* data, usz size) const;

        template<typename T>
        T Read() const
        {
            union {
                std::monostate ms;
                T t;
            };
            Read(&t, sizeof(T));
            return t;
        }

        template<typename T>
        void Read(T& t) const
        {
            Read(reinterpret_cast<char*>(&t), sizeof(T));
        }

        template<typename T>
        void Write(const T& t) const
        {
            Write(reinterpret_cast<const char*>(&t), sizeof(T));
        }
    };

    namespace streams
    {
        template<typename T>
        void Write(std::ostream& os, const T& t)
        {
            os.write(reinterpret_cast<const char*>(&t), sizeof(t));
        }

        template<typename T>
        T Read(std::istream& os)
        {
            union {
                T t;
            };
            os.read(reinterpret_cast<char*>(&t), sizeof(T));
            return t;
        }
    }
}