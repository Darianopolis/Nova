#pragma once

#define NOVA_DECORATE_FLAG_ENUM(EnumType)                                      \
    inline constexpr EnumType operator|(EnumType l, EnumType r) {              \
        return EnumType(std::to_underlying(l) | std::to_underlying(r));        \
    }                                                                          \
    inline constexpr EnumType operator|=(EnumType& l, EnumType r) {            \
        return l = l | r;                                                      \
    }                                                                          \
    inline constexpr bool operator>=(EnumType l, EnumType r) {                 \
        return std::to_underlying(r)                                           \
            == (std::to_underlying(l) & std::to_underlying(r));                \
    }                                                                          \
    inline constexpr bool operator&(EnumType l, EnumType r) {                  \
        return static_cast<std::underlying_type_t<EnumType>>(0)                \
            != (std::to_underlying(l) & std::to_underlying(r));                \
    }