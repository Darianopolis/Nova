#pragma once

#include "nova_Core.hpp"
#include "nova_Debug.hpp"

namespace nova
{
    inline
    std::string DurationToString(std::chrono::duration<double, std::nano> dur)
    {
        f64 nanos = dur.count();

        if (nanos > 1e9) {
            f64 seconds = nanos / 1e9;
            u32 decimals = 2 - u32(std::log10(seconds));
            return NOVA_FORMAT("{:.{}f}s",seconds, decimals);
        }

        if (nanos > 1e6) {
            f64 millis = nanos / 1e6;
            u32 decimals = 2 - u32(std::log10(millis));
            return NOVA_FORMAT("{:.{}f}ms", millis, decimals);
        }

        if (nanos > 1e3) {
            f64 micros = nanos / 1e3;
            u32 decimals = 2 - u32(std::log10(micros));
            return NOVA_FORMAT("{:.{}f}us", micros, decimals);
        }

        if (nanos > 0) {
            u32 decimals = 2 - u32(std::log10(nanos));
            return NOVA_FORMAT("{:.{}f}ns", nanos, decimals);
        }

        return "0";
    }

    inline
    std::string ByteSizeToString(u64 bytes)
    {
        constexpr auto Exabyte   = 1ull << 60;
        if (bytes > Exabyte) {
            f64 exabytes = bytes / f64(Exabyte);
            u32 decimals = 2 - std::min(2u, u32(std::log10(exabytes)));
            return NOVA_FORMAT("{:.{}f}EiB", exabytes, decimals);
        }

        constexpr auto Petabyte  = 1ull << 50;
        if (bytes > Petabyte) {
            f64 petabytes = bytes / f64(Petabyte);
            u32 decimals = 2 - std::min(2u, u32(std::log10(petabytes)));
            return NOVA_FORMAT("{:.{}f}PiB", petabytes, decimals);
        }

        constexpr auto Terabyte  = 1ull << 40;
        if (bytes > Terabyte) {
            f64 terabytes = bytes / f64(Terabyte);
            u32 decimals = 2 - std::min(2u, u32(std::log10(terabytes)));
            return NOVA_FORMAT("{:.{}f}TiB", terabytes, decimals);
        }

        constexpr auto Gigabyte = 1ull << 30;
        if (bytes > Gigabyte) {
            f64 gigabytes = bytes / f64(Gigabyte);
            u32 decimals = 2 - std::min(2u, u32(std::log10(gigabytes)));
            return NOVA_FORMAT("{:.{}f}GiB", gigabytes, decimals);
        }

        constexpr auto Megabyte = 1ull << 20;
        if (bytes > Megabyte) {
            f64 megabytes = bytes / f64(Megabyte);
            u32 decimals = 2 - std::min(2u, u32(std::log10(megabytes)));
            return NOVA_FORMAT("{:.{}f}MiB", megabytes, decimals);
        }

        constexpr auto Kilobyte = 1ull << 10;
        if (bytes > Kilobyte) {
            f64 kilobytes = bytes / f64(Kilobyte);
            u32 decimals = 2 - std::min(2u, u32(std::log10(kilobytes)));
            return NOVA_FORMAT("{:.{}f}KiB", kilobytes, decimals);
        }

        if (bytes > 0) {
            u32 decimals = 2 - std::min(2u, u32(std::log10(bytes)));
            return NOVA_FORMAT("{:.{}f}", f64(bytes), decimals);
        }

        return "0";
    }
}