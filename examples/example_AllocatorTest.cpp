#include "main/example_Main.hpp"

#include <nova/core/win32/nova_Win32.hpp>

NOVA_EXAMPLE(AllocTest, "alloc")
{
    using namespace std::chrono;

    constexpr u64 Count = 100'000'000;
    constexpr u64 MaxAllocs  = 10'000;

    u32 num_threads;
    if (args.empty() || std::from_chars(args[0].Data(), args[0].Data() + args[0].Size(), num_threads).ec != std::errc{}) {
        NOVA_THROW_STACKLESS("Usage: <num threads>");
    }

    constexpr std::array SizeBins {
        8, 16, 32, 64,
        8, 16, 32, 64,
        8, 16, 32, 64,
        8, 16, 32, 64,
        128, 256, 512, 1024,
        128, 256, 512, 1024,
        2048, 4096, 8192
    };

    std::atomic<u64> total_allocs = 0;
    std::atomic<u64> total_frees = 0;
    std::atomic<f64> cpu_seconds = 0;
    std::atomic<f64> user_seconds = 0;

    std::vector<std::jthread> alloc_threads;
    alloc_threads.reserve(num_threads);

    auto wall_start = steady_clock::now();

    for (u32 i = 0; i < num_threads; ++i) {
        alloc_threads.emplace_back([&, thread_idx = i] {
            u64 local_allocs = 0;
            u64 local_frees = 0;

            u32 num_allocs = 0;
            std::array<void*, MaxAllocs> allocations;

            std::mt19937 rng{std::random_device{}()};
            std::uniform_int_distribution<u32> size_bin_dist{0, u32(SizeBins.size()) - 1};

            auto start = steady_clock::now();

            for (u64 i = 0; i < Count; ++i) {
                if (num_allocs < MaxAllocs) {
                    local_allocs++;
                    allocations[num_allocs++] = nova::Alloc(SizeBins[size_bin_dist(rng)]);
                } else {
                    local_frees++;
                    u32 to_free = std::uniform_int_distribution<u32>(0, --num_allocs)(rng);
                    nova::Free(allocations[to_free]);
                    allocations[to_free] = allocations[num_allocs];
                }
            }

            auto end = steady_clock::now();

            total_allocs += local_allocs;
            total_frees += local_frees;
            cpu_seconds += duration_cast<duration<f64>>(end - start).count();

            FILETIME creation_time, exit_time, kernel_time, user_time;
            GetThreadTimes(alloc_threads[thread_idx].native_handle(), &creation_time, &exit_time, &kernel_time, &user_time);

            auto user_time_int = std::bit_cast<ULARGE_INTEGER>(user_time);
            auto kernel_time_int = std::bit_cast<ULARGE_INTEGER>(kernel_time);
            user_seconds += (f64(user_time_int.QuadPart) + f64(kernel_time_int.QuadPart)) * (100.0 / 1'000'000'000.0);
        });
    }
    for (auto& t : alloc_threads) t.join();

    auto wall_end = steady_clock::now();

    auto wall_seconds = duration_cast<duration<f64>>(wall_end - wall_start).count();
    auto wall_allocs_per_second = total_allocs / wall_seconds;

    auto TimeStr = [](f64 seconds) {
        return nova::DurationToString(duration<f64>(seconds));
    };

    nova::Log("{}/{} allocations/frees in {}", total_allocs.load(), total_frees.load(), TimeStr(wall_seconds));
    nova::Log("{} alloc/s", wall_allocs_per_second);
    nova::Log("per thread wall time: ({}) / ({} total)", TimeStr(cpu_seconds / total_allocs), TimeStr(cpu_seconds));
    nova::Log("per thread user time: ({}) / ({} total)", TimeStr(user_seconds / total_allocs), TimeStr(user_seconds));
}