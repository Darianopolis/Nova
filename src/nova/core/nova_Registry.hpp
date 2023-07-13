#pragma once

#include "nova_Core.hpp"

namespace nova
{
    template<typename Element, size_t PageSize = std::max(64ull, 4096ull / sizeof(Element))>
    class ConcurrentDynamicArray
    {
        struct Page
        {
            std::array<Element, PageSize> page;
        };

        std::shared_mutex                        mutex;
        std::list<std::unique_ptr<Page*[]>> pageTables;

        std::atomic<Page**> pageTable;
        std::atomic<usz>         size = 0;
        std::atomic<usz>     capacity = 0;
        size_t                  pages = 0;

    public:
        ConcurrentDynamicArray()
        {

        }

        ~ConcurrentDynamicArray()
        {
            for (u32 i = 0; i < pages; ++i)
                delete pageTable[i];
        }

        Element& operator[](size_t index)
        {
            size_t pageIndex = index / PageSize;
            return pageTable[pageIndex]->page[index - (pageIndex * PageSize)];
        }

        size_t GetSize()
        {
            return size;
        }

        std::pair<usz, Element&> EmplaceBackNoLock()
        {
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
                    std::memcpy(newPageTable.get(), pageTable.load(), oldPages * sizeof(void*));
                    pageTable.exchange(newPageTable.get());
                    pageTables.push_back(std::move(newPageTable));
                }

                pageTable[pageIndex] = newPage;

                capacity += PageSize;
            }

            usz index = (++size - 1);
            return { index, pageTable[pageIndex]->page[index - (pageIndex * PageSize)] };
        }

        std::pair<usz, Element&> EmplaceBack()
        {
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
                        auto newPageTable = std::make_unique<Page*[]>(pages);
                        std::memcpy(newPageTable.get(), pageTable.load(), oldPages * sizeof(void*));
                        pageTable.exchange(newPageTable.get());
                        pageTables.push_back(std::move(newPageTable));
                    }

                    pageTable[pageIndex] = newPage;

                    capacity += PageSize;
                }
            }

            usz index = (++size - 1);
            return { index, pageTable[pageIndex]->page[index - (pageIndex * PageSize)] };
        }

        template<typename Fn>
        void ForEach(Fn&& fn)
        {
            for (u32 i = 0; i < size; ++i)
                fn((*this)[i]);
        }
    };

    template<typename Element, typename Key>
    struct Registry
    {
        enum class ElementFlag : uint8_t
        {
            Empty = 0,
            Exists = 1,
        };

        ConcurrentDynamicArray<std::pair<std::atomic<ElementFlag>, Element>> elements;
        std::vector<Key>      freelist;

        std::shared_mutex mutex;

        Registry()
        {
            elements.EmplaceBack();
        }

        std::pair<Key, Element&> Acquire(bool defer = false)
        {
            std::scoped_lock lock{mutex};

            if (freelist.empty())
            {
                auto[index, entry] = elements.EmplaceBackNoLock();
                entry.first = defer ? ElementFlag::Empty : ElementFlag::Exists;
                return { Key(index), entry.second };
            }

            Key key = freelist.back();
            freelist.pop_back();
            auto& entry = elements[usz(key)];
            entry.first = defer ? ElementFlag::Empty : ElementFlag::Exists;
            return { key, entry.second };
        }

        void Return(Key key)
        {
            std::scoped_lock lock{mutex};

            elements[usz(key)].first = ElementFlag::Empty;
            freelist.push_back(key);
        }

        bool IsValid(Key key)
        {
            return elements[usz(key)].first != ElementFlag::Empty;
        }

        void MarkExists(Key key)
        {
            elements[usz(key)].first = ElementFlag::Exists;
        }

        NOVA_FORCE_INLINE
        Element& Get(Key key) noexcept
        {
            return elements[usz(key)].second;
        }

        template<typename Fn>
        void ForEach(Fn&& visit)
        {
            u64 size = elements.GetSize();
            for (u32 i = 1; i < size; ++i)
            {
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

            return u32(elements.GetSize() - freelist.size());
        }
    };
}
