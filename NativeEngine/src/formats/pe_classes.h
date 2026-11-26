#pragma once

#include "io/binary_stream.h"

#include <cstdint>

namespace il2cppdumper {

#pragma pack(push, 1)

struct DosHeader {
    uint16_t Magic;
    uint16_t Cblp;
    uint16_t Cp;
    uint16_t Crlc;
    uint16_t Cparhdr;
    uint16_t Minalloc;
    uint16_t Maxalloc;
    uint16_t Ss;
    uint16_t Sp;
    uint16_t Csum;
    uint16_t Ip;
    uint16_t Cs;
    uint16_t Lfarlc;
    uint16_t Ovno;
    uint16_t Res[4];
    uint16_t Oemid;
    uint16_t Oeminfo;
    uint16_t Res2[10];
    uint32_t Lfanew;
};

struct FileHeader {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
};

struct OptionalHeader {
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint32_t BaseOfData;
    uint32_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint32_t SizeOfStackReserve;
    uint32_t SizeOfStackCommit;
    uint32_t SizeOfHeapReserve;
    uint32_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
};

struct OptionalHeader64 {
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
};

struct SectionHeader {
    uint8_t Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
};

#pragma pack(pop)

template <>
inline DosHeader BinaryStream::readClass<DosHeader>() {
    DosHeader t{};
    t.Magic = readU16();
    t.Cblp = readU16();
    t.Cp = readU16();
    t.Crlc = readU16();
    t.Cparhdr = readU16();
    t.Minalloc = readU16();
    t.Maxalloc = readU16();
    t.Ss = readU16();
    t.Sp = readU16();
    t.Csum = readU16();
    t.Ip = readU16();
    t.Cs = readU16();
    t.Lfarlc = readU16();
    t.Ovno = readU16();
    for (int i = 0; i < 4; i++) t.Res[i] = readU16();
    t.Oemid = readU16();
    t.Oeminfo = readU16();
    for (int i = 0; i < 10; i++) t.Res2[i] = readU16();
    t.Lfanew = readU32();
    return t;
}

template <>
inline FileHeader BinaryStream::readClass<FileHeader>() {
    FileHeader t{};
    t.Machine = readU16();
    t.NumberOfSections = readU16();
    t.TimeDateStamp = readU32();
    t.PointerToSymbolTable = readU32();
    t.NumberOfSymbols = readU32();
    t.SizeOfOptionalHeader = readU16();
    t.Characteristics = readU16();
    return t;
}

template <>
inline OptionalHeader BinaryStream::readClass<OptionalHeader>() {
    OptionalHeader t{};
    t.Magic = readU16();
    t.MajorLinkerVersion = readU8();
    t.MinorLinkerVersion = readU8();
    t.SizeOfCode = readU32();
    t.SizeOfInitializedData = readU32();
    t.SizeOfUninitializedData = readU32();
    t.AddressOfEntryPoint = readU32();
    t.BaseOfCode = readU32();
    t.BaseOfData = readU32();
    t.ImageBase = readU32();
    t.SectionAlignment = readU32();
    t.FileAlignment = readU32();
    t.MajorOperatingSystemVersion = readU16();
    t.MinorOperatingSystemVersion = readU16();
    t.MajorImageVersion = readU16();
    t.MinorImageVersion = readU16();
    t.MajorSubsystemVersion = readU16();
    t.MinorSubsystemVersion = readU16();
    t.Win32VersionValue = readU32();
    t.SizeOfImage = readU32();
    t.SizeOfHeaders = readU32();
    t.CheckSum = readU32();
    t.Subsystem = readU16();
    t.DllCharacteristics = readU16();
    t.SizeOfStackReserve = readU32();
    t.SizeOfStackCommit = readU32();
    t.SizeOfHeapReserve = readU32();
    t.SizeOfHeapCommit = readU32();
    t.LoaderFlags = readU32();
    t.NumberOfRvaAndSizes = readU32();
    return t;
}

template <>
inline OptionalHeader64 BinaryStream::readClass<OptionalHeader64>() {
    OptionalHeader64 t{};
    t.Magic = readU16();
    t.MajorLinkerVersion = readU8();
    t.MinorLinkerVersion = readU8();
    t.SizeOfCode = readU32();
    t.SizeOfInitializedData = readU32();
    t.SizeOfUninitializedData = readU32();
    t.AddressOfEntryPoint = readU32();
    t.BaseOfCode = readU32();
    t.ImageBase = readU64();
    t.SectionAlignment = readU32();
    t.FileAlignment = readU32();
    t.MajorOperatingSystemVersion = readU16();
    t.MinorOperatingSystemVersion = readU16();
    t.MajorImageVersion = readU16();
    t.MinorImageVersion = readU16();
    t.MajorSubsystemVersion = readU16();
    t.MinorSubsystemVersion = readU16();
    t.Win32VersionValue = readU32();
    t.SizeOfImage = readU32();
    t.SizeOfHeaders = readU32();
    t.CheckSum = readU32();
    t.Subsystem = readU16();
    t.DllCharacteristics = readU16();
    t.SizeOfStackReserve = readU64();
    t.SizeOfStackCommit = readU64();
    t.SizeOfHeapReserve = readU64();
    t.SizeOfHeapCommit = readU64();
    t.LoaderFlags = readU32();
    t.NumberOfRvaAndSizes = readU32();
    return t;
}

template <>
inline SectionHeader BinaryStream::readClass<SectionHeader>() {
    SectionHeader t{};
    for (int i = 0; i < 8; i++) t.Name[i] = readU8();
    t.VirtualSize = readU32();
    t.VirtualAddress = readU32();
    t.SizeOfRawData = readU32();
    t.PointerToRawData = readU32();
    t.PointerToRelocations = readU32();
    t.PointerToLinenumbers = readU32();
    t.NumberOfRelocations = readU16();
    t.NumberOfLinenumbers = readU16();
    t.Characteristics = readU32();
    return t;
}

}
