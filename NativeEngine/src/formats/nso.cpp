#include "formats/nso.h"

#include "utils/section_helper.h"

#include <lz4.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace il2cppdumper {

using namespace ElfConstants;

namespace {

std::vector<uint8_t> lz4Decompress(const std::vector<uint8_t>& src, size_t dstSize) {
    std::vector<uint8_t> out(dstSize, 0);
    if (src.empty()) {
        return out;
    }
    int r = LZ4_decompress_safe(reinterpret_cast<const char*>(src.data()), reinterpret_cast<char*>(out.data()),
                                static_cast<int>(src.size()), static_cast<int>(dstSize));
    if (r < 0) {
        throw std::runtime_error("LZ4 decompression failed");
    }
    return out;
}

}

NSO::NSO(std::vector<uint8_t> data) : Il2Cpp(std::move(data)) {
    header.Magic = readU32();
    header.Version = readU32();
    header.Reserved = readU32();
    header.Flags = readU32();
    isTextCompressed = (header.Flags & 1) != 0;
    isRoDataCompressed = (header.Flags & 2) != 0;
    isDataCompressed = (header.Flags & 4) != 0;
    header.TextSegment.FileOffset = readU32();
    header.TextSegment.MemoryOffset = readU32();
    header.TextSegment.DecompressedSize = readU32();
    segments.push_back(header.TextSegment);
    header.ModuleOffset = readU32();
    header.RoDataSegment.FileOffset = readU32();
    header.RoDataSegment.MemoryOffset = readU32();
    header.RoDataSegment.DecompressedSize = readU32();
    segments.push_back(header.RoDataSegment);
    header.ModuleFileSize = readU32();
    header.DataSegment.FileOffset = readU32();
    header.DataSegment.MemoryOffset = readU32();
    header.DataSegment.DecompressedSize = readU32();
    segments.push_back(header.DataSegment);
    header.BssSize = readU32();
    header.DigestBuildID = readBytes(0x20);
    header.TextCompressedSize = readU32();
    header.RoDataCompressedSize = readU32();
    header.DataCompressedSize = readU32();
    header.Padding = readBytes(0x1C);
    header.APIInfo.RegionRoDataOffset = readU32();
    header.APIInfo.RegionSize = readU32();
    header.DynStr.RegionRoDataOffset = readU32();
    header.DynStr.RegionSize = readU32();
    header.DynSym.RegionRoDataOffset = readU32();
    header.DynSym.RegionSize = readU32();
    header.TextHash = readBytes(0x20);
    header.RoDataHash = readBytes(0x20);
    header.DataHash = readBytes(0x20);

    if (!isCompressed()) {
        setPosition(header.TextSegment.FileOffset + 4);
        uint32_t modOffset = readU32();
        setPosition(header.TextSegment.FileOffset + modOffset + 4);
        uint32_t dynamicOffset = readU32() + modOffset;
        uint32_t bssStart = readU32();
        uint32_t bssEnd = readU32();
        header.BssSegment.FileOffset = bssStart;
        header.BssSegment.MemoryOffset = bssStart;
        header.BssSegment.DecompressedSize = bssEnd - bssStart;
        uint32_t maxSize = (header.DataSegment.MemoryOffset + header.DataSegment.DecompressedSize - dynamicOffset) / 16;
        setPosition(mapVATR(dynamicOffset));
        for (uint32_t i = 0; i < maxSize; i++) {
            auto dynamic = readClass<Elf64_Dyn>();
            if (dynamic.d_tag == 0) {
                break;
            }
            dynamicSection.push_back(dynamic);
        }
        readSymbol();
        relocationProcessing();
    }
}

void NSO::readSymbol() {
    try {
        uint32_t symbolCount = 0;
        const Elf64_Dyn* hash = nullptr;
        for (const auto& dyn : dynamicSection) {
            if (dyn.d_tag == DT_HASH) {
                hash = &dyn;
                break;
            }
        }
        if (hash != nullptr) {
            uint64_t addr = mapVATR(hash->d_un);
            setPosition(addr);
            readU32();
            uint32_t nchain = readU32();
            symbolCount = nchain;
        } else {
            const Elf64_Dyn* gnuHash = nullptr;
            for (const auto& dyn : dynamicSection) {
                if (dyn.d_tag == DT_GNU_HASH) {
                    gnuHash = &dyn;
                    break;
                }
            }
            if (gnuHash == nullptr) {
                return;
            }
            uint64_t addr = mapVATR(gnuHash->d_un);
            setPosition(addr);
            uint32_t nbuckets = readU32();
            uint32_t symoffset = readU32();
            uint32_t bloom_size = readU32();
            /* uint32_t bloom_shift = */ readU32();
            uint64_t bucketsAddress = addr + 16 + (8ull * bloom_size);
            auto buckets = readClassArrayAt<uint32_t>(bucketsAddress, nbuckets);
            uint32_t last_symbol = 0;
            for (uint32_t b : buckets) {
                if (b > last_symbol) {
                    last_symbol = b;
                }
            }
            if (last_symbol < symoffset) {
                symbolCount = symoffset;
            } else {
                uint64_t chainsBaseAddress = bucketsAddress + 4ull * nbuckets;
                setPosition(chainsBaseAddress + (last_symbol - symoffset) * 4ull);
                while (true) {
                    uint32_t chain_entry = readU32();
                    ++last_symbol;
                    if ((chain_entry & 1) != 0) {
                        break;
                    }
                }
                symbolCount = last_symbol;
            }
        }
        uint64_t dynsymOffset = 0;
        for (const auto& dyn : dynamicSection) {
            if (dyn.d_tag == DT_SYMTAB) {
                dynsymOffset = mapVATR(dyn.d_un);
                break;
            }
        }
        symbolTable = readClassArrayAt<Elf64_Sym>(dynsymOffset, symbolCount);
    } catch (const std::exception&) {
    }
}

void NSO::relocationProcessing() {
    std::printf("Applying relocations...\n");
    try {
        uint64_t relaOffset = 0;
        uint64_t relaSize = 0;
        for (const auto& dyn : dynamicSection) {
            if (dyn.d_tag == DT_RELA) {
                relaOffset = mapVATR(dyn.d_un);
            } else if (dyn.d_tag == DT_RELASZ) {
                relaSize = dyn.d_un;
            }
        }
        auto relaTable = readClassArrayAt<Elf64_Rela>(relaOffset, relaSize / 24L);
        for (const auto& rela : relaTable) {
            uint64_t type = rela.r_info & 0xffffffff;
            uint64_t sym = rela.r_info >> 32;
            if (type == R_AARCH64_ABS64) {
                if (sym >= symbolTable.size()) {
                    continue;
                }
                const auto& symbol = symbolTable[static_cast<size_t>(sym)];
                setPosition(mapVATR(rela.r_offset));
                writeU64(symbol.st_value + rela.r_addend);
            } else if (type == R_AARCH64_RELATIVE) {
                setPosition(mapVATR(rela.r_offset));
                writeU64(rela.r_addend);
            }
        }
    } catch (const std::exception&) {
    }
}

uint64_t NSO::mapVATR(uint64_t addr) {
    for (const auto& segment : segments) {
        if (addr >= segment.MemoryOffset && addr <= static_cast<uint64_t>(segment.MemoryOffset) + segment.DecompressedSize) {
            return addr - segment.MemoryOffset + segment.FileOffset;
        }
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "mapVATR: address not in any segment (0x%llx)",
                  static_cast<unsigned long long>(addr));
    throw std::runtime_error(buf);
}

uint64_t NSO::mapRTVA(uint64_t addr) {
    for (const auto& segment : segments) {
        if (addr >= segment.FileOffset && addr <= static_cast<uint64_t>(segment.FileOffset) + segment.DecompressedSize) {
            return addr - segment.FileOffset + segment.MemoryOffset;
        }
    }
    return 0;
}

bool NSO::plusSearch(int methodCount, int typeDefinitionsCount, int imageCount) {
    auto sectionHelper = getSectionHelper(methodCount, typeDefinitionsCount, imageCount);
    uint64_t codeRegistration = sectionHelper.findCodeRegistration();
    uint64_t metadataRegistration = sectionHelper.findMetadataRegistration();
    return autoPlusInit(codeRegistration, metadataRegistration);
}

std::unique_ptr<NSO> NSO::unCompress() {
    if (!isCompressed()) {
        return nullptr;
    }
    std::vector<uint8_t> out;
    out.reserve(0x200 + header.TextSegment.DecompressedSize + header.RoDataSegment.DecompressedSize +
                header.DataSegment.DecompressedSize);

    auto appendU32 = [&out](uint32_t v) {
        out.push_back(static_cast<uint8_t>(v & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };
    appendU32(header.Magic);
    appendU32(header.Version);
    appendU32(header.Reserved);
    appendU32(0);
    appendU32(header.TextSegment.FileOffset);
    appendU32(header.TextSegment.MemoryOffset);
    appendU32(header.TextSegment.DecompressedSize);
    appendU32(header.ModuleOffset);
    uint32_t roOffset = header.TextSegment.FileOffset + header.TextSegment.DecompressedSize;
    appendU32(roOffset);
    appendU32(header.RoDataSegment.MemoryOffset);
    appendU32(header.RoDataSegment.DecompressedSize);
    appendU32(header.ModuleFileSize);
    appendU32(roOffset + header.RoDataSegment.DecompressedSize);
    appendU32(header.DataSegment.MemoryOffset);
    appendU32(header.DataSegment.DecompressedSize);
    appendU32(header.BssSize);
    out.insert(out.end(), header.DigestBuildID.begin(), header.DigestBuildID.end());
    appendU32(header.TextCompressedSize);
    appendU32(header.RoDataCompressedSize);
    appendU32(header.DataCompressedSize);
    out.insert(out.end(), header.Padding.begin(), header.Padding.end());
    appendU32(header.APIInfo.RegionRoDataOffset);
    appendU32(header.APIInfo.RegionSize);
    appendU32(header.DynStr.RegionRoDataOffset);
    appendU32(header.DynStr.RegionSize);
    appendU32(header.DynSym.RegionRoDataOffset);
    appendU32(header.DynSym.RegionSize);
    out.insert(out.end(), header.TextHash.begin(), header.TextHash.end());
    out.insert(out.end(), header.RoDataHash.begin(), header.RoDataHash.end());
    out.insert(out.end(), header.DataHash.begin(), header.DataHash.end());

    setPosition(header.TextSegment.FileOffset);
    auto textBytes = readBytes(header.TextCompressedSize);
    if (isTextCompressed) {
        auto data = lz4Decompress(textBytes, header.TextSegment.DecompressedSize);
        out.insert(out.end(), data.begin(), data.end());
    } else {
        out.insert(out.end(), textBytes.begin(), textBytes.end());
    }
    auto roDataBytes = readBytes(header.RoDataCompressedSize);
    if (isRoDataCompressed) {
        auto data = lz4Decompress(roDataBytes, header.RoDataSegment.DecompressedSize);
        out.insert(out.end(), data.begin(), data.end());
    } else {
        out.insert(out.end(), roDataBytes.begin(), roDataBytes.end());
    }
    auto dataBytes = readBytes(header.DataCompressedSize);
    if (isDataCompressed) {
        auto data = lz4Decompress(dataBytes, header.DataSegment.DecompressedSize);
        out.insert(out.end(), data.begin(), data.end());
    } else {
        out.insert(out.end(), dataBytes.begin(), dataBytes.end());
    }
    return std::make_unique<NSO>(std::move(out));
}

SectionHelper NSO::getSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) {
    SectionHelper sectionHelper(this, methodCount, typeDefinitionsCount, metadataUsagesCount, imageCount);
    sectionHelper.setSection(SearchSectionType::Exec, std::vector<NSOSegmentHeader>{header.TextSegment});
    sectionHelper.setSection(SearchSectionType::Data,
                             std::vector<NSOSegmentHeader>{header.DataSegment, header.RoDataSegment});
    sectionHelper.setSection(SearchSectionType::Bss, std::vector<NSOSegmentHeader>{header.BssSegment});
    return sectionHelper;
}

}
