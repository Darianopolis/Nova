#pragma once

#include "nova_Core.hpp"

namespace nova
{
    struct JsonWriter
    {
        std::ostream&          out;
        std::string  indent_string = "  ";
        u32                  depth = 0;
        bool         first_element = true;
        bool               has_key = false;

    public:
        void Indent()
        {
            for (u32 i = 0; i < depth; ++i) {
                out << indent_string;
            }
        }

        void NewElement()
        {
            if (has_key) {
                has_key = false;
                return;
            }

            if (!first_element) {
                out << ',';
            }

            if (depth > 0) {
                out << '\n';
                Indent();
            }

            first_element = false;
        }

        JsonWriter& Key(std::string_view key)
        {
            NewElement();
            out << '"' << key << "\": ";
            has_key = true;
            return *this;
        }

        JsonWriter& operator[](std::string_view key_)
        {
            return Key(key_);
        }

        void Object()
        {
            NewElement();
            out << '{';
            ++depth;
            first_element = true;
        }

        void EndObject()
        {
            --depth;
            if (!first_element) {
                out << '\n';
                Indent();
            }
            out << '}';
            first_element = false;
        }

        void Array()
        {
            NewElement();
            out << '[';
            ++depth;
            first_element = true;
        }

        void EndArray()
        {
            --depth;
            if (!first_element) {
                out << '\n';
                Indent();
            }

            out << ']';
            first_element = false;
        }

        void String(std::string_view str)
        {
            NewElement();
            out << '"' << str << '"';
        }

        void operator=(std::string_view str) { String(str); }
        void operator=(char str)             { String({ &str, 1ull }); }

        template<typename T>
        void Value(T&& value)
        {
            NewElement();
            out << std::forward<T>(value);
        }

        void operator=(f64  value) { Value(value);                    }
        void operator=(f32  value) { Value(value);                    }
        void operator=(i64  value) { Value(value);                    }
        void operator=(i32  value) { Value(value);                    }
        void operator=(i16  value) { Value(value);                    }
        void operator=(i8   value) { Value(value);                    }
        void operator=(u64  value) { Value(value);                    }
        void operator=(u32  value) { Value(value);                    }
        void operator=(u16  value) { Value(value);                    }
        void operator=(u8   value) { Value(value);                    }
        void operator=(bool value) { Value(value ? "true" : "false"); }
        void operator=(std::nullptr_t) { Value("null");               }

        template<typename T>
        void operator<<(T&& t)
        {
            this->operator=(std::forward<T>(t));
        }
    };
}