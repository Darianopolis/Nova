#include "example_Main.hpp"

#include <nova/core/nova_Stack.hpp>
#include <nova/core/nova_Timer.hpp>
#include <nova/core/nova_Debug.hpp>

#include <stdio.h>

thread_local volatile size_t Size;

void foo(std::string_view str)
{
    NOVA_STACK_POINT();

    auto wstr = NOVA_STACK_TO_UTF16(str);
    Size = wstr.size();
}

NOVA_EXAMPLE(Stack, "stack")
{
#pragma omp parallel for
    for (u32 i = 0; i < 16; ++i) {
        NOVA_TIMEIT_RESET();
        for (u32 j = 0; j < 10'000'000; ++j) {
            foo(R"(Hello, world!)");
        }
        NOVA_TIMEIT();
    }
}