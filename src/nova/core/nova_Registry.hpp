#pragma once

#include "nova_Core.hpp"

namespace nova
{
    template<class Element, class Key>
    struct Registry
    {
        enum class ElementFlag : uint8_t
        {
            Empty = 0,
            Exists = 1,
        };
        std::vector<ElementFlag> flags;
        std::vector<Element>  elements;
        std::vector<Key>      freelist;

        Registry()
        {
            flags.push_back(ElementFlag::Empty);
            elements.emplace_back();
        }

        std::pair<Key, Element&> Acquire()
        {
            if (freelist.empty())
            {
                auto& element = elements.emplace_back();
                flags.push_back(ElementFlag::Exists);
                return { Key(elements.size() - 1), element };
            }

            Key key = freelist.back();
            freelist.pop_back();
            flags[static_cast<std::underlying_type_t<Key>>(key)] = ElementFlag::Exists;
            return { key, Get(key) };
        }

        void Return(Key key)
        {
            flags[static_cast<std::underlying_type_t<Key>>(key)] = ElementFlag::Empty;
            freelist.push_back(key);
        }

        bool IsValid(Key key)
        {
            return flags[static_cast<std::underlying_type_t<Key>>(key)] != ElementFlag::Empty;
        }

        Element& Get(Key key)
        {
            return elements[static_cast<std::underlying_type_t<Key>>(key)];
        }

        template<class Fn>
        void ForEach(Fn&& visit)
        {
            for (uint32_t i = 0; i < elements.size(); ++i)
            {
                if (flags[i] == ElementFlag::Exists)
                    visit(Key(i), elements[i]);
            }
        }

        uint32_t GetCount()
        {
            return uint32_t(elements.size() - freelist.size());
        }
    };
}
