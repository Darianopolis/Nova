#include <yyjson.h>

#include "Core.hpp"

#include <optional>
#include <string>

#include "Json.hpp"

// ---------------------------------------------------------------------------------------------------------------------
//         JsonIterator
// ---------------------------------------------------------------------------------------------------------------------

JsonIterator::JsonIterator(yyjson_val* range, const char* _key, bool iterate_obj)
    : key(_key)
{
    if (yyjson_is_arr(range)) {
        max = yyjson_arr_size(range);
        val = yyjson_arr_get_first(range);
        mode = Mode::array;
    } else if (iterate_obj && yyjson_is_obj(range)) {
        max = yyjson_obj_size(range);
        val = unsafe_yyjson_get_first(range);
        mode = Mode::object;
    } else if (range) {
        mode = Mode::optional;
        max = 1;
        val = range;
    }
}

JsonIterator& JsonIterator::operator++() noexcept
{
    idx++;
    if (mode == Mode::object) {
        val = unsafe_yyjson_get_next(val + 1);
    } else if (mode == Mode::array) {
        val = unsafe_yyjson_get_next(val);
    }
    return *this;
}

JsonValue JsonIterator::operator*() const noexcept
{
    if (mode == Mode::object) return JsonValue(yyjson_get_str(val), val + 1);
    if (mode == Mode::array) return JsonValue({}, val);
    return JsonValue(key, val);
}

// ---------------------------------------------------------------------------------------------------------------------
//         JsonValue
// ---------------------------------------------------------------------------------------------------------------------

const char* JsonValue::string() const noexcept { return yyjson_get_str(val); }
std::optional<uint64_t> JsonValue::uint64() const noexcept { if (yyjson_is_int(val)) return yyjson_get_int(val); return std::nullopt; }
std::optional<int64_t> JsonValue::int64() const noexcept { if (yyjson_is_sint(val)) return yyjson_get_sint(val); return std::nullopt; }
std::optional<double> JsonValue::real() const noexcept { if (yyjson_is_real(val)) return yyjson_get_real(val); return std::nullopt; }
bool JsonValue::obj() const noexcept { return yyjson_is_obj(val); }
bool JsonValue::arr() const noexcept { return yyjson_is_arr(val); }
JsonValue::operator bool() const noexcept { return val; }
JsonValue JsonValue::operator[](const char* _key) const noexcept { return JsonValue(_key, yyjson_obj_get(val, _key)); }
JsonValue JsonValue::operator[](int index) const noexcept { return JsonValue(nullptr, yyjson_arr_get(val, index)); }
JsonValue JsonValue::operator[](uint64_t index) const noexcept { return JsonValue(nullptr, yyjson_arr_get(val, index)); }
JsonValue JsonValue::operator[](int64_t index) const noexcept { return JsonValue(nullptr, yyjson_arr_get(val, index)); }

// ---------------------------------------------------------------------------------------------------------------------
//         JsonDocument
// ---------------------------------------------------------------------------------------------------------------------

JsonDocument::JsonDocument(std::string_view json)
    : doc(yyjson_read(json.data(), json.size(), YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS))
{
    if (!doc) NOVA_THROW("Error parsing json");
}

JsonDocument::~JsonDocument()
{
    yyjson_doc_free(doc);
}

JsonValue JsonDocument::root() const noexcept { return JsonValue(nullptr, yyjson_doc_get_root(doc)); }
