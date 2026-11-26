#include "formats/macho64.h"

#include "utils/arm_utils.h"
#include "utils/section_helper.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace il2cppdumper {

static const uint8_t kFeatureBytes1[4] = {0x2, 0x0, 0x80, 0xD2};
static const uint8_t kFeatureBytes2[4] = {0x3, 0x0, 0x80, 0x52};

Macho64::Macho64(std::vector<uint8_t> data) : Il2Cpp(std::move(data)) {
    seek(16);
    uint32_t ncmds = readU32();
    seek(12);
    for (uint32_t i = 0; i < ncmds; i++) {
        size_t pos = position();
        uint32_t cmd = readU32();
        uint32_t cmdsize = readU32();
        switch (cmd) {
            case 0x19: {
                auto segnameBytes = readBytes(16);
                std::string segname(segnameBytes.begin(), segnameBytes.end());
                size_t nul = segname.find('\0');
                if (nul != std::string::npos) {
                    segname.resize(nul);
                }
                if (segname == "__TEXT") {
                    vmaddr = readU64();
                } else {
                    seek(8);
                }
                seek(32);
                uint32_t nsects = readU32();
                seek(4);
                for (uint32_t j = 0; j < nsects; j++) {
                    MachoSection64Bit section;
                    sections.push_back(section);
                    auto nameBytes = readBytes(16);
                    sections.back().sectname.assign(nameBytes.begin(), nameBytes.end());
                    nul = sections.back().sectname.find('\0');
                    if (nul != std::string::npos) {
                        sections.back().sectname.resize(nul);
                    }
                    seek(16);
                    sections.back().addr = readU64();
                    sections.back().size = readU64();
                    sections.back().offset = readU32();
                    seek(12);
                    sections.back().flags = readU32();
                    seek(12);
                }
                break;
            }
            case 0x2C: {
                seek(8);
                uint32_t cryptID = readU32();
                if (cryptID != 0) {
                    std::printf("ERROR: This Mach-O executable is encrypted and cannot be processed.\n");
                }
                break;
            }
        }
        setPosition(pos + cmdsize);
    }
}

uint64_t Macho64::mapVATR(uint64_t addr) {
    for (const auto& section : sections) {
        if (addr >= section.addr && addr <= section.addr + section.size) {
            if (section.sectname == "__bss") {
                throw std::runtime_error("mapVATR: __bss has no file data");
            }
            return addr - section.addr + section.offset;
        }
    }
    throw std::runtime_error("mapVATR: address not in any section");
}

uint64_t Macho64::mapRTVA(uint64_t addr) {
    for (const auto& section : sections) {
        if (addr >= section.offset && addr <= section.offset + section.size) {
            if (section.sectname == "__bss") {
                throw std::runtime_error("mapRTVA: __bss has no file data");
            }
            return addr - section.offset + section.addr;
        }
    }
    return 0;
}

uint64_t Macho64::readUPtr() {
    uint64_t pointer = readU64();
    if (pointer > vmaddr + 0xFFFFFFFF) {
        size_t addr = position();
        for (const auto& section : sections) {
            if (addr >= section.offset && addr <= section.offset + section.size) {
                if (section.sectname == "__const" || section.sectname == "__data") {
                    uint64_t rva = pointer - vmaddr;
                    rva &= 0xFFFFFFFF;
                    pointer = rva + vmaddr;
                }
                break;
            }
        }
    }
    return pointer;
}

bool Macho64::search() {
    uint64_t codeRegistration = 0;
    uint64_t metadataRegistration = 0;
    if (version < 23) {
        const MachoSection64Bit* modInit = nullptr;
        for (const auto& section : sections) {
            if (section.sectname == "__mod_init_func") {
                modInit = &section;
                break;
            }
        }
        if (modInit == nullptr) {
            return false;
        }
        auto addrs = readClassArrayAt<uint64_t>(modInit->offset, modInit->size / 8);
        for (uint64_t i : addrs) {
            if (i > 0) {
                bool flag = false;
                uint64_t subaddr = 0;
                setPosition(mapVATR(i));
                auto buff = readBytes(4);
                if (buff.size() == 4 && std::memcmp(buff.data(), kFeatureBytes1, 4) == 0) {
                    buff = readBytes(4);
                    if (buff.size() == 4 && std::memcmp(buff.data(), kFeatureBytes2, 4) == 0) {
                        seek(8);
                        buff = readBytes(4);
                        uint32_t inst = buff.size() == 4
                                            ? static_cast<uint32_t>(buff[0]) | (static_cast<uint32_t>(buff[1]) << 8) |
                                                  (static_cast<uint32_t>(buff[2]) << 16) |
                                                  (static_cast<uint32_t>(buff[3]) << 24)
                                            : 0;
                        if (isAdr(inst)) {
                            subaddr = decodeAdr(i + 16, inst);
                            flag = true;
                        }
                    }
                } else {
                    seek(0xc);
                    buff = readBytes(4);
                    if (buff.size() == 4 && std::memcmp(buff.data(), kFeatureBytes2, 4) == 0) {
                        buff = readBytes(4);
                        if (buff.size() == 4 && std::memcmp(buff.data(), kFeatureBytes1, 4) == 0) {
                            seek(-0x10);
                            buff = readBytes(4);
                            uint32_t inst = buff.size() == 4
                                                ? static_cast<uint32_t>(buff[0]) | (static_cast<uint32_t>(buff[1]) << 8) |
                                                      (static_cast<uint32_t>(buff[2]) << 16) |
                                                      (static_cast<uint32_t>(buff[3]) << 24)
                                                : 0;
                            if (isAdr(inst)) {
                                subaddr = decodeAdr(i + 8, inst);
                                flag = true;
                            }
                        }
                    }
                }
                if (flag) {
                    uint64_t rsubaddr = mapVATR(subaddr);
                    setPosition(rsubaddr);
                    codeRegistration = decodeAdrp(subaddr, readU32());
                    codeRegistration += decodeAdd(readU32());
                    setPosition(rsubaddr + 8);
                    metadataRegistration = decodeAdrp(subaddr + 8, readU32());
                    metadataRegistration += decodeAdd(readU32());
                }
            }
        }
    }
    if (version == 23) {
        /* ADRP X0, unk
         * ADD X0, X0, unk
         * ADR X1, sub
         * NOP
         * MOV X2, #0
         * MOV W3, #0
         * B sub
         */
        const MachoSection64Bit* modInit = nullptr;
        for (const auto& section : sections) {
            if (section.sectname == "__mod_init_func") {
                modInit = &section;
                break;
            }
        }
        if (modInit == nullptr) {
            return false;
        }
        auto addrs = readClassArrayAt<uint64_t>(modInit->offset, modInit->size / 8);
        for (uint64_t i : addrs) {
            if (i > 0) {
                setPosition(mapVATR(i) + 16);
                auto buff = readBytes(4);
                if (buff.size() == 4 && std::memcmp(buff.data(), kFeatureBytes1, 4) == 0) {
                    buff = readBytes(4);
                    if (buff.size() == 4 && std::memcmp(buff.data(), kFeatureBytes2, 4) == 0) {
                        seek(-16);
                        buff = readBytes(4);
                        uint32_t inst = static_cast<uint32_t>(buff[0]) | (static_cast<uint32_t>(buff[1]) << 8) |
                                        (static_cast<uint32_t>(buff[2]) << 16) | (static_cast<uint32_t>(buff[3]) << 24);
                        uint64_t subaddr = decodeAdr(i + 8, inst);
                        uint64_t rsubaddr = mapVATR(subaddr);
                        setPosition(rsubaddr);
                        codeRegistration = decodeAdrp(subaddr, readU32());
                        codeRegistration += decodeAdd(readU32());
                        setPosition(rsubaddr + 8);
                        metadataRegistration = decodeAdrp(subaddr + 8, readU32());
                        metadataRegistration += decodeAdd(readU32());
                    }
                }
            }
        }
    }
    if (version >= 24) {
        /* ADRP X0, unk
         * ADD X0, X0, unk
         * ADR X1, sub
         * NOP
         * MOV W3, #0
         * MOV X2, #0
         * B sub
         */
        const MachoSection64Bit* modInit = nullptr;
        for (const auto& section : sections) {
            if (section.sectname == "__mod_init_func") {
                modInit = &section;
                break;
            }
        }
        if (modInit == nullptr) {
            return false;
        }
        auto addrs = readClassArrayAt<uint64_t>(modInit->offset, modInit->size / 8);
        for (uint64_t i : addrs) {
            if (i > 0) {
                setPosition(mapVATR(i) + 16);
                auto buff = readBytes(4);
                if (buff.size() == 4 && std::memcmp(buff.data(), kFeatureBytes2, 4) == 0) {
                    buff = readBytes(4);
                    if (buff.size() == 4 && std::memcmp(buff.data(), kFeatureBytes1, 4) == 0) {
                        seek(-16);
                        buff = readBytes(4);
                        uint32_t inst = static_cast<uint32_t>(buff[0]) | (static_cast<uint32_t>(buff[1]) << 8) |
                                        (static_cast<uint32_t>(buff[2]) << 16) | (static_cast<uint32_t>(buff[3]) << 24);
                        uint64_t subaddr = decodeAdr(i + 8, inst);
                        uint64_t rsubaddr = mapVATR(subaddr);
                        setPosition(rsubaddr);
                        codeRegistration = decodeAdrp(subaddr, readU32());
                        codeRegistration += decodeAdd(readU32());
                        setPosition(rsubaddr + 8);
                        metadataRegistration = decodeAdrp(subaddr + 8, readU32());
                        metadataRegistration += decodeAdd(readU32());
                    }
                }
            }
        }
    }
    if (codeRegistration != 0 && metadataRegistration != 0) {
        std::printf("CodeRegistration : %llx\n", static_cast<unsigned long long>(codeRegistration));
        std::printf("MetadataRegistration : %llx\n", static_cast<unsigned long long>(metadataRegistration));
        init(codeRegistration, metadataRegistration);
        return true;
    }
    return false;
}

bool Macho64::plusSearch(int methodCount, int typeDefinitionsCount, int imageCount) {
    auto sectionHelper = getSectionHelper(methodCount, typeDefinitionsCount, imageCount);
    uint64_t codeRegistration = sectionHelper.findCodeRegistration();
    uint64_t metadataRegistration = sectionHelper.findMetadataRegistration();
    return autoPlusInit(codeRegistration, metadataRegistration);
}

uint64_t Macho64::getRVA(uint64_t pointer) {
    return pointer - vmaddr;
}

SectionHelper Macho64::getSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) {
    std::vector<MachoSection64Bit> data;
    std::vector<MachoSection64Bit> code;
    std::vector<MachoSection64Bit> bss;
    for (const auto& section : sections) {
        if (section.sectname == "__const" || section.sectname == "__cstring" || section.sectname == "__data") {
            data.push_back(section);
        }
        if (section.flags == 0x80000400) {
            code.push_back(section);
        }
        if (section.flags == 1u) {
            bss.push_back(section);
        }
    }
    SectionHelper sectionHelper(this, methodCount, typeDefinitionsCount, metadataUsagesCount, imageCount);
    sectionHelper.setSection(SearchSectionType::Exec, code);
    sectionHelper.setSection(SearchSectionType::Data, data);
    sectionHelper.setSection(SearchSectionType::Bss, bss);
    return sectionHelper;
}

}
