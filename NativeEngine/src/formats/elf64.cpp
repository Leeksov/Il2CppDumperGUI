#include "formats/elf64.h"

#include "utils/section_helper.h"

#include <cstdio>
#include <stdexcept>

namespace il2cppdumper {

using namespace ElfConstants;

Elf64::Elf64(std::vector<uint8_t> data) : ElfBase(std::move(data)) {
    load();
}

void Elf64::load() {
    elfHeader = readClassAt<Elf64_Ehdr>(0);
    programSegment = readClassArrayAt<Elf64_Phdr>(elfHeader.e_phoff, elfHeader.e_phnum);
    if (isDumped) {
        fixedProgramSegment();
    }
    for (const auto& phdr : programSegment) {
        if (phdr.p_type == PT_DYNAMIC) {
            pt_dynamic = phdr;
            break;
        }
    }
    dynamicSection = readClassArrayAt<Elf64_Dyn>(pt_dynamic.p_offset, pt_dynamic.p_filesz / 16L);
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

bool Elf64::checkSection() {
    try {
        std::vector<std::string> names;
        sectionTable = readClassArrayAt<Elf64_Shdr>(elfHeader.e_shoff, elfHeader.e_shnum);
        if (elfHeader.e_shstrndx >= sectionTable.size()) {
            return false;
        }
        uint32_t shstrndx = static_cast<uint32_t>(sectionTable[elfHeader.e_shstrndx].sh_offset);
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

uint64_t Elf64::mapVATR(uint64_t addr) {
    for (const auto& phdr : programSegment) {
        if (addr >= phdr.p_vaddr && addr <= phdr.p_vaddr + phdr.p_memsz) {
            return addr - phdr.p_vaddr + phdr.p_offset;
        }
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "mapVATR: address not in any segment (0x%llx)",
                  static_cast<unsigned long long>(addr));
    throw std::runtime_error(buf);
}

uint64_t Elf64::mapRTVA(uint64_t addr) {
    for (const auto& phdr : programSegment) {
        if (addr >= phdr.p_offset && addr <= phdr.p_offset + phdr.p_filesz) {
            return addr - phdr.p_offset + phdr.p_vaddr;
        }
    }
    return 0;
}

bool Elf64::search() {
    return false;
}

bool Elf64::plusSearch(int methodCount, int typeDefinitionsCount, int imageCount) {
    auto sectionHelper = getSectionHelper(methodCount, typeDefinitionsCount, imageCount);
    uint64_t codeRegistration = sectionHelper.findCodeRegistration();
    uint64_t metadataRegistration = sectionHelper.findMetadataRegistration();
    return autoPlusInit(codeRegistration, metadataRegistration);
}

bool Elf64::symbolSearch() {
    uint64_t codeRegistration = 0;
    uint64_t metadataRegistration = 0;
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
        std::printf("CodeRegistration : %llx\n", static_cast<unsigned long long>(codeRegistration));
        std::printf("MetadataRegistration : %llx\n", static_cast<unsigned long long>(metadataRegistration));
        init(codeRegistration, metadataRegistration);
        return true;
    }
    std::printf("ERROR: No symbol is detected\n");
    return false;
}

void Elf64::readSymbol() {
    try {
        uint32_t symbolCount = 0;
        const Elf64_Dyn* hash = nullptr;
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
            const Elf64_Dyn* gnuHash = nullptr;
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
            uint64_t bucketsAddress = addr + 16 + (8ull * bloom_size);
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
        symbolTable = readClassArrayAt<Elf64_Sym>(dynsymOffset, symbolCount);
    } catch (const std::exception&) {
    }
}

void Elf64::relocationProcessing() {
    std::printf("Applying relocations...\n");
    try {
        uint64_t relaOffset = 0;
        uint64_t relaSize = 0;
        for (const auto& dyn : dynamicSection) {
            if (dyn.d_tag == DT_RELA) {
                relaOffset = mapVATR(dyn.d_un);
            } else if (dyn.d_tag == DT_RELASZ) {
                relaSize = dyn.d_un;
            }
        }
        auto relaTable = readClassArrayAt<Elf64_Rela>(relaOffset, relaSize / 24L);
        for (const auto& rela : relaTable) {
            uint64_t type = rela.r_info & 0xffffffff;
            uint64_t sym = rela.r_info >> 32;
            bool recognized = false;
            uint64_t value = 0;
            if (type == R_AARCH64_ABS64 && elfHeader.e_machine == EM_AARCH64) {
                if (sym < symbolTable.size()) {
                    value = symbolTable[static_cast<size_t>(sym)].st_value + rela.r_addend;
                    recognized = true;
                }
            } else if (type == R_AARCH64_RELATIVE && elfHeader.e_machine == EM_AARCH64) {
                value = rela.r_addend;
                recognized = true;
            } else if (type == R_X86_64_64 && elfHeader.e_machine == EM_X86_64) {
                if (sym < symbolTable.size()) {
                    value = symbolTable[static_cast<size_t>(sym)].st_value + rela.r_addend;
                    recognized = true;
                }
            } else if (type == R_X86_64_RELATIVE && elfHeader.e_machine == EM_X86_64) {
                value = rela.r_addend;
                recognized = true;
            }
            if (recognized) {
                setPosition(mapVATR(rela.r_offset));
                writeU64(value);
            }
        }
    } catch (const std::exception&) {
    }
}

bool Elf64::checkProtection() {
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

uint64_t Elf64::getRVA(uint64_t pointer) {
    if (isDumped) {
        return pointer - imageBase;
    }
    return pointer;
}

void Elf64::fixedProgramSegment() {
    for (size_t i = 0; i < programSegment.size(); i++) {
        setPosition(elfHeader.e_phoff + i * 56u + 8u);
        auto& phdr = programSegment[i];
        phdr.p_offset = phdr.p_vaddr;
        writeU64(phdr.p_offset);
        phdr.p_vaddr += imageBase;
        writeU64(phdr.p_vaddr);
        seek(8);
        phdr.p_filesz = phdr.p_memsz;
        writeU64(phdr.p_filesz);
    }
}

void Elf64::fixedDynamicSection() {
    for (size_t i = 0; i < dynamicSection.size(); i++) {
        setPosition(pt_dynamic.p_offset + i * 16 + 8);
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
                dyn.d_un += imageBase;
                writeU64(dyn.d_un);
                break;
        }
    }
}

SectionHelper Elf64::getSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) {
    std::vector<Elf64_Phdr> dataList;
    std::vector<Elf64_Phdr> execList;
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
