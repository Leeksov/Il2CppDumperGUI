#pragma once

#include "il2cpp/il2cpp.h"
#include "formats/pe_classes.h"

#include <cstdint>
#include <vector>

namespace il2cppdumper {

class PE : public Il2Cpp {
public:
    explicit PE(std::vector<uint8_t> data);

    void loadFromMemory(uint64_t addr);

    uint64_t mapVATR(uint64_t absAddr) override;
    uint64_t mapRTVA(uint64_t addr) override;
    bool search() override { return false; }
    bool plusSearch(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool symbolSearch() override { return false; }
    uint64_t getRVA(uint64_t pointer) override;
    SectionHelper getSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool checkDump() override;

private:
    std::vector<SectionHeader> sections;
};

}
