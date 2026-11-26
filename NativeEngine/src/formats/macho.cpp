#include "formats/macho.h"

#include "utils/arm_utils.h"
#include "utils/section_helper.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace il2cppdumper {

static const uint8_t kFeatureBytes1[2] = {0x0, 0x22};
static const uint8_t kFeatureBytes2[4] = {0x78, 0x44, 0x79, 0x44};

Macho::Macho(std::vector<uint8_t> data) : Il2Cpp(std::move(data)) {
    is32Bit = true;
    seek(16);
    uint32_t ncmds = readU32();
    seek(8);
    for (uint32_t i = 0; i < ncmds; i++) {
        size_t pos = position();
        uint32_t cmd = readU32();
        uint32_t cmdsize = readU32();
        switch (cmd) {
            case 1: {
                auto segnameBytes = readBytes(16);
                std::string segname(segnameBytes.begin(), segnameBytes.end());
                size_t nul = segname.find('\0');
                if (nul != std::string::npos) {
                    segname.resize(nul);
                }
                if (segname == "__TEXT") {
                    vmaddr = readU32();
                } else {
                    seek(4);
                }
                seek(20);
                uint32_t nsects = readU32();
                seek(4);
                for (uint32_t j = 0; j < nsects; j++) {
                    MachoSection section;
                    sections.push_back(section);
                    auto nameBytes = readBytes(16);
                    sections.back().sectname.assign(nameBytes.begin(), nameBytes.end());
                    nul = sections.back().sectname.find('\0');
                    if (nul != std::string::npos) {
                        sections.back().sectname.resize(nul);
                    }
                    seek(16);
                    sections.back().addr = readU32();
                    sections.back().size = readU32();
                    sections.back().offset = readU32();
                    seek(12);
                    sections.back().flags = readU32();
                    seek(8);
                }
                break;
            }
            case 0x21: {
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

void Macho::init(uint64_t codeRegistration, uint64_t metadataRegistration) {
    Il2Cpp::init(codeRegistration, metadataRegistration);
    for (auto& p : methodPointers) {
        p -= 1;
    }
    for (auto& p : customAttributeGenerators) {
        p -= 1;
    }
}

uint64_t Macho::mapVATR(uint64_t addr) {
    for (const auto& section : sections) {
        if (addr >= section.addr && addr <= static_cast<uint64_t>(section.addr) + section.size) {
            return addr - section.addr + section.offset;
        }
    }
    throw std::runtime_error("mapVATR: address not in any section");
}

uint64_t Macho::mapRTVA(uint64_t addr) {
    for (const auto& section : sections) {
        if (addr >= section.offset && addr <= static_cast<uint64_t>(section.offset) + section.size) {
            return addr - section.offset + section.addr;
        }
    }
    return 0;
}

bool Macho::search() {
    const MachoSection* modInit = nullptr;
    for (const auto& section : sections) {
        if (section.sectname == "__mod_init_func") {
            modInit = &section;
            break;
        }
    }
    if (modInit == nullptr) {
        return false;
    }
    auto addrs = readClassArrayAt<uint32_t>(modInit->offset, modInit->size / 4u);
    for (auto a : addrs) {
        if (a > 0) {
            uint64_t i = a - 1;
            setPosition(mapVATR(i));
            seek(4);
            auto buff = readBytes(2);
            if (buff.size() == 2 && buff[0] == kFeatureBytes1[0] && buff[1] == kFeatureBytes1[1]) {
                seek(12);
                buff = readBytes(4);
                if (buff.size() == 4 && std::memcmp(buff.data(), kFeatureBytes2, 4) == 0) {
                    setPosition(mapVATR(i) + 10);
                    uint8_t mov[8];
                    auto bytes = readBytes(8);
                    std::memcpy(mov, bytes.data(), bytes.size());
                    uint32_t subaddr = static_cast<uint32_t>(decodeMov(mov) + i + 24u - 1u);
                    uint64_t rsubaddr = mapVATR(subaddr);
                    setPosition(rsubaddr);
                    bytes = readBytes(8);
                    std::memcpy(mov, bytes.data(), bytes.size());
                    uint32_t ptr = decodeMov(mov) + subaddr + 16u;
                    setPosition(mapVATR(ptr));
                    uint32_t metadataRegistration = readU32();
                    setPosition(rsubaddr + 8);
                    buff = readBytes(4);
                    setPosition(rsubaddr + 14);
                    auto buff2 = readBytes(4);
                    buff.insert(buff.end(), buff2.begin(), buff2.end());
                    std::memcpy(mov, buff.data(), 8);
                    uint32_t codeRegistration = decodeMov(mov) + subaddr + (version < 21 ? 22u : 26u);
                    std::printf("CodeRegistration : %x\n", codeRegistration);
                    std::printf("MetadataRegistration : %x\n", metadataRegistration);
                    init(codeRegistration, metadataRegistration);
                    return true;
                }
            }
        }
    }
    return false;
}

bool Macho::plusSearch(int methodCount, int typeDefinitionsCount, int imageCount) {
    auto sectionHelper = getSectionHelper(methodCount, typeDefinitionsCount, imageCount);
    uint64_t codeRegistration = sectionHelper.findCodeRegistration();
    uint64_t metadataRegistration = sectionHelper.findMetadataRegistration();
    return autoPlusInit(codeRegistration, metadataRegistration);
}

uint64_t Macho::getRVA(uint64_t pointer) {
    return pointer - vmaddr;
}

SectionHelper Macho::getSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) {
    std::vector<MachoSection> data;
    std::vector<MachoSection> code;
    std::vector<MachoSection> bss;
    for (const auto& section : sections) {
        if (section.sectname == "__const") {
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
