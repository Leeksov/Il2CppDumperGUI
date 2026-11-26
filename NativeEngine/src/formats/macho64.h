#pragma once

#include "formats/macho.h"

#include <cstdint>
#include <vector>

namespace il2cppdumper {

class Macho64 : public Il2Cpp {
public:
    explicit Macho64(std::vector<uint8_t> data);

    uint64_t mapVATR(uint64_t addr) override;
    uint64_t mapRTVA(uint64_t addr) override;
    bool search() override;
    bool plusSearch(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool symbolSearch() override { return false; }
    uint64_t getRVA(uint64_t pointer) override;
    SectionHelper getSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool checkDump() override { return false; }
    uint64_t readUPtr() override;

private:
    std::vector<MachoSection64Bit> sections;
    uint64_t vmaddr = 0;
};

}
