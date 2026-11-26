#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace il2cppdumper {

class BinaryStream {
public:
    double version = 0.0;
    bool is32Bit = false;
    uint64_t imageBase = 0;
    int typeIndexSize = 4;
    int typeDefinitionIndexSize = 4;
    int genericContainerIndexSize = 4;
    int parameterIndexSize = 4;
    int eventIndexSize = 4;
    int interfaceIndexSize = 4;
    int nestedTypeIndexSize = 4;
    int propertyIndexSize = 4;
    int methodIndexSize = 4;
    int genericParameterIndexSize = 4;
    int fieldIndexSize = 4;
    int defaultValueDataIndexSize = 4;
    int genericInstIndexSize = 4;
    int genericMethodIndexSize = 4;
    int methodPointerTableIndexSize = 4;
    int invokerTableIndexSize = 4;
    int adjustorThunkIndexSize = 4;
    int genericContainerRowSize = 16;
    int assemblyRowSize = 0;

    explicit BinaryStream(std::vector<uint8_t> data) : data_(std::move(data)) {}
    virtual ~BinaryStream() = default;

    BinaryStream(const BinaryStream&) = delete;
    BinaryStream& operator=(const BinaryStream&) = delete;

    size_t position() const { return pos_; }
    void setPosition(size_t p) { pos_ = p; }
    void seek(int64_t offset) { pos_ += offset; }
    size_t length() const { return data_.size(); }

    uint8_t readU8();
    int8_t readI8();
    uint16_t readU16();
    int16_t readI16();
    uint32_t readU32();
    int32_t readI32();
    int32_t readIndex(int width) {
        switch (width) {
        case 1: { auto v = readU8(); return v == 0xff ? -1 : v; }
        case 2: { auto v = readU16(); return v == 0xffff ? -1 : v; }
        case 4: return readI32();
        default: throw std::runtime_error("Invalid metadata index width");
        }
    }
    uint64_t readU64();
    int64_t readI64();
    float readF32();
    double readF64();
    uint32_t readU32BE();
    int32_t readI32BE();
    virtual uint64_t readUPtr();
    int64_t readIPtr();
    std::vector<uint8_t> readBytes(size_t count);
    std::string readStringToNull(size_t addr);
    uint32_t readULEB128();
    uint32_t readCompressedU32();
    int32_t readCompressedI32();
    uint64_t pointerSize() const { return is32Bit ? 4ull : 8ull; }

    void writeU16(uint16_t v);
    void writeU32(uint32_t v);
    void writeU64(uint64_t v);
    void writeI32(int32_t v);
    void writeUPtr(uint64_t v);

    template <typename T>
    T readClass() {
        static_assert(sizeof(T) == 0, "readClass<T>: missing specialization for this type");
    }

    template <typename T>
    T readClassAt(uint64_t addr) {
        setPosition(static_cast<size_t>(addr));
        return readClass<T>();
    }

    template <typename T>
    std::vector<T> readClassArray(int64_t count);

    template <typename T>
    std::vector<T> readClassArrayAt(uint64_t addr, int64_t count) {
        setPosition(static_cast<size_t>(addr));
        return readClassArray<T>(count);
    }

    template <typename T>
    size_t sizeOfClass() {
        bool old = countOnly_;
        countOnly_ = true;
        size_t start = pos_;
        readClass<T>();
        size_t sz = pos_ - start;
        countOnly_ = old;
        return sz;
    }

protected:
    std::vector<uint8_t> data_;
    size_t pos_ = 0;
    bool countOnly_ = false;

    bool inRange(double min, double max) const { return version >= min && version <= max; }
};

template <typename T>
std::vector<T> BinaryStream::readClassArray(int64_t count) {
    std::vector<T> result;
    if (count <= 0) {
        return result;
    }
    result.reserve(static_cast<size_t>(count));
    if constexpr (std::is_arithmetic_v<T>) {
        for (int64_t i = 0; i < count; i++) {
            if constexpr (sizeof(T) == 1) {
                result.push_back(static_cast<T>(readU8()));
            } else if constexpr (sizeof(T) == 2) {
                result.push_back(static_cast<T>(readU16()));
            } else if constexpr (sizeof(T) == 4) {
                if constexpr (std::is_signed_v<T>) {
                    result.push_back(static_cast<T>(readI32()));
                } else {
                    result.push_back(static_cast<T>(readU32()));
                }
            } else {
                if constexpr (std::is_same_v<T, double>) {
                    result.push_back(static_cast<T>(readF64()));
                } else if constexpr (std::is_signed_v<T>) {
                    result.push_back(static_cast<T>(readIPtr()));
                } else {
                    result.push_back(static_cast<T>(readUPtr()));
                }
            }
        }
    } else {
        for (int64_t i = 0; i < count; i++) {
            result.push_back(readClass<T>());
        }
    }
    return result;
}

}
