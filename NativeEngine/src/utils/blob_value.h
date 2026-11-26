#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace il2cppdumper {

struct BlobValue {
    enum class Kind : uint8_t {
        None,
        Bool,
        I8,
        U8,
        I16,
        U16,
        I32,
        U32,
        I64,
        U64,
        F32,
        F64,
        Char,
        String,
        Array,
        TypeIndex,
    };

    Kind kind = Kind::None;
    bool b = false;
    int8_t i8 = 0;
    uint8_t u8 = 0;
    int16_t i16 = 0;
    uint16_t u16 = 0;
    int32_t i32 = 0;
    uint32_t u32 = 0;
    int64_t i64 = 0;
    uint64_t u64 = 0;
    float f32 = 0.0f;
    double f64 = 0.0;
    char16_t ch = 0;
    std::string str;
    std::vector<BlobValue> array;
    uint32_t typeIndex = 0;
    int il2CppTypeEnum = 0;
    uint32_t enumTypeIndex = 0;

    bool isNull() const { return kind == Kind::None; }
};

}
