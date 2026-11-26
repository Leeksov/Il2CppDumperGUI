#include "formats/macho_fat.h"

#include <stdexcept>

namespace il2cppdumper {

MachoFat::MachoFat(std::vector<uint8_t> data) : data_(std::move(data)) {
    pos_ = 4;
    int32_t size = static_cast<int32_t>(readU32BE());
    fats.resize(static_cast<size_t>(size));
    for (int32_t i = 0; i < size; i++) {
        pos_ += 8;
        fats[static_cast<size_t>(i)].offset = readU32BE();
        fats[static_cast<size_t>(i)].size = readU32BE();
        pos_ += 4;
    }
    for (int32_t i = 0; i < size; i++) {
        fats[static_cast<size_t>(i)].magic = readLittleEndianAt(fats[static_cast<size_t>(i)].offset);
    }
}

std::vector<uint8_t> MachoFat::getMacho(int index) {
    pos_ = fats[static_cast<size_t>(index)].offset;
    return readBytes(fats[static_cast<size_t>(index)].size);
}

uint32_t MachoFat::readU32BE() {
    if (pos_ + 4 > data_.size()) {
        throw std::out_of_range("MachoFat: unexpected end of data");
    }
    uint32_t v = (static_cast<uint32_t>(data_[pos_]) << 24) | (static_cast<uint32_t>(data_[pos_ + 1]) << 16) |
                 (static_cast<uint32_t>(data_[pos_ + 2]) << 8) | static_cast<uint32_t>(data_[pos_ + 3]);
    pos_ += 4;
    return v;
}

uint32_t MachoFat::readLittleEndianAt(size_t offset) {
    if (offset + 4 > data_.size()) {
        return 0;
    }
    return static_cast<uint32_t>(data_[offset]) | (static_cast<uint32_t>(data_[offset + 1]) << 8) |
           (static_cast<uint32_t>(data_[offset + 2]) << 16) | (static_cast<uint32_t>(data_[offset + 3]) << 24);
}

std::vector<uint8_t> MachoFat::readBytes(size_t count) {
    if (pos_ + count > data_.size()) {
        count = data_.size() - pos_;
    }
    std::vector<uint8_t> out(data_.begin() + static_cast<std::ptrdiff_t>(pos_),
                             data_.begin() + static_cast<std::ptrdiff_t>(pos_ + count));
    pos_ += count;
    return out;
}

}
