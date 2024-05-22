#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Containers.hpp>

namespace nova
{
    namespace base64
    {
        static constexpr i8 Invalid = -1;
        static constexpr i8 Padding = -2;

        struct Table
        {
            char padding     = {};
            char encode[64]  = {};
            i8   decode[256] = {};

            constexpr Table(char _padding, const char (&_encode)[65])
                : padding(_padding)
            {
                for (u32 i = 0; i < 256; ++i) {
                    decode[i] = Invalid;
                }
                decode[_padding] = Padding;
                for (u32 i = 0; i < 64; ++i) {
                    encode[i] = _encode[i];
                    decode[_encode[i]] = i8(i);
                }
            }
        };

        namespace tables
        {
            // 64 characters + Padding
            constexpr static Table Default = Table('=', "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
            constexpr static Table URL     = Table('.', "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_");
        };

        usz Encode(void* output, usz output_size, const void* input, usz size, bool pad = true, const Table& table = tables::Default);
        usz Decode(void* output, usz output_size, const void* input, usz size, const Table& table = tables::Default);

        std::string Encode(Span<b8> bytes, bool pad = true, const Table& table = tables::Default);
        std::vector<b8> Decode(std::string_view encoded, const Table& table = tables::Default);
    }
}