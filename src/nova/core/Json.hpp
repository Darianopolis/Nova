#pragma once

#include <iterator>
#include <utility>

struct yyjson_val;
struct yyjson_doc;

struct JsonValue;

struct JsonIterator
{
    enum class Mode
    {
        none,
        array,
        object,
        optional
    };

    size_t idx = 0;
    size_t max = 0;
    yyjson_val* val = nullptr;
    const char* key;
    Mode mode = Mode::none;

    JsonIterator(yyjson_val* range, const char* _key, bool iterate_obj = false);

    using value_type = JsonValue;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::input_iterator_tag;

    JsonIterator& operator++() noexcept;

    JsonIterator& operator++(int) noexcept
    {
        auto old = *this;
        operator++();
        return old;
    }

    bool operator==(std::default_sentinel_t) const noexcept
    {
        return idx >= max;
    }

    JsonValue operator*() const noexcept;
};

struct JsonRange {
    const char* key;
    yyjson_val* val;

    JsonIterator begin() const noexcept { return JsonIterator(val, key, true); }
    static std::default_sentinel_t end() noexcept { return std::default_sentinel; }
};

struct JsonValue
{
    const char* key;
    yyjson_val* val;

    const char* string() const noexcept;
    std::optional<uint64_t> uint64() const noexcept;
    std::optional<int64_t> int64() const noexcept;
    std::optional<double> real() const noexcept;
    bool obj() const noexcept;
    bool arr() const noexcept;
    operator bool() const noexcept;
    JsonValue operator[](const char* _key) const noexcept;
    JsonValue operator[](int index) const noexcept;
    JsonValue operator[](uint64_t index) const noexcept;
    JsonValue operator[](int64_t index) const noexcept;

    JsonRange iter() const noexcept { return JsonRange(key, val); }
    JsonIterator begin() const noexcept { return JsonIterator(val, key); }
    static std::default_sentinel_t end() noexcept { return std::default_sentinel; }
};

template<> struct std::tuple_size<JsonValue> { static constexpr size_t value = 2; };
template<> struct std::tuple_element<0, JsonValue> { using type = const char*; };
template<> struct std::tuple_element<1, JsonValue> { using type = JsonValue; };
namespace std {
    template<size_t Idx> auto get(JsonValue ptr) { if constexpr (Idx == 0) { return ptr.key; } else { return ptr; } }
}

struct JsonDocument
{
    yyjson_doc* doc = nullptr;

    JsonDocument(std::string_view json);
    ~JsonDocument();

    JsonValue root() const noexcept;
};
