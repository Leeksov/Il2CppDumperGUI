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
    std::vector<uint8_t> data(640);
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
    std::memcpy(data.data() + 404, "xyz", 3);
    put(data, 504, 0x08000001);
    data[508] = 0xfe;
    data[520] = 0x7f;
    data[552] = 5;    // byval type index
    data[553] = 0xff; // missing declaring type
    data[554] = 0xfe; // parent type index
    data[555] = 0xff; // missing generic container
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
        assert(native.getStringLiteralFromIndex(0) == "xyz");
    }
    std::cout << "metadata version smoke tests passed\n";
}
