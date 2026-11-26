#pragma once

#include "io/binary_stream.h"

#include <cstdint>

namespace il2cppdumper {

#pragma pack(push, 1)

struct Elf32_Ehdr {
    uint32_t ei_mag;
    uint8_t ei_class;
    uint8_t ei_data;
    uint8_t ei_version;
    uint8_t ei_osabi;
    uint8_t ei_abiversion;
    uint8_t ei_pad[7];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf32_Phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

struct Elf32_Shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
};

struct Elf32_Sym {
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
};

struct Elf32_Dyn {
    int32_t d_tag;
    uint32_t d_un;
};

struct Elf32_Rel {
    uint32_t r_offset;
    uint32_t r_info;
};

struct Elf64_Ehdr {
    uint32_t ei_mag;
    uint8_t ei_class;
    uint8_t ei_data;
    uint8_t ei_version;
    uint8_t ei_osabi;
    uint8_t ei_abiversion;
    uint8_t ei_pad[7];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf64_Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct Elf64_Shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
};

struct Elf64_Sym {
    uint32_t st_name;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
};

struct Elf64_Dyn {
    int64_t d_tag;
    uint64_t d_un;
};

struct Elf64_Rela {
    uint64_t r_offset;
    uint64_t r_info;
    uint64_t r_addend;
};

#pragma pack(pop)

namespace ElfConstants {
constexpr uint16_t EM_386 = 3;
constexpr uint16_t EM_ARM = 40;
constexpr uint16_t EM_X86_64 = 62;
constexpr uint16_t EM_AARCH64 = 183;
constexpr uint32_t PT_LOAD = 1;
constexpr uint32_t PT_DYNAMIC = 2;
constexpr uint32_t PF_X = 1;
constexpr int64_t DT_PLTGOT = 3;
constexpr int64_t DT_HASH = 4;
constexpr int64_t DT_STRTAB = 5;
constexpr int64_t DT_SYMTAB = 6;
constexpr int64_t DT_RELA = 7;
constexpr int64_t DT_RELASZ = 8;
constexpr int64_t DT_INIT = 12;
constexpr int64_t DT_FINI = 13;
constexpr int64_t DT_REL = 17;
constexpr int64_t DT_RELSZ = 18;
constexpr int64_t DT_JMPREL = 23;
constexpr int64_t DT_INIT_ARRAY = 25;
constexpr int64_t DT_FINI_ARRAY = 26;
constexpr int64_t DT_GNU_HASH = 0x6ffffef5;
constexpr uint32_t SHT_LOUSER = 0x80000000;
constexpr uint32_t R_ARM_ABS32 = 2;
constexpr uint32_t R_386_32 = 1;
constexpr uint64_t R_AARCH64_ABS64 = 257;
constexpr uint64_t R_AARCH64_RELATIVE = 1027;
constexpr uint64_t R_X86_64_64 = 1;
constexpr uint64_t R_X86_64_RELATIVE = 8;
}

template <>
inline Elf32_Ehdr BinaryStream::readClass<Elf32_Ehdr>() {
    Elf32_Ehdr t{};
    t.ei_mag = readU32();
    t.ei_class = readU8();
    t.ei_data = readU8();
    t.ei_version = readU8();
    t.ei_osabi = readU8();
    t.ei_abiversion = readU8();
    for (int i = 0; i < 7; i++) t.ei_pad[i] = readU8();
    t.e_type = readU16();
    t.e_machine = readU16();
    t.e_version = readU32();
    t.e_entry = readU32();
    t.e_phoff = readU32();
    t.e_shoff = readU32();
    t.e_flags = readU32();
    t.e_ehsize = readU16();
    t.e_phentsize = readU16();
    t.e_phnum = readU16();
    t.e_shentsize = readU16();
    t.e_shnum = readU16();
    t.e_shstrndx = readU16();
    return t;
}

template <>
inline Elf32_Phdr BinaryStream::readClass<Elf32_Phdr>() {
    Elf32_Phdr t{};
    t.p_type = readU32();
    t.p_offset = readU32();
    t.p_vaddr = readU32();
    t.p_paddr = readU32();
    t.p_filesz = readU32();
    t.p_memsz = readU32();
    t.p_flags = readU32();
    t.p_align = readU32();
    return t;
}

template <>
inline Elf32_Shdr BinaryStream::readClass<Elf32_Shdr>() {
    Elf32_Shdr t{};
    t.sh_name = readU32();
    t.sh_type = readU32();
    t.sh_flags = readU32();
    t.sh_addr = readU32();
    t.sh_offset = readU32();
    t.sh_size = readU32();
    t.sh_link = readU32();
    t.sh_info = readU32();
    t.sh_addralign = readU32();
    t.sh_entsize = readU32();
    return t;
}

template <>
inline Elf32_Sym BinaryStream::readClass<Elf32_Sym>() {
    Elf32_Sym t{};
    t.st_name = readU32();
    t.st_value = readU32();
    t.st_size = readU32();
    t.st_info = readU8();
    t.st_other = readU8();
    t.st_shndx = readU16();
    return t;
}

template <>
inline Elf32_Dyn BinaryStream::readClass<Elf32_Dyn>() {
    Elf32_Dyn t{};
    t.d_tag = readI32();
    t.d_un = readU32();
    return t;
}

template <>
inline Elf32_Rel BinaryStream::readClass<Elf32_Rel>() {
    Elf32_Rel t{};
    t.r_offset = readU32();
    t.r_info = readU32();
    return t;
}

template <>
inline Elf64_Ehdr BinaryStream::readClass<Elf64_Ehdr>() {
    Elf64_Ehdr t{};
    t.ei_mag = readU32();
    t.ei_class = readU8();
    t.ei_data = readU8();
    t.ei_version = readU8();
    t.ei_osabi = readU8();
    t.ei_abiversion = readU8();
    for (int i = 0; i < 7; i++) t.ei_pad[i] = readU8();
    t.e_type = readU16();
    t.e_machine = readU16();
    t.e_version = readU32();
    t.e_entry = readU64();
    t.e_phoff = readU64();
    t.e_shoff = readU64();
    t.e_flags = readU32();
    t.e_ehsize = readU16();
    t.e_phentsize = readU16();
    t.e_phnum = readU16();
    t.e_shentsize = readU16();
    t.e_shnum = readU16();
    t.e_shstrndx = readU16();
    return t;
}

template <>
inline Elf64_Phdr BinaryStream::readClass<Elf64_Phdr>() {
    Elf64_Phdr t{};
    t.p_type = readU32();
    t.p_flags = readU32();
    t.p_offset = readU64();
    t.p_vaddr = readU64();
    t.p_paddr = readU64();
    t.p_filesz = readU64();
    t.p_memsz = readU64();
    t.p_align = readU64();
    return t;
}

template <>
inline Elf64_Shdr BinaryStream::readClass<Elf64_Shdr>() {
    Elf64_Shdr t{};
    t.sh_name = readU32();
    t.sh_type = readU32();
    t.sh_flags = readU64();
    t.sh_addr = readU64();
    t.sh_offset = readU64();
    t.sh_size = readU64();
    t.sh_link = readU32();
    t.sh_info = readU32();
    t.sh_addralign = readU64();
    t.sh_entsize = readU64();
    return t;
}

template <>
inline Elf64_Sym BinaryStream::readClass<Elf64_Sym>() {
    Elf64_Sym t{};
    t.st_name = readU32();
    t.st_info = readU8();
    t.st_other = readU8();
    t.st_shndx = readU16();
    t.st_value = readU64();
    t.st_size = readU64();
    return t;
}

template <>
inline Elf64_Dyn BinaryStream::readClass<Elf64_Dyn>() {
    Elf64_Dyn t{};
    t.d_tag = readI64();
    t.d_un = readU64();
    return t;
}

template <>
inline Elf64_Rela BinaryStream::readClass<Elf64_Rela>() {
    Elf64_Rela t{};
    t.r_offset = readU64();
    t.r_info = readU64();
    t.r_addend = readU64();
    return t;
}

}
