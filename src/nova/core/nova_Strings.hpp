#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Stack.hpp>
#include <nova/core/nova_Debug.hpp>

#include <fmt/format.h>
#include <simdutf.h>

namespace nova
{
    template<class CharT>
    class CString
    {
        static constexpr const CharT Empty[] { 0 };

        const CharT* data = Empty;
        bool        owned = false;

    public:
        constexpr CString() = default;

        constexpr CString(const CharT* _data, usz length, bool null_terminated)
        {
            if (null_terminated) {
                data = _data;
            } else {
                auto* cstr = new CharT[length + 1];
                std::memcpy(cstr, _data, length);
                cstr[length] = '\0';

                data = cstr;
                owned = true;
            }
        }

// -----------------------------------------------------------------------------

        CString(const CString&) = delete;
        CString& operator=(const CString&) = delete;
        CString(CString&&) = delete;
        CString& operator=(CString&&) = delete;

// -----------------------------------------------------------------------------

        constexpr operator const CharT*() const noexcept
        {
            return data;
        }

        constexpr const CharT& operator[](usz index) const noexcept
        {
            return data[index];
        }

        constexpr const CharT* const& Get() const noexcept
        {
            return data;
        }

        constexpr ~CString()
        {
            if (owned) delete[] data;
        }
    };

// -----------------------------------------------------------------------------

    // TODO: Hashing, comparison, etc..
    // TODO: One String class to rule them all with COW semantics?

    template<class CharT>
    class BasicStringView
    {
        const CharT*   data      = CString<CharT>::Empty;
        u64          length : 63 = 0;
        u64 null_terminated :  1 = false;

        constexpr void FixIfLastIsNull() noexcept
        {
            if (length > 0 && data[length - 1] == '\0') {
                null_terminated = true;
                length--;
            }
        }

// -----------------------------------------------------------------------------

    public:
        constexpr BasicStringView() noexcept
            : data(CString<CharT>::Empty)
            , length(0)
            , null_terminated(true)
        {}

// -----------------------------------------------------------------------------

        constexpr BasicStringView(const BasicStringView& other) noexcept
            : data(other.data)
            , length(other.length)
            , null_terminated(other.null_terminated)
        {}

        constexpr BasicStringView& operator=(const BasicStringView& other) noexcept
        {
            data = other.data;
            length = other.length;
            null_terminated = other.null_terminated;
            return *this;
        }

// -----------------------------------------------------------------------------

        constexpr BasicStringView(const std::basic_string<CharT>& str) noexcept
            : data(str.data())
            , length(str.size())
            , null_terminated(true)
        {}

        static constexpr usz StrLen(const CharT* str)
        {
            auto first = str;
            for (;;) {
                if (!*str) {
                    return str - first;
                }
                str++;
            }
        }

        constexpr BasicStringView(const CharT* str) noexcept
            : data(str)
            , length(StrLen(str))
            , null_terminated(true)
        {}

// -----------------------------------------------------------------------------

        constexpr BasicStringView(std::basic_string_view<CharT> str) noexcept
            : data(str.data())
            , length(str.size())
            , null_terminated(false)
        {
            FixIfLastIsNull();
        }

        constexpr BasicStringView(const CharT* str, size_t count) noexcept
            : data(str)
            , length(count)
            , null_terminated(false)
        {
            FixIfLastIsNull();
        }

        template<usz Size>
        constexpr BasicStringView(const CharT(&str)[Size]) noexcept
            : data(str)
            , length(u64(Size))
            , null_terminated(false)
        {
            FixIfLastIsNull();
        }

// -----------------------------------------------------------------------------

        constexpr bool IsNullTerminated() const noexcept
        {
            return null_terminated;
        }

        constexpr usz Size() const noexcept
        {
            return length;
        }

        constexpr const CharT* begin() const noexcept
        {
            return data;
        }

        constexpr const CharT* end() const noexcept
        {
            return data + length;
        }

        constexpr const CharT* Data() const noexcept
        {
            return data;
        }

        constexpr CString<CharT> CStr() const
        {
            return CString<CharT>(data, length, null_terminated);
        }

// -----------------------------------------------------------------------------

        constexpr explicit operator std::basic_string<CharT>() const
        {
            return std::basic_string<CharT>(data, length);
        }

        constexpr operator std::basic_string_view<CharT>() const noexcept
        {
            return std::basic_string_view<CharT>(data, length);
        }

        constexpr operator std::filesystem::path() const noexcept
        {
            return std::filesystem::path(std::basic_string_view<CharT>(*this));
        }

// -----------------------------------------------------------------------------

        constexpr bool operator==(const BasicStringView& other) const noexcept
        {
            if (length != other.length) return false;
            return std::memcmp(data, other.data, length) == 0;
        }
    };

    template<class CharT>
    std::basic_ostream<CharT, std::char_traits<CharT>>& operator<<(std::basic_ostream<CharT, std::char_traits<CharT>>& os, const BasicStringView<CharT>& str)
    {
        return os << std::basic_string_view<CharT>(str);
    }

    template<class CharT>
    struct fmt::formatter<BasicStringView<CharT>> : fmt::ostream_formatter {};

    using StringView = BasicStringView<char>;

// -----------------------------------------------------------------------------

    inline
    std::string Fmt(StringView str)
    {
        return std::string(str);
    }

    template<class ...Args>
    std::string Fmt(const fmt::format_string<Args...> fmt, Args&&... args)
    {
        return fmt::vformat(fmt.get(), fmt::make_format_args(args...));
    }

    inline
    std::wstring ToUtf16(StringView source)
    {
        std::wstring out(source.Size(), '\0');
        auto len = simdutf::convert_utf8_to_utf16(source.Data(), source.Size(), (char16_t*)out.data());
        out.resize(len);
        return out;
    }

    inline
    std::string FromUtf16(BasicStringView<wchar_t> source)
    {
        std::string out(source.Size() * 3, '\0');
        auto len = simdutf::convert_utf16_to_utf8((const char16_t*)source.Data(), source.Size(), out.data());
        out.resize(len);
        return out;
    }
};
