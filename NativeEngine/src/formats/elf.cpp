#include "formats/elf.h"

#include "utils/pattern.h"
#include "utils/section_helper.h"

#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace il2cppdumper {

using namespace ElfConstants;

/*
 * LDR R1, [X]
 * ADD R0, X, X
 * ADD R2, X, X
 */
static const std::string kArmFeatureBytes = "? 0x10 ? 0xE7 ? 0x00 ? 0xE0 ? 0x20 ? 0xE0";
static const std::string kX86FeatureBytes = "? 0x10 ? 0xE7 ? 0x00 ? 0xE0 ? 0x20 ? 0xE0";

Elf::Elf(std::vector<uint8_t> data) : ElfBase(std::move(data)) {
    is32Bit = true;
    load();
}

void Elf::load() {
    elfHeader = readClassAt<Elf32_Ehdr>(0);
    programSegment = readClassArrayAt<Elf32_Phdr>(elfHeader.e_phoff, elfHeader.e_phnum);
    if (isDumped) {
        fixedProgramSegment();
    }
    for (const auto& phdr : programSegment) {
        if (phdr.p_type == PT_DYNAMIC) {
            pt_dynamic = phdr;
            break;
        }
    }
    dynamicSection = readClassArrayAt<Elf32_Dyn>(pt_dynamic.p_offset, pt_dynamic.p_filesz / 8u);
    if (isDumped) {
        fixedDynamicSection();
    }
    readSymbol();
    if (!isDumped) {
        relocationProcessing();
        if (checkProtection()) {
            std::printf("ERROR: This file may be protected.\n");
        }
    }
}

bool Elf::checkSection() {
    try {
        std::vector<std::string> names;
        sectionTable = readClassArrayAt<Elf32_Shdr>(elfHeader.e_shoff, elfHeader.e_shnum);
        if (elfHeader.e_shstrndx >= sectionTable.size()) {
            return false;
        }
        uint32_t shstrndx = sectionTable[elfHeader.e_shstrndx].sh_offset;
        for (const auto& section : sectionTable) {
            names.push_back(readStringToNull(shstrndx + section.sh_name));
        }
        for (const auto& name : names) {
            if (name == ".text") {
                return true;
            }
        }
        return false;
    } catch (const std::exception&) {
        return false;
    }
}

uint64_t Elf::mapVATR(uint64_t addr) {
    for (const auto& phdr : programSegment) {
        if (addr >= phdr.p_vaddr && addr <= static_cast<uint64_t>(phdr.p_vaddr) + phdr.p_memsz) {
            return addr - phdr.p_vaddr + phdr.p_offset;
        }
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "mapVATR: address not in any segment (0x%llx)",
                  static_cast<unsigned long long>(addr));
    throw std::runtime_error(buf);
}

uint64_t Elf::mapRTVA(uint64_t addr) {
    for (const auto& phdr : programSegment) {
        if (addr >= phdr.p_offset && addr <= static_cast<uint64_t>(phdr.p_offset) + phdr.p_filesz) {
            return addr - phdr.p_offset + phdr.p_vaddr;
        }
    }
    return 0;
}

bool Elf::search() {
    uint32_t _GLOBAL_OFFSET_TABLE_ = 0;
    for (const auto& dyn : dynamicSection) {
        if (dyn.d_tag == DT_PLTGOT) {
            _GLOBAL_OFFSET_TABLE_ = dyn.d_un;
            break;
        }
    }
    std::vector<Elf32_Phdr> execs;
    for (const auto& phdr : programSegment) {
        if (phdr.p_type == PT_LOAD && (phdr.p_flags & PF_X) == 1) {
            execs.push_back(phdr);
        }
    }
    std::vector<int> resultList;
    const std::string& featureBytes = elfHeader.e_machine == EM_ARM ? kArmFeatureBytes : kX86FeatureBytes;
    for (const auto& exec : execs) {
        setPosition(exec.p_offset);
        auto buff = readBytes(exec.p_filesz);
        for (size_t temp : searchPattern(buff, featureBytes)) {
            if (((buff[temp + 2] >> 4) & 1) == 1) {
                resultList.push_back(static_cast<int>(temp));
            }
        }
    }
    if (resultList.size() == 1) {
        uint32_t codeRegistration = 0;
        uint32_t metadataRegistration = 0;
        uint32_t result = static_cast<uint32_t>(resultList[0]);
        if (version < 24) {
            if (elfHeader.e_machine == EM_ARM) {
                setPosition(result + 0x14);
                codeRegistration = readU32() + _GLOBAL_OFFSET_TABLE_;
                setPosition(result + 0x18);
                uint32_t ptr = readU32() + _GLOBAL_OFFSET_TABLE_;
                setPosition(mapVATR(ptr));
                metadataRegistration = readU32();
            }
        } else if (version >= 24) {
            if (elfHeader.e_machine == EM_ARM) {
                setPosition(result + 0x14);
                codeRegistration = readU32() + result + 0xcu + static_cast<uint32_t>(imageBase);
                setPosition(result + 0x10);
                uint32_t ptr = readU32() + result + 0x8;
                setPosition(mapVATR(ptr + imageBase));
                metadataRegistration = readU32();
            }
        }
        std::printf("CodeRegistration : %x\n", codeRegistration);
        std::printf("MetadataRegistration : %x\n", metadataRegistration);
        init(codeRegistration, metadataRegistration);
        return true;
    }
    return false;
}

bool Elf::plusSearch(int methodCount, int typeDefinitionsCount, int imageCount) {
    auto sectionHelper = getSectionHelper(methodCount, typeDefinitionsCount, imageCount);
    uint64_t codeRegistration = sectionHelper.findCodeRegistration();
    uint64_t metadataRegistration = sectionHelper.findMetadataRegistration();
    return autoPlusInit(codeRegistration, metadataRegistration);
}

bool Elf::symbolSearch() {
    uint32_t codeRegistration = 0;
    uint32_t metadataRegistration = 0;
    uint64_t dynstrOffset = 0;
    for (const auto& dyn : dynamicSection) {
        if (dyn.d_tag == DT_STRTAB) {
            dynstrOffset = mapVATR(dyn.d_un);
            break;
        }
    }
    for (const auto& symbol : symbolTable) {
        std::string name = readStringToNull(dynstrOffset + symbol.st_name);
        if (name == "g_CodeRegistration") {
            codeRegistration = symbol.st_value;
        } else if (name == "g_MetadataRegistration") {
            metadataRegistration = symbol.st_value;
        }
    }
    if (codeRegistration > 0 && metadataRegistration > 0) {
        std::printf("Detected Symbol !\n");
        std::printf("CodeRegistration : %x\n", codeRegistration);
        std::printf("MetadataRegistration : %x\n", metadataRegistration);
        init(codeRegistration, metadataRegistration);
        return true;
    }
    std::printf("ERROR: No symbol is detected\n");
    return false;
}

void Elf::readSymbol() {
    try {
        uint32_t symbolCount = 0;
        const Elf32_Dyn* hash = nullptr;
        for (const auto& dyn : dynamicSection) {
            if (dyn.d_tag == DT_HASH) {
                hash = &dyn;
                break;
            }
        }
        if (hash != nullptr) {
            uint64_t addr = mapVATR(hash->d_un);
            setPosition(addr);
            readU32();
            uint32_t nchain = readU32();
            symbolCount = nchain;
        } else {
            const Elf32_Dyn* gnuHash = nullptr;
            for (const auto& dyn : dynamicSection) {
                if (dyn.d_tag == DT_GNU_HASH) {
                    gnuHash = &dyn;
                    break;
                }
            }
            if (gnuHash == nullptr) {
                return;
            }
            uint64_t addr = mapVATR(gnuHash->d_un);
            setPosition(addr);
            uint32_t nbuckets = readU32();
            uint32_t symoffset = readU32();
            uint32_t bloom_size = readU32();
            /* uint32_t bloom_shift = */ readU32();
            uint64_t bucketsAddress = addr + 16 + (4ull * bloom_size);
            auto buckets = readClassArrayAt<uint32_t>(bucketsAddress, nbuckets);
            uint32_t last_symbol = 0;
            for (uint32_t b : buckets) {
                if (b > last_symbol) {
                    last_symbol = b;
                }
            }
            if (last_symbol < symoffset) {
                symbolCount = symoffset;
            } else {
                uint64_t chainsBaseAddress = bucketsAddress + 4ull * nbuckets;
                setPosition(chainsBaseAddress + (last_symbol - symoffset) * 4ull);
                while (true) {
                    uint32_t chain_entry = readU32();
                    ++last_symbol;
                    if ((chain_entry & 1) != 0) {
                        break;
                    }
                }
                symbolCount = last_symbol;
            }
        }
        uint64_t dynsymOffset = 0;
        for (const auto& dyn : dynamicSection) {
            if (dyn.d_tag == DT_SYMTAB) {
                dynsymOffset = mapVATR(dyn.d_un);
                break;
            }
        }
        symbolTable = readClassArrayAt<Elf32_Sym>(dynsymOffset, symbolCount);
    } catch (const std::exception&) {
    }
}

void Elf::relocationProcessing() {
    std::printf("Applying relocations...\n");
    try {
        uint64_t reldynOffset = 0;
        uint64_t reldynSize = 0;
        for (const auto& dyn : dynamicSection) {
            if (dyn.d_tag == DT_REL) {
                reldynOffset = mapVATR(dyn.d_un);
            } else if (dyn.d_tag == DT_RELSZ) {
                reldynSize = dyn.d_un;
            }
        }
        auto relTable = readClassArrayAt<Elf32_Rel>(reldynOffset, reldynSize / 8);
        bool isx86 = elfHeader.e_machine == 0x3;
        for (const auto& rel : relTable) {
            uint32_t type = rel.r_info & 0xff;
            uint32_t sym = rel.r_info >> 8;
            if ((type == R_386_32 && isx86) || (type == R_ARM_ABS32 && !isx86)) {
                if (sym >= symbolTable.size()) {
                    continue;
                }
                const auto& symbol = symbolTable[sym];
                setPosition(mapVATR(rel.r_offset));
                writeU32(symbol.st_value);
            }
        }
    } catch (const std::exception&) {
    }
}

bool Elf::checkProtection() {
    try {
        for (const auto& dyn : dynamicSection) {
            if (dyn.d_tag == DT_INIT) {
                std::printf("WARNING: find .init_proc\n");
                return true;
            }
        }
        uint64_t dynstrOffset = 0;
        for (const auto& dyn : dynamicSection) {
            if (dyn.d_tag == DT_STRTAB) {
                dynstrOffset = mapVATR(dyn.d_un);
                break;
            }
        }
        for (const auto& symbol : symbolTable) {
            std::string name = readStringToNull(dynstrOffset + symbol.st_name);
            if (name == "JNI_OnLoad") {
                std::printf("WARNING: find JNI_OnLoad\n");
                return true;
            }
        }
        for (const auto& section : sectionTable) {
            if (section.sh_type == SHT_LOUSER) {
                std::printf("WARNING: find SHT_LOUSER section\n");
                return true;
            }
        }
    } catch (const std::exception&) {
    }
    return false;
}

uint64_t Elf::getRVA(uint64_t pointer) {
    if (isDumped) {
        return pointer - imageBase;
    }
    return pointer;
}

void Elf::fixedProgramSegment() {
    for (size_t i = 0; i < programSegment.size(); i++) {
        setPosition(elfHeader.e_phoff + i * 32u + 4u);
        auto& phdr = programSegment[i];
        phdr.p_offset = phdr.p_vaddr;
        writeU32(phdr.p_offset);
        phdr.p_vaddr += static_cast<uint32_t>(imageBase);
        writeU32(phdr.p_vaddr);
        seek(4);
        phdr.p_filesz = phdr.p_memsz;
        writeU32(phdr.p_filesz);
    }
}

void Elf::fixedDynamicSection() {
    for (size_t i = 0; i < dynamicSection.size(); i++) {
        setPosition(pt_dynamic.p_offset + i * 8 + 4);
        auto& dyn = dynamicSection[i];
        switch (dyn.d_tag) {
            case DT_PLTGOT:
            case DT_HASH:
            case DT_STRTAB:
            case DT_SYMTAB:
            case DT_RELA:
            case DT_INIT:
            case DT_FINI:
            case DT_REL:
            case DT_JMPREL:
            case DT_INIT_ARRAY:
            case DT_FINI_ARRAY:
                dyn.d_un += static_cast<uint32_t>(imageBase);
                writeU32(dyn.d_un);
                break;
        }
    }
}

SectionHelper Elf::getSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) {
    std::vector<Elf32_Phdr> dataList;
    std::vector<Elf32_Phdr> execList;
    for (const auto& phdr : programSegment) {
        if (phdr.p_memsz != 0) {
            switch (phdr.p_flags) {
                case 1u:
                case 3u:
                case 5u:
                case 7u:
                    execList.push_back(phdr);
                    break;
                case 2u:
                case 4u:
                case 6u:
                    dataList.push_back(phdr);
                    break;
            }
        }
    }
    SectionHelper sectionHelper(this, methodCount, typeDefinitionsCount, metadataUsagesCount, imageCount);
    sectionHelper.setSection(SearchSectionType::Exec, execList);
    sectionHelper.setSection(SearchSectionType::Data, dataList);
    sectionHelper.setSection(SearchSectionType::Bss, dataList);
    return sectionHelper;
}

}
