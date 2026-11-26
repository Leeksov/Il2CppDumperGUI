#pragma once

#include "formats/elf_classes.h"
#include "il2cpp/il2cpp.h"

#include <cstdint>
#include <vector>

namespace il2cppdumper {

class ElfBase : public Il2Cpp {
public:
    explicit ElfBase(std::vector<uint8_t> data) : Il2Cpp(std::move(data)) {}

    bool checkDump() override { return !checkSection(); }

    void reload() { load(); }

protected:
    virtual void load() = 0;
    virtual bool checkSection() = 0;
};

class Elf : public ElfBase {
public:
    explicit Elf(std::vector<uint8_t> data);

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

    Elf32_Ehdr elfHeader;
    std::vector<Elf32_Phdr> programSegment;
    std::vector<Elf32_Dyn> dynamicSection;
    std::vector<Elf32_Sym> symbolTable;
    std::vector<Elf32_Shdr> sectionTable;
    Elf32_Phdr pt_dynamic;
};

}
