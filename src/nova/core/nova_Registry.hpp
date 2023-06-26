#pragma once

#include "nova_Core.hpp"

namespace nova
{
    template<class Element, size_t PageSize = 256>
    class Colony
    {
        struct Page
        {
            std::array<Element, PageSize> page;
        };

        std::vector<std::unique_ptr<Page>> pages;
        size_t size = 0;

    public:
        Element& operator[](size_t index) const noexcept
        {
            size_t pageIndex = index / PageSize;
            return pages[pageIndex].page[index - (pageIndex * PageSize)];
        }

        size_t GetSize()
        {
            return size;
        }

        template<class ...Args>
        void EmplaceBack(Args&&... args)
        {
            if (size >= pages * PageSize)
                pages.emplace_back();

            return pages.back()
                .page[size++ - (pages.size() - 1) * PageSize]
                .emplace_back(std::forward<Args>(args)...);
        }

        template<class Fn>
        void ForEach(Fn&& fn)
        {
            for (u32 i = 0; i < size; ++i)
                fn((*this)[i]);
        }
    };

    template<class Element, class Key>
    struct Registry
    {
        enum class ElementFlag : uint8_t
        {
            Empty = 0,
            Exists = 1,
        };
        std::vector<ElementFlag> flags;
        // TODO: Pointer stable colony/hive
        std::vector<std::unique_ptr<Element>>  elements;
        std::vector<Key>      freelist;

        std::recursive_mutex mutex;

        Registry()
        {
            flags.push_back(ElementFlag::Empty);
            elements.emplace_back();
        }

        std::pair<Key, Element&> Acquire(bool defer = false)
        {
            std::scoped_lock lock{mutex};

            if (freelist.empty())
            {
                auto& element = elements.emplace_back(std::make_unique<Element>());
                flags.push_back(defer ? ElementFlag::Empty : ElementFlag::Exists);
                return { Key(elements.size() - 1), *element };
            }

            Key key = freelist.back();
            freelist.pop_back();
            flags[static_cast<std::underlying_type_t<Key>>(key)] = defer
                ? ElementFlag::Empty
                : ElementFlag::Exists;
            return { key, *elements[static_cast<std::underlying_type_t<Key>>(key)] };
        }

        void Return(Key key)
        {
            std::scoped_lock lock{mutex};

            flags[static_cast<std::underlying_type_t<Key>>(key)] = ElementFlag::Empty;
            freelist.push_back(key);
        }

        bool IsValid(Key key)
        {
            std::scoped_lock lock{mutex};

            return flags[static_cast<std::underlying_type_t<Key>>(key)] != ElementFlag::Empty;
        }

        void MarkExists(Key key)
        {
            std::scoped_lock lock{mutex};

            flags[static_cast<std::underlying_type_t<Key>>(key)] = ElementFlag::Exists;
        }

        NOVA_FORCE_INLINE
        Element& Get(Key key) noexcept
        {
            std::scoped_lock lock{mutex};

            return *elements[static_cast<std::underlying_type_t<Key>>(key)];
        }

        template<class Fn>
        void ForEach(Fn&& visit)
        {
            std::scoped_lock lock{mutex};

            for (uint32_t i = 0; i < elements.size(); ++i)
            {
                if (flags[i] == ElementFlag::Exists)
                {
                    visit(Key(i), *elements[i]);
                }
            }
        }

        uint32_t GetCount()
        {
            std::scoped_lock lock{mutex};

            return uint32_t(elements.size() - freelist.size());
        }
    };
}
