#include "formats/pe_loader.h"

#include <stdexcept>

namespace il2cppdumper {

std::unique_ptr<PE> PELoader::load(const std::string& fileName, const std::vector<uint8_t>& buff) {
    (void)fileName;
    BinaryStream reader(buff);
    auto dosHeader = reader.readClassAt<DosHeader>(0);
    if (dosHeader.Magic != 0x5A4D) {
        throw std::runtime_error("ERROR: Invalid PE file");
    }
    reader.setPosition(dosHeader.Lfanew);
    if (reader.readU32() != 0x4550u) {
        throw std::runtime_error("ERROR: Invalid PE file");
    }
    auto fileHeader = reader.readClass<FileHeader>();
    size_t pos = reader.position();
    reader.seek(pos + fileHeader.SizeOfOptionalHeader);
    auto sections = reader.readClassArray<SectionHeader>(fileHeader.NumberOfSections);
    if (sections.empty()) {
        throw std::runtime_error("ERROR: PE file has no sections");
    }
    const auto& last = sections.back();
    size_t size = static_cast<size_t>(last.VirtualAddress) + last.VirtualSize;
    if (size == 0) {
        throw std::runtime_error("ERROR: PE file has invalid size");
    }
    std::vector<uint8_t> peBuff(size, 0);
    for (const auto& section : sections) {
        switch (section.Characteristics) {
            case 0x60000020:
            case 0x40000040:
            case 0xC0000040: {
                if (section.PointerToRawData < buff.size()) {
                    size_t n = section.VirtualSize;
                    if (section.PointerToRawData + n > buff.size()) {
                        n = buff.size() - section.PointerToRawData;
                    }
                    if (section.VirtualAddress < peBuff.size()) {
                        if (section.VirtualAddress + n > peBuff.size()) {
                            n = peBuff.size() - section.VirtualAddress;
                        }
                        std::copy(buff.begin() + section.PointerToRawData,
                                  buff.begin() + section.PointerToRawData + n,
                                  peBuff.begin() + section.VirtualAddress);
                    }
                }
                break;
            }
        }
    }
    size_t headerSize = reader.position();
    if (headerSize > peBuff.size()) {
        headerSize = peBuff.size();
    }
    std::copy(buff.begin(), buff.begin() + headerSize, peBuff.begin());
    auto pe = std::make_unique<PE>(std::move(peBuff));
    pe->loadFromMemory(0);
    return pe;
}

}
