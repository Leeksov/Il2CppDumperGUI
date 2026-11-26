#pragma once

#include "formats/elf_classes.h"
#include "il2cpp/il2cpp.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace il2cppdumper {

struct NSOSegmentHeader {
    uint32_t FileOffset = 0;
    uint32_t MemoryOffset = 0;
    uint32_t DecompressedSize = 0;
};

struct NSORelativeExtent {
    uint32_t RegionRoDataOffset = 0;
    uint32_t RegionSize = 0;
};

struct NSOHeader {
    uint32_t Magic = 0;
    uint32_t Version = 0;
    uint32_t Reserved = 0;
    uint32_t Flags = 0;
    NSOSegmentHeader TextSegment;
    uint32_t ModuleOffset = 0;
    NSOSegmentHeader RoDataSegment;
    uint32_t ModuleFileSize = 0;
    NSOSegmentHeader DataSegment;
    uint32_t BssSize = 0;
    std::vector<uint8_t> DigestBuildID;
    uint32_t TextCompressedSize = 0;
    uint32_t RoDataCompressedSize = 0;
    uint32_t DataCompressedSize = 0;
    std::vector<uint8_t> Padding;
    NSORelativeExtent APIInfo;
    NSORelativeExtent DynStr;
    NSORelativeExtent DynSym;
    std::vector<uint8_t> TextHash;
    std::vector<uint8_t> RoDataHash;
    std::vector<uint8_t> DataHash;
    NSOSegmentHeader BssSegment;
};

class NSO : public Il2Cpp {
public:
    explicit NSO(std::vector<uint8_t> data);

    std::unique_ptr<NSO> unCompress();

    uint64_t mapVATR(uint64_t addr) override;
    uint64_t mapRTVA(uint64_t addr) override;
    bool search() override { return false; }
    bool plusSearch(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool symbolSearch() override { return false; }
    SectionHelper getSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool checkDump() override { return false; }

private:
    void readSymbol();
    void relocationProcessing();

    NSOHeader header;
    bool isTextCompressed = false;
    bool isRoDataCompressed = false;
    bool isDataCompressed = false;
    std::vector<NSOSegmentHeader> segments;
    std::vector<Elf64_Sym> symbolTable;
    std::vector<Elf64_Dyn> dynamicSection;
    bool isCompressed() const { return isTextCompressed || isRoDataCompressed || isDataCompressed; }
};

}
