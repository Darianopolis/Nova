#pragma once

#include "nova_Core.hpp"

namespace nova
{
    template<class Element, size_t PageSize = std::max(64ull, 4096ull / sizeof(Element))>
    class ConcurrentDynamicArray
    {
        struct Page
        {
            std::array<Element, PageSize> page;
        };

        std::shared_mutex mutex;
        std::list<std::unique_ptr<Page*[]>> pageTables;

        std::atomic<Page**> page;
        std::atomic<usz> size = 0;
        std::atomic<usz> capacity = 0;
        size_t pages = 0;

    public:
        Element& operator[](size_t index)
        {
            // std::scoped_lock lock{mutex};
            size_t pageIndex = index / PageSize;
            return page[pageIndex]->page[index - (pageIndex * PageSize)];
        }

        size_t GetSize()
        {
            // std::scoped_lock lock{mutex};
            return size;
        }

        std::pair<usz, Element&> EmplaceBackNoLock()
        {
            // std::scoped_lock lock{mutex};
            size_t pageIndex = size / PageSize;

            if (size >= capacity)
            {
                auto newPage = new Page{};
                if (pageIndex >= pages)
                {
                    auto oldPages = pages;
                    pages = std::max(16ull, pages * 2);
                    NOVA_LOG("Page index table expanded from {} to {}", oldPages, pages);
                    auto newPageTable = std::make_unique<Page*[]>(pages);
                    std::memcpy(newPageTable.get(), page.load(), oldPages * sizeof(void*));
                    page.exchange(newPageTable.get());
                    pageTables.push_back(std::move(newPageTable));
                }

                page[pageIndex] = newPage;

                capacity += PageSize;
            }

            usz index = (++size - 1);
            return { index, page[pageIndex]->page[index - (pageIndex * PageSize)] };
        }

        std::pair<usz, Element&> EmplaceBack()
        {
            // std::scoped_lock lock{mutex};
            size_t pageIndex = size / PageSize;

            if (size >= capacity)
            {
                std::scoped_lock lock{mutex};

                if (size >= capacity)
                {
                    auto newPage = new Page{};
                    if (pageIndex >= pages)
                    {
                        auto oldPages = pages;
                        pages = std::max(16ull, pages * 2);
                        NOVA_LOG("Page index table expanded from {} to {}", oldPages, pages);
                        auto newPageTable = std::make_unique<Page*[]>(pages);
                        std::memcpy(newPageTable.get(), page.load(), oldPages * sizeof(void*));
                        page.exchange(newPageTable.get());
                        pageTables.push_back(std::move(newPageTable));
                    }

                    page[pageIndex] = newPage;

                    capacity += PageSize;
                    // NOVA_LOG("  Capacity {} -> {}", capacity - PageSize, capacity.load());
                }
            }

            usz index = (++size - 1);
            return { index, page[pageIndex]->page[index - (pageIndex * PageSize)] };
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
        // std::vector<ElementFlag> flags;
        // TODO: Pointer stable colony/hive
        // std::vector<std::unique_ptr<Element>>  elements;
        ConcurrentDynamicArray<std::pair<std::atomic<ElementFlag>, Element>> elements;
        std::vector<Key>      freelist;

        std::shared_mutex mutex;

        Registry()
        {
            // flags.push_back(ElementFlag::Empty);
            // elements.emplace_back();
            elements.EmplaceBack();
        }

        std::pair<Key, Element&> Acquire(bool defer = false)
        {
            std::scoped_lock lock{mutex};

            if (freelist.empty())
            {
                // auto& element = elements.emplace_back(std::make_unique<Element>());
                // flags.push_back(defer ? ElementFlag::Empty : ElementFlag::Exists);
                // return { Key(elements.size() - 1), *element };

                auto[index, entry] = elements.EmplaceBackNoLock();
                entry.first = defer ? ElementFlag::Empty : ElementFlag::Exists;
                return { Key(index), entry.second };
            }

            // Key key = freelist.back();
            // freelist.pop_back();
            // flags[static_cast<std::underlying_type_t<Key>>(key)] = defer
            //     ? ElementFlag::Empty
            //     : ElementFlag::Exists;
            // return { key, *elements[static_cast<std::underlying_type_t<Key>>(key)] };

            Key key = freelist.back();
            freelist.pop_back();
            auto& entry = elements[usz(key)];
            entry.first = defer ? ElementFlag::Empty : ElementFlag::Exists;
            return { key, entry.second };
        }

        void Return(Key key)
        {
            std::scoped_lock lock{mutex};

            // flags[static_cast<std::underlying_type_t<Key>>(key)] = ElementFlag::Empty;
            elements[usz(key)].first = ElementFlag::Empty;
            freelist.push_back(key);
        }

        bool IsValid(Key key)
        {
            // std::scoped_lock lock{mutex};

            // return flags[static_cast<std::underlying_type_t<Key>>(key)] != ElementFlag::Empty;
            return elements[usz(key)].first != ElementFlag::Empty;
        }

        void MarkExists(Key key)
        {
            // std::scoped_lock lock{mutex};

            // flags[static_cast<std::underlying_type_t<Key>>(key)] = ElementFlag::Exists;
            elements[usz(key)].first = ElementFlag::Exists;
        }

        NOVA_FORCE_INLINE
        Element& Get(Key key) noexcept
        {
            // std::scoped_lock lock{mutex};

            // return *elements[static_cast<std::underlying_type_t<Key>>(key)];
            return elements[usz(key)].second;
        }

        template<class Fn>
        void ForEach(Fn&& visit)
        {
            // std::scoped_lock lock{mutex};

            u64 size = elements.GetSize();
            // for (uint32_t i = 0; i < elements.size(); ++i)
            for (u32 i = 1; i < size; ++i)
            {
                // if (flags[i] == ElementFlag::Exists)

                // TODO: This is a race condition
                //   Return after visit yield dangling reference
                if (IsValid(Key(i)))
                {
                    visit(Key(i), elements[i].second);
                }
            }
        }

        uint32_t GetCount()
        {
            std::scoped_lock lock{mutex};

            // return uint32_t(elements.size() - freelist.size());
            return u32(elements.GetSize() - freelist.size());
        }
    };
}
