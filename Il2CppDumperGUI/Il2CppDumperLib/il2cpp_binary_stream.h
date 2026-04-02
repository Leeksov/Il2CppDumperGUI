#pragma once
#include "il2cpp_types.h"
#include <cstdio>

// ============================================================
// BinaryStream - core binary reader (replaces C# BinaryStream)
// ============================================================
class BinaryStream {
public:
    double Version = 0;
    bool Is32Bit = false;
    uint64_t ImageBase = 0;
    bool IsDumped = false;

    BinaryStream() = default;
    BinaryStream(const std::vector<uint8_t>& data);
    BinaryStream(const uint8_t* data, size_t size);
    virtual ~BinaryStream() = default;

    // Position
    uint64_t GetPosition() const { return position_; }
    void SetPosition(uint64_t pos) { position_ = pos; }
    uint64_t GetLength() const { return data_.size(); }

    // Read primitives
    bool ReadBoolean();
    uint8_t ReadByte();
    int8_t ReadSByte();
    std::vector<uint8_t> ReadBytes(int count);
    int16_t ReadInt16();
    uint16_t ReadUInt16();
    int32_t ReadInt32();
    uint32_t ReadUInt32();
    int64_t ReadInt64();
    uint64_t ReadUInt64();
    float ReadSingle();
    double ReadDouble();
    uint32_t ReadCompressedUInt32();
    int32_t ReadCompressedInt32();
    uint32_t ReadULeb128();

    // Pointer-size aware reads
    int64_t ReadIntPtr();
    virtual uint64_t ReadUIntPtr();
    uint64_t PointerSize() const { return Is32Bit ? 4 : 8; }

    // String
    std::string ReadStringToNull(uint64_t addr);

    // Write primitives (for relocation processing)
    void WriteUInt32(uint32_t value);
    void WriteUInt64(uint64_t value);
    void WriteUIntPtr(uint64_t value);

    // Version-aware struct readers
    Il2CppCodeRegistration ReadCodeRegistration();
    Il2CppMetadataRegistration ReadMetadataRegistration();
    Il2CppGenericClass ReadGenericClass();
    Il2CppGenericInst ReadGenericInst();
    Il2CppArrayType ReadArrayType();
    Il2CppGenericMethodFunctionsDefinitions ReadGenericMethodFunctionsDefinitions();
    Il2CppMethodSpec ReadMethodSpec();
    Il2CppCodeGenModule ReadCodeGenModule();
    Il2CppTokenRangePair ReadTokenRangePair();
    Il2CppRGCTXDefinition ReadRGCTXDefinition();
    Il2CppType ReadIl2CppType();

    // Metadata struct readers
    Il2CppGlobalMetadataHeader ReadGlobalMetadataHeader();
    Il2CppImageDefinition ReadImageDefinition();
    Il2CppAssemblyDefinition ReadAssemblyDefinition();
    Il2CppTypeDefinition ReadTypeDefinition();
    Il2CppMethodDefinition ReadMethodDefinition();
    Il2CppParameterDefinition ReadParameterDefinition();
    Il2CppFieldDefinition ReadFieldDefinition();
    Il2CppFieldDefaultValue ReadFieldDefaultValue();
    Il2CppParameterDefaultValue ReadParameterDefaultValue();
    Il2CppPropertyDefinition ReadPropertyDefinition();
    Il2CppCustomAttributeTypeRange ReadCustomAttributeTypeRange();
    Il2CppCustomAttributeDataRange ReadCustomAttributeDataRange();
    Il2CppMetadataUsageList ReadMetadataUsageList();
    Il2CppMetadataUsagePair ReadMetadataUsagePair();
    Il2CppStringLiteral ReadStringLiteral();
    Il2CppEventDefinition ReadEventDefinition();
    Il2CppGenericContainer ReadGenericContainer();
    Il2CppFieldRef ReadFieldRef();
    Il2CppGenericParameter ReadGenericParameter();
    Il2CppGenericMethodIndices ReadGenericMethodIndices();

    // ELF readers
    Elf32_Ehdr ReadElf32Ehdr();
    Elf32_Phdr ReadElf32Phdr();
    Elf32_Dyn ReadElf32Dyn();
    Elf32_Sym ReadElf32Sym();
    Elf32_Shdr ReadElf32Shdr();
    Elf32_Rel ReadElf32Rel();
    Elf64_Ehdr ReadElf64Ehdr();
    Elf64_Phdr ReadElf64Phdr();
    Elf64_Dyn ReadElf64Dyn();
    Elf64_Sym ReadElf64Sym();
    Elf64_Shdr ReadElf64Shdr();
    Elf64_Rela ReadElf64Rela();

    // PE readers
    DosHeader ReadDosHeader();
    PEFileHeader ReadPEFileHeader();
    PESectionHeader ReadPESectionHeader();

    // Template array readers
    template<typename T, typename ReadFunc>
    std::vector<T> ReadArray(uint64_t addr, int64_t count, ReadFunc readFunc) {
        SetPosition(addr);
        std::vector<T> result;
        result.reserve((size_t)count);
        for (int64_t i = 0; i < count; i++) {
            result.push_back((this->*readFunc)());
        }
        return result;
    }

    // Read array of uint64_t
    std::vector<uint64_t> ReadUInt64Array(uint64_t addr, int64_t count);
    std::vector<uint32_t> ReadUInt32Array(uint64_t addr, int64_t count);
    std::vector<int32_t> ReadInt32Array(uint64_t addr, int64_t count);

    // Raw data access
    const uint8_t* RawData() const { return data_.data(); }
    size_t RawSize() const { return data_.size(); }

protected:
    std::vector<uint8_t> data_;
    uint64_t position_ = 0;

    void EnsureBytes(size_t count) const;
    template<typename T> T ReadRaw() {
        EnsureBytes(sizeof(T));
        T value;
        memcpy(&value, data_.data() + position_, sizeof(T));
        position_ += sizeof(T);
        return value;
    }
};
