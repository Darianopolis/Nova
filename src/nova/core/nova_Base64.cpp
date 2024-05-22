#include "nova_Base64.hpp"

#include <nova/core/nova_Debug.hpp>
#include <nova/core/nova_Strings.hpp>

namespace nova::base64
{
    usz Encode(void* output, usz output_size, const void* input, usz size, bool pad, const Table& table)
    {
        usz full_triple_count    =  size      / 3;
        usz rounded_triple_count = (size + 2) / 3;
        usz remainder = size - (full_triple_count * 3);

        usz expected_size = rounded_triple_count * 4;
        if (!pad) {
            switch (remainder) {
                break;case 1: expected_size -= 2; // xx==
                break;case 2: expected_size -= 1; // xxx=
            }
        }

        if (expected_size > output_size) return expected_size;

        const u8* data = reinterpret_cast<const u8*>(input);
        char* encoded = reinterpret_cast<char*>(output);

        for (usz i = 0; i < full_triple_count; ++i) {
            usz oi = i * 4;
            usz ii = i * 3;
            encoded[oi + 0] = table.encode[                                data[ii + 0] >> 2 ];
            encoded[oi + 1] = table.encode[((data[ii + 0] & 0x03) << 4) + (data[ii + 1] >> 4)];
            encoded[oi + 2] = table.encode[((data[ii + 1] & 0x0f) << 2) + (data[ii + 2] >> 6)];
            encoded[oi + 3] = table.encode[  data[ii + 2] & 0x3f                             ];
        }

        if (remainder) {
            usz oi = full_triple_count * 4;
            usz ii = full_triple_count * 3;
            switch (remainder) {
                break;case 1:
                    encoded[oi + 0] = table.encode[                                data[ii + 0] >> 2 ];
                    encoded[oi + 1] = table.encode[(data[ii + 0] & 0x03) << 4                        ];
                    if (pad) {
                        encoded[oi + 2] = table.padding;
                        encoded[oi + 3] = table.padding;
                    }
                break;case 2:
                    encoded[oi + 0] = table.encode[                                data[ii + 0] >> 2 ];
                    encoded[oi + 1] = table.encode[((data[ii + 0] & 0x03) << 4) | (data[ii + 1] >> 4)];
                    encoded[oi + 2] = table.encode[( data[ii + 1] & 0x0f) << 2];
                    if (pad) {
                        encoded[oi + 3] = table.padding;
                    }
            }
        }

        return expected_size;
    }

    usz Decode(void* output, usz output_size, const void* input, usz size, const Table& table)
    {
        usz full_quad_count    =  size      / 4;
        usz remainder = size - (full_quad_count * 4);

        u8* data = reinterpret_cast<u8*>(output);
        const char* encoded = reinterpret_cast<const char*>(input);

        // Check for padding in last quad
        if (full_quad_count > 0) {
            const char* last_quad = encoded + (full_quad_count - 1) * 4;
            if      (table.decode[last_quad[2]] == Padding) { full_quad_count--; remainder = 2; }
            else if (table.decode[last_quad[3]] == Padding) { full_quad_count--; remainder = 3; }
        }

        // Compute size
        usz expected_size = full_quad_count * 3;
        switch (remainder) {
            break;case 2: expected_size += 1;
            break;case 3: expected_size += 2;
        }

        if (expected_size > output_size) return expected_size;

        for (usz i = 0; i < full_quad_count; ++i) {
            usz di = i * 3;
            usz ei = i * 4;
            u8 e0 = table.decode[encoded[ei + 0]];
            u8 e1 = table.decode[encoded[ei + 1]];
            u8 e2 = table.decode[encoded[ei + 2]];
            u8 e3 = table.decode[encoded[ei + 3]];
            data[di + 0] = (e0 << 2) | (e1 >> 4);
            data[di + 1] = (e1 << 4) | (e2 >> 2);
            data[di + 2] = (e2 << 6) |  e3;
        }

        if (remainder) {
            usz di = full_quad_count * 3;
            usz ei = full_quad_count * 4;
            u8 e0 = table.decode[encoded[ei + 0]];
            u8 e1 = table.decode[encoded[ei + 1]];
            u8 e2 = table.decode[encoded[ei + 2]];
                                data[di + 0] = (e0 << 2) | (e1 >> 4);
            if (remainder == 3) data[di + 1] = (e1 << 4) | (e2 >> 2);
        }

        return expected_size;
    }

    std::string Encode(Span<b8> bytes, bool pad, const Table& table)
    {
        std::string str(Encode(nullptr, 0, bytes.data(), bytes.size(), pad, table), '\0');
        Encode(str.data(), str.size(), bytes.data(), bytes.size(), pad, table);
        return str;
    }

    std::vector<b8> Decode(std::string_view encoded, const Table& table)
    {
        std::vector<b8> dec(Decode(nullptr, 0, encoded.data(), encoded.size(), table));
        Decode(dec.data(), dec.size(), encoded.data(), encoded.size(), table);
        return dec;
    }
}