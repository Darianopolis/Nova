#pragma once

#include <nova/core/nova_Core.hpp>

namespace nova
{
    namespace guards
    {
        template<typename Fn>
        struct DoOnceGuard
        {
            DoOnceGuard(Fn&& fn)
            {
                fn();
            }
        };

        template<typename Fn>
        struct OnExitGuard
        {
            Fn fn;

            OnExitGuard(Fn&& _fn)
                : fn(std::move(_fn))
            {}

            ~OnExitGuard()
            {
                fn();
            }
        };
    }

#define NOVA_DO_ONCE(...) static ::nova::guards::DoOnceGuard NOVA_CONCAT(nova_do_once_, __LINE__) = [__VA_ARGS__]
#define NOVA_ON_EXIT(...) static ::nova::guards::OnExitGuard NOVA_CONCAT(nova_on_exit_, __LINE__) = [__VA_ARGS__]

// -----------------------------------------------------------------------------

    namespace guards
    {
        template<typename Fn>
        class CleanupGuard
        {
            Fn fn;

        public:
            CleanupGuard(Fn fn)
                : fn(std::move(fn))
            {}

            ~CleanupGuard()
            {
                fn();
            }
        };

        template<typename Fn>
        class DeferGuard
        {
            Fn fn;
            i32 exceptions;

        public:
            DeferGuard(Fn fn)
                : fn(std::move(fn))
                , exceptions(std::uncaught_exceptions())
            {}

            ~DeferGuard()
            {
                fn(std::uncaught_exceptions() - exceptions);
            }
        };
    }

#define NOVA_DEFER(...) ::nova::guards::DeferGuard NOVA_CONCAT(nova_defer_, __LINE__) = [__VA_ARGS__]([[maybe_unused]] ::nova::i32 exceptions)
}