#include <nova/core/Files.hpp>

#include "Win32.hpp"

namespace nova
{
    template<>
    struct Handle<MappedFile>::Impl
    {
        HANDLE    file = {};
        HANDLE mapping = {};
        void*   mapped = {};
        usz       size = {};

        void*     head = {};
    };

    MappedFile MappedFile::Open(StringView path, bool write)
    {
        auto impl = new Impl;
        DWORD access = GENERIC_READ;
        if (write) {
            access |= GENERIC_WRITE;
        }

        NOVA_CLEANUP_ON_EXCEPTION(&) { MappedFile(impl).Destroy(); };

        impl->file = win::Check(CreateFileW(ToUtf16(path).c_str(), access, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr), "opening file");
        {
            DWORD file_size_high;
            DWORD file_size_low = GetFileSize(impl->file, &file_size_high);
            impl->size = usz(file_size_high) << 32 | usz(file_size_low);
        }
        impl->mapping = win::Check(CreateFileMappingW(impl->file, nullptr, write ? PAGE_READWRITE : PAGE_READONLY, 0, 0, nullptr), "creating file mapping");
        impl->mapped = MapViewOfFile(impl->mapping, FILE_MAP_READ, 0, 0, 0);

        if (!impl->mapped) {
            NOVA_THROW("Failed to map file: {}", win::LastErrorString());
        }

        impl->head = impl->mapped;

        return { impl };
    }

    void MappedFile::Destroy()
    {
        if (!impl) return;

        if (impl->mapped) UnmapViewOfFile(impl->mapped);
        if (impl->mapping) CloseHandle(impl->mapping);
        if (impl->file) CloseHandle(impl->file);

        delete impl;
    }

    void* MappedFile::GetAddress() const
    {
        return impl->mapped;
    }

    usz MappedFile::GetSize() const
    {
        return impl->size;
    }

    void MappedFile::Seek(usz offset) const
    {
        impl->head = ByteOffsetPointer(impl->mapped, offset);
    }

    usz MappedFile::GetOffset() const
    {
        return ByteDistance(impl->mapped, impl->head);
    }

    void MappedFile::Write(const void* data, usz size) const
    {
        std::memcpy(impl->head, data, size);
        impl->head = ByteOffsetPointer(impl->head, size);
    }

    void MappedFile::Read(void* data, usz size) const
    {
        std::memcpy(data, impl->head, size);
        impl->head = ByteOffsetPointer(impl->head, size);
    }
}