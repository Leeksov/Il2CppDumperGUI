#pragma once

#include "il2cpp/il2cpp.h"

#include <cstdint>
#include <string>
#include <vector>

namespace il2cppdumper {

struct MachoSection {
    std::string sectname;
    uint32_t addr = 0;
    uint32_t size = 0;
    uint32_t offset = 0;
    uint32_t flags = 0;
};

struct MachoSection64Bit {
    std::string sectname;
    uint64_t addr = 0;
    uint64_t size = 0;
    uint64_t offset = 0;
    uint32_t flags = 0;
};

class Macho : public Il2Cpp {
public:
    explicit Macho(std::vector<uint8_t> data);

    void init(uint64_t codeRegistration, uint64_t metadataRegistration) override;
    uint64_t mapVATR(uint64_t addr) override;
    uint64_t mapRTVA(uint64_t addr) override;
    bool search() override;
    bool plusSearch(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool symbolSearch() override { return false; }
    uint64_t getRVA(uint64_t pointer) override;
    SectionHelper getSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool checkDump() override { return false; }

private:
    std::vector<MachoSection> sections;
    uint64_t vmaddr = 0;
};

}
