#pragma once

#include "formats/elf.h"

#include <cstdint>
#include <vector>

namespace il2cppdumper {

class Elf64 : public ElfBase {
public:
    explicit Elf64(std::vector<uint8_t> data);

    uint64_t mapVATR(uint64_t addr) override;
    uint64_t mapRTVA(uint64_t addr) override;
    bool search() override;
    bool plusSearch(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool symbolSearch() override;
    uint64_t getRVA(uint64_t pointer) override;
    SectionHelper getSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) override;

protected:
    void load() override;
    bool checkSection() override;

private:
    void readSymbol();
    void relocationProcessing();
    bool checkProtection();
    void fixedProgramSegment();
    void fixedDynamicSection();

    Elf64_Ehdr elfHeader;
    std::vector<Elf64_Phdr> programSegment;
    std::vector<Elf64_Dyn> dynamicSection;
    std::vector<Elf64_Sym> symbolTable;
    std::vector<Elf64_Shdr> sectionTable;
    Elf64_Phdr pt_dynamic;
};

}
