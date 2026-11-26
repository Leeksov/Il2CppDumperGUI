#pragma once

#include "formats/elf_classes.h"
#include "il2cpp/il2cpp.h"
#include "formats/macho.h"
#include "formats/nso.h"
#include "formats/pe_classes.h"

#include <cstdint>
#include <vector>

namespace il2cppdumper {

enum class SearchSectionType { Exec, Data, Bss };

struct SearchSection {
    uint64_t offset = 0;
    uint64_t offsetEnd = 0;
    uint64_t address = 0;
    uint64_t addressEnd = 0;
};

class SectionHelper {
public:
    SectionHelper(Il2Cpp* il2Cpp, int methodCount, int typeDefinitionsCount, int64_t metadataUsagesCount,
                  int imageCount);

    void setSection(SearchSectionType type, const std::vector<SearchSection>& secs);
    void setSection(SearchSectionType type, const std::vector<Elf32_Phdr>& sections);
    void setSection(SearchSectionType type, const std::vector<Elf64_Phdr>& sections);
    void setSection(SearchSectionType type, const std::vector<MachoSection>& sections);
    void setSection(SearchSectionType type, const std::vector<MachoSection64Bit>& sections);
    void setSection(SearchSectionType type, uint64_t imageBase, const std::vector<SectionHeader>& sections);
    void setSection(SearchSectionType type, const std::vector<NSOSegmentHeader>& sections);

    const std::vector<SearchSection>& exec() const { return exec_; }
    const std::vector<SearchSection>& data() const { return data_; }
    const std::vector<SearchSection>& bss() const { return bss_; }

    uint64_t findCodeRegistration();
    uint64_t findMetadataRegistration();

private:
    uint64_t findCodeRegistrationOld();
    uint64_t findMetadataRegistrationOld();
    uint64_t findMetadataRegistrationV21();
    uint64_t findCodeRegistration2019(const std::vector<SearchSection>& secs);
    std::vector<uint64_t> findReference(uint64_t addr);

    bool checkPointerRangeDataRa(uint64_t pointer) const;
    bool checkPointerRangeExecVa(const std::vector<uint64_t>& pointers) const;
    bool checkPointerRangeDataVa(const std::vector<uint64_t>& pointers) const;
    bool checkPointerRangeBssVa(const std::vector<uint64_t>& pointers) const;

    std::vector<SearchSection> exec_;
    std::vector<SearchSection> data_;
    std::vector<SearchSection> bss_;
    Il2Cpp* il2Cpp;
    int methodCount;
    int typeDefinitionsCount;
    int64_t metadataUsagesCount;
    int imageCount;
    bool pointerInExec = false;
};

}
