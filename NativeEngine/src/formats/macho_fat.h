#pragma once

#include <cstdint>
#include <vector>

namespace il2cppdumper {

struct Fat {
    uint32_t magic = 0;
    uint32_t offset = 0;
    uint32_t size = 0;
};

class MachoFat {
public:
    explicit MachoFat(std::vector<uint8_t> data);

    std::vector<uint8_t> getMacho(int index);

    std::vector<Fat> fats;

private:
    std::vector<uint8_t> data_;
    size_t pos_ = 0;

    uint32_t readU32BE();
    uint32_t readLittleEndianAt(size_t offset);
    std::vector<uint8_t> readBytes(size_t count);
};

}
