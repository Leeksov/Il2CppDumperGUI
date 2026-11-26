#include "formats/pe.h"

#include "utils/section_helper.h"

#include <stdexcept>

namespace il2cppdumper {

PE::PE(std::vector<uint8_t> data) : Il2Cpp(std::move(data)) {
    auto dosHeader = readClassAt<DosHeader>(0);
    if (dosHeader.Magic != 0x5A4D) {
        throw std::runtime_error("ERROR: Invalid PE file");
    }
    setPosition(dosHeader.Lfanew);
    if (readU32() != 0x4550u) {
        throw std::runtime_error("ERROR: Invalid PE file");
    }
    auto fileHeader = readClass<FileHeader>();
    size_t pos = position();
    uint16_t magic = readU16();
    seek(-2);
    if (magic == 0x10b) {
        is32Bit = true;
        auto optionalHeader = readClass<OptionalHeader>();
        imageBase = optionalHeader.ImageBase;
    } else if (magic == 0x20b) {
        auto optionalHeader = readClass<OptionalHeader64>();
        imageBase = optionalHeader.ImageBase;
    } else {
        throw std::runtime_error("Invalid Optional header magic");
    }
    setPosition(pos + fileHeader.SizeOfOptionalHeader);
    sections = readClassArray<SectionHeader>(fileHeader.NumberOfSections);
}

void PE::loadFromMemory(uint64_t addr) {
    imageBase = addr;
    for (auto& section : sections) {
        section.PointerToRawData = section.VirtualAddress;
        section.SizeOfRawData = section.VirtualSize;
    }
}

uint64_t PE::mapVATR(uint64_t absAddr) {
    if (absAddr < imageBase) {
        return 0;
    }
    uint64_t addr = absAddr - imageBase;
    for (const auto& section : sections) {
        if (addr >= section.VirtualAddress && addr <= static_cast<uint64_t>(section.VirtualAddress) + section.VirtualSize) {
            return addr - section.VirtualAddress + section.PointerToRawData;
        }
    }
    return 0;
}

uint64_t PE::mapRTVA(uint64_t addr) {
    for (const auto& section : sections) {
        if (addr >= section.PointerToRawData && addr <= static_cast<uint64_t>(section.PointerToRawData) + section.SizeOfRawData) {
            return addr - section.PointerToRawData + section.VirtualAddress + imageBase;
        }
    }
    return 0;
}

bool PE::plusSearch(int methodCount, int typeDefinitionsCount, int imageCount) {
    auto sectionHelper = getSectionHelper(methodCount, typeDefinitionsCount, imageCount);
    uint64_t codeRegistration = sectionHelper.findCodeRegistration();
    uint64_t metadataRegistration = sectionHelper.findMetadataRegistration();
    return autoPlusInit(codeRegistration, metadataRegistration);
}

uint64_t PE::getRVA(uint64_t pointer) {
    return pointer - imageBase;
}

SectionHelper PE::getSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) {
    std::vector<SectionHeader> execList;
    std::vector<SectionHeader> dataList;
    for (const auto& section : sections) {
        switch (section.Characteristics) {
            case 0x60000020:
                execList.push_back(section);
                break;
            case 0x40000040:
            case 0xC0000040:
                dataList.push_back(section);
                break;
        }
    }
    SectionHelper sectionHelper(this, methodCount, typeDefinitionsCount, metadataUsagesCount, imageCount);
    sectionHelper.setSection(SearchSectionType::Exec, imageBase, execList);
    sectionHelper.setSection(SearchSectionType::Data, imageBase, dataList);
    sectionHelper.setSection(SearchSectionType::Bss, imageBase, dataList);
    return sectionHelper;
}

bool PE::checkDump() {
    if (is32Bit) {
        return imageBase != 0x10000000;
    }
    return imageBase != 0x180000000;
}

}
