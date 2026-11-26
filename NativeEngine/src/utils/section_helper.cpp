#include "utils/section_helper.h"

#include "formats/elf.h"
#include "utils/pattern.h"

#include <algorithm>
#include <cstdint>

namespace il2cppdumper {

static const std::vector<uint8_t> kFeatureBytes = {0x6D, 0x73, 0x63, 0x6F, 0x72, 0x6C, 0x69, 0x62, 0x2E,
                                                   0x64, 0x6C, 0x6C, 0x00};

SectionHelper::SectionHelper(Il2Cpp* il2Cpp, int methodCount, int typeDefinitionsCount, int64_t metadataUsagesCount,
                             int imageCount)
    : il2Cpp(il2Cpp),
      methodCount(methodCount),
      typeDefinitionsCount(typeDefinitionsCount),
      metadataUsagesCount(metadataUsagesCount),
      imageCount(imageCount) {}

void SectionHelper::setSection(SearchSectionType type, const std::vector<SearchSection>& secs) {
    switch (type) {
        case SearchSectionType::Exec:
            exec_ = secs;
            break;
        case SearchSectionType::Data:
            data_ = secs;
            break;
        case SearchSectionType::Bss:
            bss_ = secs;
            break;
    }
}

void SectionHelper::setSection(SearchSectionType type, const std::vector<Elf32_Phdr>& sections) {
    std::vector<SearchSection> secs;
    for (const auto& section : sections) {
        SearchSection s;
        s.offset = section.p_offset;
        s.offsetEnd = static_cast<uint64_t>(section.p_offset) + section.p_filesz;
        s.address = section.p_vaddr;
        s.addressEnd = static_cast<uint64_t>(section.p_vaddr) + section.p_memsz;
        secs.push_back(s);
    }
    setSection(type, secs);
}

void SectionHelper::setSection(SearchSectionType type, const std::vector<Elf64_Phdr>& sections) {
    std::vector<SearchSection> secs;
    for (const auto& section : sections) {
        SearchSection s;
        s.offset = section.p_offset;
        s.offsetEnd = section.p_offset + section.p_filesz;
        s.address = section.p_vaddr;
        s.addressEnd = section.p_vaddr + section.p_memsz;
        secs.push_back(s);
    }
    setSection(type, secs);
}

void SectionHelper::setSection(SearchSectionType type, const std::vector<MachoSection>& sections) {
    std::vector<SearchSection> secs;
    for (const auto& section : sections) {
        SearchSection s;
        s.offset = section.offset;
        s.offsetEnd = static_cast<uint64_t>(section.offset) + section.size;
        s.address = section.addr;
        s.addressEnd = static_cast<uint64_t>(section.addr) + section.size;
        secs.push_back(s);
    }
    setSection(type, secs);
}

void SectionHelper::setSection(SearchSectionType type, const std::vector<MachoSection64Bit>& sections) {
    std::vector<SearchSection> secs;
    for (const auto& section : sections) {
        SearchSection s;
        s.offset = section.offset;
        s.offsetEnd = section.offset + section.size;
        s.address = section.addr;
        s.addressEnd = section.addr + section.size;
        secs.push_back(s);
    }
    setSection(type, secs);
}

void SectionHelper::setSection(SearchSectionType type, uint64_t imageBase, const std::vector<SectionHeader>& sections) {
    std::vector<SearchSection> secs;
    for (const auto& section : sections) {
        SearchSection s;
        s.offset = section.PointerToRawData;
        s.offsetEnd = static_cast<uint64_t>(section.PointerToRawData) + section.SizeOfRawData;
        s.address = static_cast<uint64_t>(section.VirtualAddress) + imageBase;
        s.addressEnd = static_cast<uint64_t>(section.VirtualAddress) + section.VirtualSize + imageBase;
        secs.push_back(s);
    }
    setSection(type, secs);
}

void SectionHelper::setSection(SearchSectionType type, const std::vector<NSOSegmentHeader>& sections) {
    std::vector<SearchSection> secs;
    for (const auto& section : sections) {
        SearchSection s;
        s.offset = section.FileOffset;
        s.offsetEnd = static_cast<uint64_t>(section.FileOffset) + section.DecompressedSize;
        s.address = section.MemoryOffset;
        s.addressEnd = static_cast<uint64_t>(section.MemoryOffset) + section.DecompressedSize;
        secs.push_back(s);
    }
    setSection(type, secs);
}

uint64_t SectionHelper::findCodeRegistration() {
    if (il2Cpp->version >= 24.2) {
        uint64_t codeRegistration;
        if (dynamic_cast<ElfBase*>(il2Cpp) != nullptr) {
            codeRegistration = findCodeRegistration2019(exec_);
            if (codeRegistration == 0) {
                codeRegistration = findCodeRegistration2019(data_);
            } else {
                pointerInExec = true;
            }
        } else {
            codeRegistration = findCodeRegistration2019(data_);
            if (codeRegistration == 0) {
                codeRegistration = findCodeRegistration2019(exec_);
                pointerInExec = true;
            }
        }
        return codeRegistration;
    }
    return findCodeRegistrationOld();
}

uint64_t SectionHelper::findMetadataRegistration() {
    if (il2Cpp->version < 19) {
        return 0;
    }
    if (il2Cpp->version >= 27) {
        return findMetadataRegistrationV21();
    }
    return findMetadataRegistrationOld();
}

uint64_t SectionHelper::findCodeRegistrationOld() {
    for (const auto& section : data_) {
        il2Cpp->setPosition(section.offset);
        while (il2Cpp->position() < section.offsetEnd) {
            size_t addr = il2Cpp->position();
            if (il2Cpp->readIPtr() == methodCount) {
                try {
                    uint64_t pointer = il2Cpp->mapVATR(il2Cpp->readUPtr());
                    if (checkPointerRangeDataRa(pointer)) {
                        auto pointers = il2Cpp->readClassArrayAt<uint64_t>(pointer, methodCount);
                        if (checkPointerRangeExecVa(pointers)) {
                            return addr - section.offset + section.address;
                        }
                    }
                } catch (const std::exception&) {
                }
            }
            il2Cpp->setPosition(addr + il2Cpp->pointerSize());
        }
    }
    return 0;
}

uint64_t SectionHelper::findMetadataRegistrationOld() {
    for (const auto& section : data_) {
        il2Cpp->setPosition(section.offset);
        size_t end = std::min<uint64_t>(section.offsetEnd, il2Cpp->length()) - il2Cpp->pointerSize();
        while (il2Cpp->position() < end) {
            size_t addr = il2Cpp->position();
            if (il2Cpp->readIPtr() == typeDefinitionsCount) {
                try {
                    il2Cpp->seek(il2Cpp->pointerSize() * 2);
                    uint64_t pointer = il2Cpp->mapVATR(il2Cpp->readUPtr());
                    if (checkPointerRangeDataRa(pointer)) {
                        auto pointers = il2Cpp->readClassArrayAt<uint64_t>(pointer, metadataUsagesCount);
                        if (checkPointerRangeBssVa(pointers)) {
                            return addr - il2Cpp->pointerSize() * 12 - section.offset + section.address;
                        }
                    }
                } catch (const std::exception&) {
                }
            }
            il2Cpp->setPosition(addr + il2Cpp->pointerSize());
        }
    }
    return 0;
}

uint64_t SectionHelper::findMetadataRegistrationV21() {
    for (const auto& section : data_) {
        il2Cpp->setPosition(section.offset);
        size_t end = std::min<uint64_t>(section.offsetEnd, il2Cpp->length()) - il2Cpp->pointerSize();
        while (il2Cpp->position() < end) {
            size_t addr = il2Cpp->position();
            if (il2Cpp->readIPtr() == typeDefinitionsCount) {
                il2Cpp->seek(il2Cpp->pointerSize());
                if (il2Cpp->readIPtr() == typeDefinitionsCount) {
                    try {
                        uint64_t pointer = il2Cpp->mapVATR(il2Cpp->readUPtr());
                        if (checkPointerRangeDataRa(pointer)) {
                            auto pointers = il2Cpp->readClassArrayAt<uint64_t>(pointer, typeDefinitionsCount);
                            bool flag;
                            if (pointerInExec) {
                                flag = checkPointerRangeExecVa(pointers);
                            } else {
                                flag = checkPointerRangeDataVa(pointers);
                            }
                            if (flag) {
                                return addr - il2Cpp->pointerSize() * 10 - section.offset + section.address;
                            }
                        }
                    } catch (const std::exception&) {
                    }
                }
            }
            il2Cpp->setPosition(addr + il2Cpp->pointerSize());
        }
    }
    return 0;
}

bool SectionHelper::checkPointerRangeDataRa(uint64_t pointer) const {
    for (const auto& s : data_) {
        if (pointer >= s.offset && pointer <= s.offsetEnd) {
            return true;
        }
    }
    return false;
}

bool SectionHelper::checkPointerRangeExecVa(const std::vector<uint64_t>& pointers) const {
    for (uint64_t x : pointers) {
        bool found = false;
        for (const auto& y : exec_) {
            if (x >= y.address && x <= y.addressEnd) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool SectionHelper::checkPointerRangeDataVa(const std::vector<uint64_t>& pointers) const {
    for (uint64_t x : pointers) {
        bool found = false;
        for (const auto& y : data_) {
            if (x >= y.address && x <= y.addressEnd) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool SectionHelper::checkPointerRangeBssVa(const std::vector<uint64_t>& pointers) const {
    for (uint64_t x : pointers) {
        bool found = false;
        for (const auto& y : bss_) {
            if (x >= y.address && x <= y.addressEnd) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

uint64_t SectionHelper::findCodeRegistration2019(const std::vector<SearchSection>& secs) {
    for (const auto& sec : secs) {
        il2Cpp->setPosition(sec.offset);
        auto buff = il2Cpp->readBytes(sec.offsetEnd - sec.offset);
        for (size_t index : searchPattern(buff, kFeatureBytes)) {
            uint64_t dllva = static_cast<uint64_t>(index) + sec.address;
            for (uint64_t refva : findReference(dllva)) {
                for (uint64_t refva2 : findReference(refva)) {
                    if (il2Cpp->version >= 27) {
                        for (int i = imageCount - 1; i >= 0; i--) {
                            for (uint64_t refva3 : findReference(refva2 - static_cast<uint64_t>(i) * il2Cpp->pointerSize())) {
                                il2Cpp->setPosition(il2Cpp->mapVATR(refva3 - il2Cpp->pointerSize()));
                                if (il2Cpp->readIPtr() == imageCount) {
                                    if (il2Cpp->version >= 35) {
                                        return refva3 - il2Cpp->pointerSize() * 16;
                                    }
                                    if (il2Cpp->version >= 29) {
                                        return refva3 - il2Cpp->pointerSize() * 14;
                                    }
                                    return refva3 - il2Cpp->pointerSize() * 13;
                                }
                            }
                        }
                    } else {
                        for (int i = 0; i < imageCount; i++) {
                            auto refs = findReference(refva2 - static_cast<uint64_t>(i) * il2Cpp->pointerSize());
                            if (!refs.empty()) {
                                return refs.front() - il2Cpp->pointerSize() * 13;
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}

std::vector<uint64_t> SectionHelper::findReference(uint64_t addr) {
    std::vector<uint64_t> results;
    for (const auto& dataSec : data_) {
        size_t position = dataSec.offset;
        size_t end = std::min<uint64_t>(dataSec.offsetEnd, il2Cpp->length()) - il2Cpp->pointerSize();
        while (position < end) {
            il2Cpp->setPosition(position);
            if (il2Cpp->readUPtr() == addr) {
                results.push_back(position - dataSec.offset + dataSec.address);
            }
            position += il2Cpp->pointerSize();
        }
    }
    return results;
}

}
