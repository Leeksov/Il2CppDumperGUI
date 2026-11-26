#include "io/binary_stream.h"

#include <cstring>
#include <stdexcept>
#include <string>

namespace il2cppdumper {

namespace {

[[noreturn]] void throwEof() {
    throw std::out_of_range("BinaryStream: unexpected end of data");
}

}

uint8_t BinaryStream::readU8() {
    if (countOnly_) {
        pos_ += 1;
        return 0;
    }
    if (pos_ + 1 > data_.size()) {
        throwEof();
    }
    return data_[pos_++];
}

int8_t BinaryStream::readI8() {
    return static_cast<int8_t>(readU8());
}

uint16_t BinaryStream::readU16() {
    if (countOnly_) {
        pos_ += 2;
        return 0;
    }
    if (pos_ + 2 > data_.size()) {
        throwEof();
    }
    uint16_t v = static_cast<uint16_t>(data_[pos_] | (static_cast<uint16_t>(data_[pos_ + 1]) << 8));
    pos_ += 2;
    return v;
}

int16_t BinaryStream::readI16() {
    return static_cast<int16_t>(readU16());
}

uint32_t BinaryStream::readU32() {
    if (countOnly_) {
        pos_ += 4;
        return 0;
    }
    if (pos_ + 4 > data_.size()) {
        throwEof();
    }
    uint32_t v = static_cast<uint32_t>(data_[pos_]) |
                 (static_cast<uint32_t>(data_[pos_ + 1]) << 8) |
                 (static_cast<uint32_t>(data_[pos_ + 2]) << 16) |
                 (static_cast<uint32_t>(data_[pos_ + 3]) << 24);
    pos_ += 4;
    return v;
}

int32_t BinaryStream::readI32() {
    return static_cast<int32_t>(readU32());
}

uint64_t BinaryStream::readU64() {
    if (countOnly_) {
        pos_ += 8;
        return 0;
    }
    if (pos_ + 8 > data_.size()) {
        throwEof();
    }
    uint64_t v = static_cast<uint64_t>(data_[pos_]) |
                 (static_cast<uint64_t>(data_[pos_ + 1]) << 8) |
                 (static_cast<uint64_t>(data_[pos_ + 2]) << 16) |
                 (static_cast<uint64_t>(data_[pos_ + 3]) << 24) |
                 (static_cast<uint64_t>(data_[pos_ + 4]) << 32) |
                 (static_cast<uint64_t>(data_[pos_ + 5]) << 40) |
                 (static_cast<uint64_t>(data_[pos_ + 6]) << 48) |
                 (static_cast<uint64_t>(data_[pos_ + 7]) << 56);
    pos_ += 8;
    return v;
}

int64_t BinaryStream::readI64() {
    return static_cast<int64_t>(readU64());
}

float BinaryStream::readF32() {
    uint32_t bits = readU32();
    float f;
    static_assert(sizeof(f) == sizeof(bits), "float must be 32-bit");
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

double BinaryStream::readF64() {
    uint64_t bits = readU64();
    double d;
    static_assert(sizeof(d) == sizeof(bits), "double must be 64-bit");
    std::memcpy(&d, &bits, sizeof(d));
    return d;
}

uint32_t BinaryStream::readU32BE() {
    if (countOnly_) {
        pos_ += 4;
        return 0;
    }
    if (pos_ + 4 > data_.size()) {
        throwEof();
    }
    uint32_t v = (static_cast<uint32_t>(data_[pos_]) << 24) |
                 (static_cast<uint32_t>(data_[pos_ + 1]) << 16) |
                 (static_cast<uint32_t>(data_[pos_ + 2]) << 8) |
                 static_cast<uint32_t>(data_[pos_ + 3]);
    pos_ += 4;
    return v;
}

int32_t BinaryStream::readI32BE() {
    return static_cast<int32_t>(readU32BE());
}

uint64_t BinaryStream::readUPtr() {
    return is32Bit ? readU32() : readU64();
}

int64_t BinaryStream::readIPtr() {
    return is32Bit ? readI32() : readI64();
}

std::vector<uint8_t> BinaryStream::readBytes(size_t count) {
    if (countOnly_) {
        pos_ += count;
        return {};
    }
    if (count == 0) {
        return {};
    }
    size_t avail = data_.size() > pos_ ? data_.size() - pos_ : 0;
    size_t n = count < avail ? count : avail;
    std::vector<uint8_t> out(data_.begin() + static_cast<std::ptrdiff_t>(pos_),
                             data_.begin() + static_cast<std::ptrdiff_t>(pos_ + n));
    pos_ += n;
    return out;
}

std::string BinaryStream::readStringToNull(size_t addr) {
    if (addr >= data_.size()) {
        return {};
    }
    size_t p = addr;
    size_t end = data_.size();
    std::string out;
    out.reserve(end - p);
    while (p < end && data_[p] != 0) {
        out.push_back(static_cast<char>(data_[p]));
        p++;
    }
    return out;
}

uint32_t BinaryStream::readULEB128() {
    uint32_t value = readU8();
    if (value >= 0x80) {
        uint32_t bitshift = 0;
        value &= 0x7f;
        while (true) {
            uint8_t b = readU8();
            bitshift += 7;
            value |= static_cast<uint32_t>(b & 0x7f) << bitshift;
            if (b < 0x80) {
                break;
            }
        }
    }
    return value;
}

uint32_t BinaryStream::readCompressedU32() {
    uint32_t val;
    uint8_t read = readU8();

    if ((read & 0x80) == 0) {
        val = read;
    } else if ((read & 0xC0) == 0x80) {
        val = static_cast<uint32_t>(read & ~0x80u) << 8;
        val |= readU8();
    } else if ((read & 0xE0) == 0xC0) {
        val = static_cast<uint32_t>(read & ~0xC0u) << 24;
        val |= static_cast<uint32_t>(readU8()) << 16;
        val |= static_cast<uint32_t>(readU8()) << 8;
        val |= readU8();
    } else if (read == 0xF0) {
        val = readU32();
    } else if (read == 0xFE) {
        val = UINT32_MAX - 1;
    } else if (read == 0xFF) {
        val = UINT32_MAX;
    } else {
        throw std::runtime_error("Invalid compressed integer format");
    }
    return val;
}

int32_t BinaryStream::readCompressedI32() {
    uint32_t encoded = readCompressedU32();
    if (encoded == UINT32_MAX) {
        return INT32_MIN;
    }
    bool isNegative = (encoded & 1) != 0;
    encoded >>= 1;
    if (isNegative) {
        return -static_cast<int32_t>(encoded + 1);
    }
    return static_cast<int32_t>(encoded);
}

void BinaryStream::writeU16(uint16_t v) {
    if (pos_ + 2 > data_.size()) {
        data_.resize(pos_ + 2, 0);
    }
    data_[pos_] = static_cast<uint8_t>(v & 0xFF);
    data_[pos_ + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    pos_ += 2;
}

void BinaryStream::writeU32(uint32_t v) {
    if (pos_ + 4 > data_.size()) {
        data_.resize(pos_ + 4, 0);
    }
    data_[pos_] = static_cast<uint8_t>(v & 0xFF);
    data_[pos_ + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    data_[pos_ + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    data_[pos_ + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
    pos_ += 4;
}

void BinaryStream::writeU64(uint64_t v) {
    if (pos_ + 8 > data_.size()) {
        data_.resize(pos_ + 8, 0);
    }
    for (int i = 0; i < 8; i++) {
        data_[pos_ + static_cast<size_t>(i)] = static_cast<uint8_t>((v >> (i * 8)) & 0xFF);
    }
    pos_ += 8;
}

void BinaryStream::writeI32(int32_t v) {
    writeU32(static_cast<uint32_t>(v));
}

void BinaryStream::writeUPtr(uint64_t v) {
    if (is32Bit) {
        writeU32(static_cast<uint32_t>(v));
    } else {
        writeU64(v);
    }
}

}
