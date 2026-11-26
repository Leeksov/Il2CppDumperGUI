#include "../NativeEngine/src/il2cpp/metadata.h"
#include <cassert>
#include <cstring>
#include <iostream>

static void put(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

static std::vector<uint8_t> oldHeader(uint32_t version) {
    std::vector<uint8_t> data(512);
    put(data, 0, 0xFAB11BAF);
    put(data, 4, version);
    put(data, 8, 400);  // string literal table
    put(data, 12, 4);   // v33+ stores only the data index
    put(data, 16, 404); // literal bytes
    put(data, 20, 3);
    std::memcpy(data.data() + 404, "abc", 3);
    return data;
}

static std::vector<uint8_t> newHeader(uint32_t version) {
    std::vector<uint8_t> data(1024);
    put(data, 0, 0xFAB11BAF);
    put(data, 4, version);
    auto section = [&](size_t index, uint32_t offset, uint32_t size, uint32_t count) {
        auto pos = 8 + index * 12;
        put(data, pos, offset);
        put(data, pos + 4, size);
        put(data, pos + 8, count);
    };
    section(0, 400, 4, 1);   // string literals
    section(1, 404, 3, 3);   // literal data
    section(10, 500, 9, 1);  // parameter row: 8 bytes + 1 byte type index
    section(16, 520, 1, 1);  // compact interface type index
    section(18, 524, 5, 1);  // interface offset pair determines type index width
    section(19, 544, 72, 1); // type row with compact indices
    section(2, 700, 13, 13); // assembly names
    section(21, 800, 136, 2); // 68-byte assembly rows
    std::memcpy(data.data() + 404, "xyz", 3);
    put(data, 504, 0x08000001);
    data[508] = 0xfe;
    data[520] = 0x7f;
    data[552] = 5;    // byval type index
    data[553] = 0xff; // missing declaring type
    data[554] = 0xfe; // parent type index
    data[555] = 0xff; // missing generic container
    std::memcpy(data.data() + 700, "First\0Second\0", 13);
    put(data, 800, 0); put(data, 804, 0x20000001); put(data, 808, 1);
    put(data, 868, 1); put(data, 872, 0x20000001); put(data, 876, 1);
    put(data, 888, 6); // second assembly name index
    return data;
}

static std::vector<uint8_t> modernHeader(uint32_t version) {
    std::vector<uint8_t> data(4096);
    put(data, 0, 0xFAB11BAF);
    put(data, 4, version);
    size_t cursor = 1024;
    auto u8 = [](std::vector<uint8_t>& row, uint8_t value) { row.push_back(value); };
    auto u16 = [](std::vector<uint8_t>& row, uint16_t value) {
        row.push_back(value & 0xff);
        row.push_back(value >> 8);
    };
    auto u32 = [](std::vector<uint8_t>& row, uint32_t value) {
        for (int i = 0; i < 4; ++i) row.push_back((value >> (i * 8)) & 0xff);
    };
    auto idx = [&](std::vector<uint8_t>& row, int width, uint32_t value) {
        if (width == 1) u8(row, value);
        else if (width == 2) u16(row, value);
        else u32(row, value);
    };
    const size_t shift = version >= 111 ? 1 : 0;
    auto section = [&](size_t number, const std::vector<uint8_t>& row, uint32_t count) {
        put(data, 8 + number * 12, static_cast<uint32_t>(cursor));
        put(data, 8 + number * 12 + 4, static_cast<uint32_t>(row.size()));
        put(data, 8 + number * 12 + 8, count);
        std::memcpy(data.data() + cursor, row.data(), row.size());
        cursor += row.size();
    };
    section(2, std::vector<uint8_t>{'T','e','s','t',0}, 5);
    section(18 + shift, std::vector<uint8_t>{0,0,0,0,0}, 1);
    std::vector<uint8_t> parameter;
    u32(parameter, 0);
    if (version < 111) { u32(parameter, 0x08000001); u8(parameter, 0); }
    section(10 + shift, parameter, 1);
    std::vector<uint8_t> field;
    u32(field, 0); u8(field, 0);
    if (version < 110) u32(field, 0x04000001);
    section(11 + shift, field, 1);
    std::vector<uint8_t> method;
    u32(method, 0); u8(method, 0);
    if (version < 111) {
        u8(method, 0); u32(method, 0x08000001); u8(method, 0); u8(method, 0xff);
        if (version < 110) u32(method, 0x06000001);
        u16(method, 0); u16(method, 0); u16(method, 0); u16(method, 1);
    } else {
        u8(method, 0); u8(method, 0xff);
        u16(method, 0); u16(method, 0); u16(method, 0); u32(method, 0);
        section(5, std::vector<uint8_t>{0,1,0}, 3);
    }
    section(5 + shift, method, 1);
    int methodWidth = version >= 105 ? 1 : 4;
    int fieldWidth = version >= 106 ? 1 : 4;
    std::vector<uint8_t> type;
    u32(type, 0); u32(type, 0); u8(type, 0); u8(type, 0xff); u8(type, 0xff);
    u8(type, 0xff); u32(type, 0);
    idx(type, fieldWidth, 0); idx(type, methodWidth, 0);
    for (int i = 0; i < 4; ++i) u8(type, 0);
    u32(type, 0); u8(type, 0);
    u16(type, 1); u16(type, 0); u16(type, 1);
    for (int i = 0; i < 5; ++i) u16(type, 0);
    u32(type, 0);
    if (version < 110) u32(type, 0x02000001);
    section(19 + shift, type, 1);
    section(20 + shift, {}, 0); // typeInlineArrays
    std::vector<uint8_t> image;
    u32(image, 0); u32(image, 0); u8(image, 0); u32(image, 1);
    u8(image, 0xff); u32(image, 0);
    idx(image, methodWidth, 0xff); u32(image, 0x20000001);
    u32(image, 0); u32(image, 0);
    if (version >= 108) {
        u32(image, 0); u32(image, 0); u32(image, 0); u8(image, 0xff); u32(image, 0);
    }
    if (version >= 110) {
        u32(image, 0); u32(image, 0); u32(image, 0); u8(image, 0);
    }
    section(21 + shift, image, 1);
    if (version >= 108) {
        section(32 + shift, std::vector<uint8_t>{0, 0xff}, 1); // method on generic type
        section(35 + shift, std::vector<uint8_t>{0, 0, 0}, 1); // method pointer table
    }
    if (version >= 110) section(42 + shift, {}, 0);
    return data;
}

int main() {
    for (auto version : {33u, 35u}) {
        il2cppdumper::Metadata native(oldHeader(version));
        assert(native.version == version);
        assert(native.getStringLiteralFromIndex(0) == "abc");
    }
    for (auto version : {38u, 39u}) {
        il2cppdumper::Metadata native(newHeader(version));
        assert(native.version == version);
        assert(native.typeIndexSize == 1);
        assert(native.parameterDefs.size() == 1);
        assert(native.parameterDefs[0].typeIndex == 254);
        assert(native.typeDefs.size() == 1);
        assert(native.typeDefs[0].parentIndex == 254);
        assert(native.interfaceIndices.size() == 1);
        assert(native.interfaceIndices[0] == 127);
        assert(native.assemblyDefs.size() == 2);
        assert(native.assemblyDefs[1].imageIndex == 1);
        assert(native.assemblyDefs[1].moduleToken == 1);
        assert(native.getStringFromIndex(native.assemblyDefs[1].aname.nameIndex) == "Second");
        assert(native.getStringLiteralFromIndex(0) == "xyz");
    }
    for (auto version : {104u, 105u, 106u, 107u, 108u, 109u, 110u, 111u}) {
        il2cppdumper::Metadata native(modernHeader(version));
        assert(native.version == version);
        assert(native.typeDefs.size() == 1);
        assert(native.methodDefs.size() == 1);
        assert(native.parameterDefs.size() == 1);
        assert(native.fieldDefs.size() == 1);
        assert(native.imageDefs.size() == 1);
        assert(native.methodDefs[0].parameterCount == 1);
        assert(native.methodDefs[0].returnType == 0);
        assert(native.parameterDefs[0].typeIndex == 0);
        assert(native.getStringFromIndex(0) == "Test");
        if (version >= 110) {
            assert(native.typeDefs[0].token == 0x02000001);
            assert(native.fieldDefs[0].token == 0x04000001);
            assert(native.methodDefs[0].token == 0x06000001);
        }
        if (version >= 108) {
            assert(native.getMethodSpec(0).methodDefinitionIndex == 0);
            assert(native.getMethodSpec(0).classIndexIndex == -1);
            assert(native.genericMethodFunctionsDefinitions.size() == 1);
        }
    }
    std::cout << "metadata version smoke tests passed\n";
}
