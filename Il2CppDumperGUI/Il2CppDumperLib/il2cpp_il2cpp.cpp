#include "il2cpp_il2cpp.h"
#include <algorithm>
#include <bitset>

// ============================================================
// Boyer-Moore-Horspool byte search
// ============================================================
std::vector<int> SearchBytes(const uint8_t* source, size_t sourceLen, const uint8_t* pattern, size_t patternLen) {
    std::vector<int> results;
    if (patternLen == 0 || patternLen > sourceLen) return results;
    int badChar[256];
    for (int i = 0; i < 256; i++) badChar[i] = (int)patternLen;
    for (size_t i = 0; i < patternLen - 1; i++) {
        badChar[pattern[i]] = (int)(patternLen - 1 - i);
    }
    size_t index = 0;
    while (index <= sourceLen - patternLen) {
        size_t i = patternLen - 1;
        while (source[index + i] == pattern[i]) {
            if (i == 0) { results.push_back((int)index); break; }
            i--;
        }
        index += badChar[source[index + patternLen - 1]];
    }
    return results;
}

// ============================================================
// ARM Utils
// ============================================================
namespace ArmUtils {

std::string HexToBin(uint8_t b) {
    std::string result(8, '0');
    for (int i = 7; i >= 0; i--) {
        result[7 - i] = ((b >> i) & 1) ? '1' : '0';
    }
    return result;
}

std::string HexToBin(const uint8_t* bytes, size_t count) {
    std::string result;
    // Reverse order like C# (insert at beginning)
    for (size_t i = 0; i < count; i++) {
        result = HexToBin(bytes[i]) + result;
    }
    return result;
}

uint32_t DecodeMov(const uint8_t* asm_bytes) {
    auto low = (uint16_t)(asm_bytes[2] + ((asm_bytes[3] & 0x70) << 4) + ((asm_bytes[1] & 0x04) << 9) + ((asm_bytes[0] & 0x0f) << 12));
    auto high = (uint16_t)(asm_bytes[6] + ((asm_bytes[7] & 0x70) << 4) + ((asm_bytes[5] & 0x04) << 9) + ((asm_bytes[4] & 0x0f) << 12));
    return (uint32_t)((high << 16) + low);
}

uint64_t DecodeAdr(uint64_t pc, const uint8_t* inst) {
    auto bin = HexToBin(inst, 4);
    // immhi = bits[8..26] (19 bits), immlo = bits[1..2] (2 bits)
    std::string uint64str = bin.substr(8, 19) + bin.substr(1, 2);
    // Sign extend
    char signBit = uint64str[0];
    while (uint64str.size() < 64) uint64str = signBit + uint64str;
    uint64_t offset = 0;
    for (auto c : uint64str) { offset = (offset << 1) | (c - '0'); }
    return pc + offset;
}

uint64_t DecodeAdrp(uint64_t pc, const uint8_t* inst) {
    pc &= 0xFFFFFFFFFFFFF000ULL;
    auto bin = HexToBin(inst, 4);
    std::string uint64str = bin.substr(8, 19) + bin.substr(1, 2) + std::string(12, '0');
    char signBit = uint64str[0];
    while (uint64str.size() < 64) uint64str = signBit + uint64str;
    uint64_t offset = 0;
    for (auto c : uint64str) { offset = (offset << 1) | (c - '0'); }
    return pc + offset;
}

uint64_t DecodeAdd(const uint8_t* inst) {
    auto bin = HexToBin(inst, 4);
    std::string imm12str = bin.substr(10, 12);
    uint64_t val = 0;
    for (auto c : imm12str) { val = (val << 1) | (c - '0'); }
    if (bin[9] == '1') val <<= 12;
    return val;
}

bool IsAdr(const uint8_t* inst) {
    auto bin = HexToBin(inst, 4);
    return bin[0] == '0' && bin.substr(3, 5) == "10000";
}

} // namespace ArmUtils

// ============================================================
// Il2Cpp base
// ============================================================
Il2Cpp::Il2Cpp(const std::vector<uint8_t>& data, LogCallback logCb)
    : BinaryStream(data), log(logCb) {}

void Il2Cpp::SetProperties(double version, int64_t metadataUsagesCount) {
    Version = version;
    metadataUsagesCount_ = metadataUsagesCount;
}

bool Il2Cpp::AutoPlusInit(uint64_t codeRegistration, uint64_t metadataRegistration) {
    if (codeRegistration != 0) {
        uint64_t limit = 0x50000;
        if (Version >= 24.2) {
            SetPosition(MapVATR(codeRegistration));
            auto pCodeReg = ReadCodeRegistration();
            if (Version == 31) {
                if (pCodeReg.genericMethodPointersCount > limit) {
                    codeRegistration -= PointerSize() * 2;
                } else {
                    Version = 29;
                    if (log) log("Change il2cpp version to: " + std::to_string(Version));
                }
            }
            if (Version == 29) {
                if (pCodeReg.genericMethodPointersCount > limit) {
                    Version = 29.1;
                    codeRegistration -= PointerSize() * 2;
                    if (log) log("Change il2cpp version to: " + std::to_string(Version));
                }
            }
            if (Version == 27) {
                if (pCodeReg.reversePInvokeWrapperCount > limit) {
                    Version = 27.1;
                    codeRegistration -= PointerSize();
                    if (log) log("Change il2cpp version to: " + std::to_string(Version));
                }
            }
            if (Version == 24.4) {
                codeRegistration -= PointerSize() * 2;
                SetPosition(MapVATR(codeRegistration));
                pCodeReg = ReadCodeRegistration();
                if (pCodeReg.reversePInvokeWrapperCount > limit) {
                    Version = 24.5;
                    codeRegistration -= PointerSize();
                    if (log) log("Change il2cpp version to: " + std::to_string(Version));
                }
            }
            if (Version == 24.2) {
                if (pCodeReg.interopDataCount == 0) {
                    Version = 24.3;
                    codeRegistration -= PointerSize() * 2;
                    if (log) log("Change il2cpp version to: " + std::to_string(Version));
                }
            }
        }
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "CodeRegistration : %llx", (unsigned long long)codeRegistration);
    if (log) log(buf);
    snprintf(buf, sizeof(buf), "MetadataRegistration : %llx", (unsigned long long)metadataRegistration);
    if (log) log(buf);
    if (codeRegistration != 0 && metadataRegistration != 0) {
        Init(codeRegistration, metadataRegistration);
        return true;
    }
    return false;
}

void Il2Cpp::Init(uint64_t codeRegistration, uint64_t metadataRegistration) {
    SetPosition(MapVATR(codeRegistration));
    auto pCodeRegistration = ReadCodeRegistration();
    uint64_t limit = 0x50000;

    if (Version == 27 && pCodeRegistration.invokerPointersCount > limit) {
        Version = 27.1;
        if (log) log("Change il2cpp version to: " + std::to_string(Version));
        SetPosition(MapVATR(codeRegistration));
        pCodeRegistration = ReadCodeRegistration();
    }

    if (Version == 27.1) {
        auto pCodeGenModulePtrs = MapVATRReadUIntPtrArray(pCodeRegistration.codeGenModules, pCodeRegistration.codeGenModulesCount);
        for (auto& ptr : pCodeGenModulePtrs) {
            SetPosition(MapVATR(ptr));
            auto cgm = ReadCodeGenModule();
            if (cgm.rgctxsCount > 0) {
                auto rgctxs = MapVATRReadArray<Il2CppRGCTXDefinition>(cgm.rgctxs, cgm.rgctxsCount, &BinaryStream::ReadRGCTXDefinition);
                bool allAboveLimit = true;
                for (auto& r : rgctxs) {
                    if (r.rgctxDataDummy <= (int64_t)limit) { allAboveLimit = false; break; }
                }
                if (allAboveLimit) {
                    Version = 27.2;
                    if (log) log("Change il2cpp version to: " + std::to_string(Version));
                }
                break;
            }
        }
    }

    if (Version == 24.4 && pCodeRegistration.invokerPointersCount > limit) {
        Version = 24.5;
        if (log) log("Change il2cpp version to: " + std::to_string(Version));
        SetPosition(MapVATR(codeRegistration));
        pCodeRegistration = ReadCodeRegistration();
    }
    if (Version == 24.2 && pCodeRegistration.codeGenModules == 0) {
        Version = 24.3;
        if (log) log("Change il2cpp version to: " + std::to_string(Version));
        SetPosition(MapVATR(codeRegistration));
        pCodeRegistration = ReadCodeRegistration();
    }

    SetPosition(MapVATR(metadataRegistration));
    auto pMetadataRegistration = ReadMetadataRegistration();

    genericMethodPointers = MapVATRReadUIntPtrArray(pCodeRegistration.genericMethodPointers, pCodeRegistration.genericMethodPointersCount);
    invokerPointers = MapVATRReadUIntPtrArray(pCodeRegistration.invokerPointers, pCodeRegistration.invokerPointersCount);

    if (Version < 27) {
        customAttributeGenerators = MapVATRReadUIntPtrArray(pCodeRegistration.customAttributeGenerators, pCodeRegistration.customAttributeCount);
    }
    if (Version > 16 && Version < 27) {
        metadataUsages = MapVATRReadUIntPtrArray(pMetadataRegistration.metadataUsages, metadataUsagesCount_);
    }
    if (Version >= 22) {
        if (pCodeRegistration.reversePInvokeWrapperCount != 0)
            reversePInvokeWrappers = MapVATRReadUIntPtrArray(pCodeRegistration.reversePInvokeWrappers, pCodeRegistration.reversePInvokeWrapperCount);
        if (pCodeRegistration.unresolvedVirtualCallCount != 0)
            unresolvedVirtualCallPointers = MapVATRReadUIntPtrArray(pCodeRegistration.unresolvedVirtualCallPointers, pCodeRegistration.unresolvedVirtualCallCount);
    }

    genericInstPointers = MapVATRReadUIntPtrArray(pMetadataRegistration.genericInsts, pMetadataRegistration.genericInstsCount);
    genericInsts.resize(genericInstPointers.size());
    for (size_t i = 0; i < genericInstPointers.size(); i++) {
        SetPosition(MapVATR(genericInstPointers[i]));
        genericInsts[i] = ReadGenericInst();
    }

    fieldOffsetsArePointers_ = Version > 21;
    if (Version == 21) {
        auto fieldTest = ReadUInt32Array(MapVATR(pMetadataRegistration.fieldOffsets), 6);
        fieldOffsetsArePointers_ = fieldTest[0] == 0 && fieldTest[1] == 0 && fieldTest[2] == 0 && fieldTest[3] == 0 && fieldTest[4] == 0 && fieldTest[5] > 0;
    }
    if (fieldOffsetsArePointers_) {
        fieldOffsets_ = MapVATRReadUIntPtrArray(pMetadataRegistration.fieldOffsets, pMetadataRegistration.fieldOffsetsCount);
    } else {
        auto raw = ReadUInt32Array(MapVATR(pMetadataRegistration.fieldOffsets), pMetadataRegistration.fieldOffsetsCount);
        fieldOffsets_.resize(raw.size());
        for (size_t i = 0; i < raw.size(); i++) fieldOffsets_[i] = raw[i];
    }

    auto pTypes = MapVATRReadUIntPtrArray(pMetadataRegistration.types, pMetadataRegistration.typesCount);
    types.resize(pMetadataRegistration.typesCount);
    for (int64_t i = 0; i < pMetadataRegistration.typesCount; i++) {
        SetPosition(MapVATR(pTypes[i]));
        types[i] = ReadIl2CppType();
        types[i].Init(Version);
        typeDic_[pTypes[i]] = &types[i];
    }

    if (Version >= 24.2) {
        auto pCodeGenModulePtrs = MapVATRReadUIntPtrArray(pCodeRegistration.codeGenModules, pCodeRegistration.codeGenModulesCount);
        for (auto& ptr : pCodeGenModulePtrs) {
            SetPosition(MapVATR(ptr));
            auto codeGenModule = ReadCodeGenModule();
            auto moduleName = ReadStringToNull(MapVATR(codeGenModule.moduleName));
            codeGenModules[moduleName] = codeGenModule;

            std::vector<uint64_t> mptrs;
            try {
                mptrs = MapVATRReadUIntPtrArray(codeGenModule.methodPointers, codeGenModule.methodPointerCount);
            } catch (...) {
                mptrs.resize(codeGenModule.methodPointerCount, 0);
            }
            codeGenModuleMethodPointers[moduleName] = mptrs;

            std::map<uint32_t, std::vector<Il2CppRGCTXDefinition>> rgctxsDefDictionary;
            if (codeGenModule.rgctxsCount > 0) {
                auto rgctxs = MapVATRReadArray<Il2CppRGCTXDefinition>(codeGenModule.rgctxs, codeGenModule.rgctxsCount, &BinaryStream::ReadRGCTXDefinition);
                auto rgctxRanges = MapVATRReadArray<Il2CppTokenRangePair>(codeGenModule.rgctxRanges, codeGenModule.rgctxRangesCount, &BinaryStream::ReadTokenRangePair);
                for (auto& range : rgctxRanges) {
                    std::vector<Il2CppRGCTXDefinition> defs(rgctxs.begin() + range.range.start, rgctxs.begin() + range.range.start + range.range.length);
                    rgctxsDefDictionary[range.token] = defs;
                }
            }
            rgctxsDictionary[moduleName] = rgctxsDefDictionary;
        }
    } else {
        methodPointers = MapVATRReadUIntPtrArray(pCodeRegistration.methodPointers, pCodeRegistration.methodPointersCount);
    }

    auto gmTable = MapVATRReadArray<Il2CppGenericMethodFunctionsDefinitions>(
        pMetadataRegistration.genericMethodTable, pMetadataRegistration.genericMethodTableCount,
        &BinaryStream::ReadGenericMethodFunctionsDefinitions);
    methodSpecs = MapVATRReadArray<Il2CppMethodSpec>(
        pMetadataRegistration.methodSpecs, pMetadataRegistration.methodSpecsCount,
        &BinaryStream::ReadMethodSpec);

    for (auto& table : gmTable) {
        auto& methodSpec = methodSpecs[table.genericMethodIndex];
        auto methodDefinitionIndex = methodSpec.methodDefinitionIndex;
        methodDefinitionMethodSpecs[methodDefinitionIndex].push_back(methodSpec);
        methodSpecGenericMethodPointers[table.genericMethodIndex] = genericMethodPointers[table.indices.methodIndex];
    }
}

int Il2Cpp::GetFieldOffsetFromIndex(int typeIndex, int fieldIndexInType, int fieldIndex, bool isValueType, bool isStatic) {
    try {
        int offset = -1;
        if (fieldOffsetsArePointers_) {
            auto ptr = fieldOffsets_[typeIndex];
            if (ptr > 0) {
                SetPosition(MapVATR(ptr) + 4ULL * fieldIndexInType);
                offset = ReadInt32();
            }
        } else {
            offset = (int)fieldOffsets_[fieldIndex];
        }
        if (offset > 0 && isValueType && !isStatic) {
            offset -= Is32Bit ? 8 : 16;
        }
        return offset;
    } catch (...) {
        return -1;
    }
}

Il2CppType* Il2Cpp::GetIl2CppType(uint64_t pointer) {
    auto it = typeDic_.find(pointer);
    if (it != typeDic_.end()) return it->second;
    return nullptr;
}

uint64_t Il2Cpp::GetMethodPointer(const std::string& imageName, const Il2CppMethodDefinition& methodDef) {
    if (Version >= 24.2) {
        auto it = codeGenModuleMethodPointers.find(imageName);
        if (it != codeGenModuleMethodPointers.end()) {
            auto methodPointerIndex = methodDef.token & 0x00FFFFFFu;
            if (methodPointerIndex > 0 && methodPointerIndex <= it->second.size()) {
                return it->second[methodPointerIndex - 1];
            }
        }
        return 0;
    } else {
        if (methodDef.methodIndex >= 0 && methodDef.methodIndex < (int)methodPointers.size()) {
            return methodPointers[methodDef.methodIndex];
        }
    }
    return 0;
}

// ============================================================
// SectionHelper
// ============================================================
SectionHelper::SectionHelper(Il2Cpp* il2Cpp, int methodCount, int typeDefinitionsCount, int64_t metadataUsagesCount, int imageCount)
    : il2Cpp_(il2Cpp), methodCount_(methodCount), typeDefinitionsCount_(typeDefinitionsCount),
      metadataUsagesCount_(metadataUsagesCount), imageCount_(imageCount) {}

void SectionHelper::SetSection(SearchSectionType type, const std::vector<SearchSection>& secs) {
    switch (type) {
        case SearchSectionType::Exec: exec = secs; break;
        case SearchSectionType::Data: data = secs; break;
        case SearchSectionType::Bss: bss = secs; break;
    }
}

uint64_t SectionHelper::FindCodeRegistration() {
    if (il2Cpp_->Version >= 24.2) {
        uint64_t codeReg;
        auto isElf = dynamic_cast<ElfBase*>(il2Cpp_) != nullptr;
        if (isElf) {
            codeReg = FindCodeRegistrationExec();
            if (codeReg == 0) codeReg = FindCodeRegistrationData();
            else pointerInExec_ = true;
        } else {
            codeReg = FindCodeRegistrationData();
            if (codeReg == 0) { codeReg = FindCodeRegistrationExec(); pointerInExec_ = true; }
        }
        return codeReg;
    }
    return FindCodeRegistrationOld();
}

uint64_t SectionHelper::FindMetadataRegistration() {
    if (il2Cpp_->Version < 19) return 0;
    if (il2Cpp_->Version >= 27) return FindMetadataRegistrationV21();
    return FindMetadataRegistrationOld();
}

uint64_t SectionHelper::FindCodeRegistrationOld() {
    for (auto& section : data) {
        il2Cpp_->SetPosition(section.offset);
        while (il2Cpp_->GetPosition() < section.offsetEnd) {
            auto addr = il2Cpp_->GetPosition();
            if (il2Cpp_->ReadIntPtr() == methodCount_) {
                try {
                    auto pointer = il2Cpp_->MapVATR(il2Cpp_->ReadUIntPtr());
                    if (CheckPointerRangeDataRa(pointer)) {
                        auto pointers = il2Cpp_->ReadUInt64Array(pointer, methodCount_);
                        if (CheckPointerRangeExecVa(pointers)) {
                            return addr - section.offset + section.address;
                        }
                    }
                } catch (...) {}
            }
            il2Cpp_->SetPosition(addr + il2Cpp_->PointerSize());
        }
    }
    return 0;
}

uint64_t SectionHelper::FindMetadataRegistrationOld() {
    for (auto& section : data) {
        il2Cpp_->SetPosition(section.offset);
        auto end = std::min(section.offsetEnd, il2Cpp_->GetLength()) - il2Cpp_->PointerSize();
        while (il2Cpp_->GetPosition() < end) {
            auto addr = il2Cpp_->GetPosition();
            if (il2Cpp_->ReadIntPtr() == typeDefinitionsCount_) {
                try {
                    il2Cpp_->SetPosition(il2Cpp_->GetPosition() + il2Cpp_->PointerSize() * 2);
                    auto pointer = il2Cpp_->MapVATR(il2Cpp_->ReadUIntPtr());
                    if (CheckPointerRangeDataRa(pointer)) {
                        auto pointers = il2Cpp_->ReadUInt64Array(pointer, metadataUsagesCount_);
                        if (CheckPointerRangeBssVa(pointers)) {
                            return addr - il2Cpp_->PointerSize() * 12 - section.offset + section.address;
                        }
                    }
                } catch (...) {}
            }
            il2Cpp_->SetPosition(addr + il2Cpp_->PointerSize());
        }
    }
    return 0;
}

uint64_t SectionHelper::FindMetadataRegistrationV21() {
    for (auto& section : data) {
        il2Cpp_->SetPosition(section.offset);
        auto end = std::min(section.offsetEnd, il2Cpp_->GetLength()) - il2Cpp_->PointerSize();
        while (il2Cpp_->GetPosition() < end) {
            auto addr = il2Cpp_->GetPosition();
            if (il2Cpp_->ReadIntPtr() == typeDefinitionsCount_) {
                il2Cpp_->SetPosition(il2Cpp_->GetPosition() + il2Cpp_->PointerSize());
                if (il2Cpp_->ReadIntPtr() == typeDefinitionsCount_) {
                    try {
                        auto pointer = il2Cpp_->MapVATR(il2Cpp_->ReadUIntPtr());
                        if (CheckPointerRangeDataRa(pointer)) {
                            auto pointers = il2Cpp_->ReadUInt64Array(pointer, typeDefinitionsCount_);
                            bool flag = pointerInExec_ ? CheckPointerRangeExecVa(pointers) : CheckPointerRangeDataVa(pointers);
                            if (flag) {
                                return addr - il2Cpp_->PointerSize() * 10 - section.offset + section.address;
                            }
                        }
                    } catch (...) {}
                }
            }
            il2Cpp_->SetPosition(addr + il2Cpp_->PointerSize());
        }
    }
    return 0;
}

static const uint8_t featureBytes[] = { 0x6D, 0x73, 0x63, 0x6F, 0x72, 0x6C, 0x69, 0x62, 0x2E, 0x64, 0x6C, 0x6C, 0x00 }; // mscorlib.dll

uint64_t SectionHelper::FindCodeRegistrationData() { return FindCodeRegistration2019(data); }
uint64_t SectionHelper::FindCodeRegistrationExec() { return FindCodeRegistration2019(exec); }

uint64_t SectionHelper::FindCodeRegistration2019(const std::vector<SearchSection>& secs) {
    for (auto& sec : secs) {
        il2Cpp_->SetPosition(sec.offset);
        auto buff = il2Cpp_->ReadBytes((int)(sec.offsetEnd - sec.offset));
        auto indices = SearchBytes(buff.data(), buff.size(), featureBytes, sizeof(featureBytes));
        for (auto index : indices) {
            auto dllva = (uint64_t)index + sec.address;
            for (auto refva : FindReference(dllva)) {
                for (auto refva2 : FindReference(refva)) {
                    if (il2Cpp_->Version >= 27) {
                        for (int i = imageCount_ - 1; i >= 0; i--) {
                            for (auto refva3 : FindReference(refva2 - (uint64_t)i * il2Cpp_->PointerSize())) {
                                il2Cpp_->SetPosition(il2Cpp_->MapVATR(refva3 - il2Cpp_->PointerSize()));
                                if (il2Cpp_->ReadIntPtr() == imageCount_) {
                                    if (il2Cpp_->Version >= 29) return refva3 - il2Cpp_->PointerSize() * 14;
                                    return refva3 - il2Cpp_->PointerSize() * 13;
                                }
                            }
                        }
                    } else {
                        for (int i = 0; i < imageCount_; i++) {
                            for (auto refva3 : FindReference(refva2 - (uint64_t)i * il2Cpp_->PointerSize())) {
                                return refva3 - il2Cpp_->PointerSize() * 13;
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}

std::vector<uint64_t> SectionHelper::FindReference(uint64_t addr) {
    std::vector<uint64_t> results;
    for (auto& dataSec : data) {
        auto position = dataSec.offset;
        auto end = std::min(dataSec.offsetEnd, il2Cpp_->GetLength()) - il2Cpp_->PointerSize();
        while (position < end) {
            il2Cpp_->SetPosition(position);
            if (il2Cpp_->ReadUIntPtr() == addr) {
                results.push_back(position - dataSec.offset + dataSec.address);
            }
            position += il2Cpp_->PointerSize();
        }
    }
    return results;
}

bool SectionHelper::CheckPointerRangeDataRa(uint64_t pointer) const {
    for (auto& x : data) { if (pointer >= x.offset && pointer <= x.offsetEnd) return true; }
    return false;
}

bool SectionHelper::CheckPointerRangeExecVa(const std::vector<uint64_t>& pointers) const {
    for (auto p : pointers) {
        bool found = false;
        for (auto& y : exec) { if (p >= y.address && p <= y.addressEnd) { found = true; break; } }
        if (!found) return false;
    }
    return true;
}

bool SectionHelper::CheckPointerRangeDataVa(const std::vector<uint64_t>& pointers) const {
    for (auto p : pointers) {
        bool found = false;
        for (auto& y : data) { if (p >= y.address && p <= y.addressEnd) { found = true; break; } }
        if (!found) return false;
    }
    return true;
}

bool SectionHelper::CheckPointerRangeBssVa(const std::vector<uint64_t>& pointers) const {
    for (auto p : pointers) {
        bool found = false;
        for (auto& y : bss) { if (p >= y.address && p <= y.addressEnd) { found = true; break; } }
        if (!found) return false;
    }
    return true;
}

// ============================================================
// Macho64
// ============================================================
Macho64::Macho64(const std::vector<uint8_t>& data, LogCallback logCb) : Il2Cpp(data, logCb) {
    SetPosition(GetPosition() + 16); // skip magic, cputype, cpusubtype, filetype
    auto ncmds = ReadUInt32();
    SetPosition(GetPosition() + 12); // skip sizeofcmds, flags, reserved
    for (uint32_t i = 0; i < ncmds; i++) {
        auto pos = GetPosition();
        auto cmd = ReadUInt32();
        auto cmdsize = ReadUInt32();
        if (cmd == 0x19) { // LC_SEGMENT_64
            auto nameBytes = ReadBytes(16);
            std::string segname(nameBytes.begin(), nameBytes.end());
            segname = segname.c_str(); // trim nulls
            if (segname == "__TEXT") {
                vmaddr_ = ReadUInt64();
            } else {
                SetPosition(GetPosition() + 8);
            }
            SetPosition(GetPosition() + 32); // skip vmsize..initprot
            auto nsects = ReadUInt32();
            SetPosition(GetPosition() + 4); // skip flags
            for (uint32_t j = 0; j < nsects; j++) {
                MachoSection64Bit section;
                auto snBytes = ReadBytes(16);
                section.sectname = std::string(snBytes.begin(), snBytes.end());
                section.sectname = section.sectname.c_str();
                SetPosition(GetPosition() + 16); // skip segname
                section.addr = ReadUInt64();
                section.size = ReadUInt64();
                section.offset = ReadUInt32();
                SetPosition(GetPosition() + 12); // skip align, reloff, nreloc
                section.flags = ReadUInt32();
                SetPosition(GetPosition() + 12); // skip reserved1,2,3
                sections_.push_back(section);
            }
        } else if (cmd == 0x2C) { // LC_ENCRYPTION_INFO_64
            SetPosition(GetPosition() + 8);
            auto cryptID = ReadUInt32();
            if (cryptID != 0 && log) {
                log("ERROR: This Mach-O executable is encrypted and cannot be processed.");
            }
        }
        SetPosition(pos + cmdsize);
    }
}

uint64_t Macho64::MapVATR(uint64_t addr) {
    for (auto& s : sections_) {
        if (addr >= s.addr && addr <= s.addr + s.size) {
            if (s.sectname == "__bss") throw std::runtime_error("bss section");
            return addr - s.addr + s.offset;
        }
    }
    throw std::runtime_error("MapVATR: address not found");
}

uint64_t Macho64::MapRTVA(uint64_t addr) {
    for (auto& s : sections_) {
        if (addr >= s.offset && addr <= s.offset + s.size) {
            if (s.sectname == "__bss") throw std::runtime_error("bss section");
            return addr - s.offset + s.addr;
        }
    }
    return 0;
}

bool Macho64::Search() {
    static const uint8_t feat1[] = { 0x2, 0x0, 0x80, 0xD2 }; // MOV X2, #0
    static const uint8_t feat2[] = { 0x3, 0x0, 0x80, 0x52 }; // MOV W3, #0

    uint64_t codeReg = 0, metaReg = 0;

    auto it = std::find_if(sections_.begin(), sections_.end(), [](const MachoSection64Bit& s){ return s.sectname == "__mod_init_func"; });
    if (it == sections_.end()) return false;
    auto& modInitFunc = *it;

    auto addrs = ReadUInt64Array(modInitFunc.offset, modInitFunc.size / 8);

    if (Version >= 24) {
        for (auto i : addrs) {
            if (i > 0) {
                SetPosition(MapVATR(i) + 16);
                auto buff = ReadBytes(4);
                if (memcmp(buff.data(), feat2, 4) == 0) {
                    buff = ReadBytes(4);
                    if (memcmp(buff.data(), feat1, 4) == 0) {
                        SetPosition(GetPosition() - 16);
                        auto inst = ReadBytes(4);
                        auto subaddr = ArmUtils::DecodeAdr(i + 8, inst.data());
                        auto rsubaddr = MapVATR(subaddr);
                        SetPosition(rsubaddr);
                        auto inst2 = ReadBytes(4);
                        codeReg = ArmUtils::DecodeAdrp(subaddr, inst2.data());
                        inst2 = ReadBytes(4);
                        codeReg += ArmUtils::DecodeAdd(inst2.data());
                        SetPosition(rsubaddr + 8);
                        inst2 = ReadBytes(4);
                        metaReg = ArmUtils::DecodeAdrp(subaddr + 8, inst2.data());
                        inst2 = ReadBytes(4);
                        metaReg += ArmUtils::DecodeAdd(inst2.data());
                    }
                }
            }
        }
    } else if (Version == 23) {
        for (auto i : addrs) {
            if (i > 0) {
                SetPosition(MapVATR(i) + 16);
                auto buff = ReadBytes(4);
                if (memcmp(buff.data(), feat1, 4) == 0) {
                    buff = ReadBytes(4);
                    if (memcmp(buff.data(), feat2, 4) == 0) {
                        SetPosition(GetPosition() - 16);
                        auto inst = ReadBytes(4);
                        auto subaddr = ArmUtils::DecodeAdr(i + 8, inst.data());
                        auto rsubaddr = MapVATR(subaddr);
                        SetPosition(rsubaddr);
                        auto inst2 = ReadBytes(4);
                        codeReg = ArmUtils::DecodeAdrp(subaddr, inst2.data());
                        inst2 = ReadBytes(4);
                        codeReg += ArmUtils::DecodeAdd(inst2.data());
                        SetPosition(rsubaddr + 8);
                        inst2 = ReadBytes(4);
                        metaReg = ArmUtils::DecodeAdrp(subaddr + 8, inst2.data());
                        inst2 = ReadBytes(4);
                        metaReg += ArmUtils::DecodeAdd(inst2.data());
                    }
                }
            }
        }
    } else { // Version < 23
        for (auto i : addrs) {
            if (i > 0) {
                SetPosition(MapVATR(i));
                auto buff = ReadBytes(4);
                if (memcmp(buff.data(), feat1, 4) == 0) {
                    buff = ReadBytes(4);
                    if (memcmp(buff.data(), feat2, 4) == 0) {
                        SetPosition(GetPosition() + 8);
                        auto inst = ReadBytes(4);
                        if (ArmUtils::IsAdr(inst.data())) {
                            auto subaddr = ArmUtils::DecodeAdr(i + 16, inst.data());
                            auto rsubaddr = MapVATR(subaddr);
                            SetPosition(rsubaddr);
                            auto inst2 = ReadBytes(4);
                            codeReg = ArmUtils::DecodeAdrp(subaddr, inst2.data());
                            inst2 = ReadBytes(4);
                            codeReg += ArmUtils::DecodeAdd(inst2.data());
                            SetPosition(rsubaddr + 8);
                            inst2 = ReadBytes(4);
                            metaReg = ArmUtils::DecodeAdrp(subaddr + 8, inst2.data());
                            inst2 = ReadBytes(4);
                            metaReg += ArmUtils::DecodeAdd(inst2.data());
                        }
                    }
                }
            }
        }
    }

    if (codeReg != 0 && metaReg != 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "CodeRegistration : %llx", (unsigned long long)codeReg);
        if (log) log(buf);
        snprintf(buf, sizeof(buf), "MetadataRegistration : %llx", (unsigned long long)metaReg);
        if (log) log(buf);
        Il2Cpp::Init(codeReg, metaReg);
        return true;
    }
    return false;
}

bool Macho64::PlusSearch(int methodCount, int typeDefinitionsCount, int imageCount) {
    auto sh = GetSectionHelper(methodCount, typeDefinitionsCount, imageCount);
    auto codeReg = sh->FindCodeRegistration();
    auto metaReg = sh->FindMetadataRegistration();
    return AutoPlusInit(codeReg, metaReg);
}

bool Macho64::SymbolSearch() { return false; }
uint64_t Macho64::GetRVA(uint64_t pointer) { return pointer - vmaddr_; }
bool Macho64::CheckDump() { return false; }

std::unique_ptr<SectionHelper> Macho64::GetSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) {
    auto sh = std::make_unique<SectionHelper>(this, methodCount, typeDefinitionsCount, metadataUsagesCount_, imageCount);
    std::vector<SearchSection> dataSecs, codeSecs, bssSecs;
    for (auto& s : sections_) {
        SearchSection ss { s.offset, s.offset + s.size, s.addr, s.addr + s.size };
        if (s.sectname == "__const" || s.sectname == "__cstring" || s.sectname == "__data") dataSecs.push_back(ss);
        if (s.flags == 0x80000400) codeSecs.push_back(ss);
        if (s.flags == 1u) bssSecs.push_back(ss);
    }
    sh->SetSection(SearchSectionType::Exec, codeSecs);
    sh->SetSection(SearchSectionType::Data, dataSecs);
    sh->SetSection(SearchSectionType::Bss, bssSecs);
    return sh;
}

uint64_t Macho64::ReadUIntPtr() {
    auto pointer = ReadUInt64();
    if (pointer > vmaddr_ + 0xFFFFFFFF) {
        auto addr = GetPosition();
        for (auto& s : sections_) {
            if (addr >= s.offset && addr <= s.offset + s.size) {
                if (s.sectname == "__const" || s.sectname == "__data") {
                    auto rva = pointer - vmaddr_;
                    rva &= 0xFFFFFFFF;
                    pointer = rva + vmaddr_;
                }
                break;
            }
        }
    }
    return pointer;
}

// ============================================================
// Macho (32-bit) - simplified
// ============================================================
Macho::Macho(const std::vector<uint8_t>& data, LogCallback logCb) : Il2Cpp(data, logCb) {
    Is32Bit = true;
    SetPosition(GetPosition() + 16);
    auto ncmds = ReadUInt32();
    SetPosition(GetPosition() + 8);
    for (uint32_t i = 0; i < ncmds; i++) {
        auto pos = GetPosition();
        auto cmd = ReadUInt32();
        auto cmdsize = ReadUInt32();
        if (cmd == 1) { // LC_SEGMENT
            auto nameBytes = ReadBytes(16);
            std::string segname(nameBytes.begin(), nameBytes.end());
            segname = segname.c_str();
            if (segname == "__TEXT") { vmaddr_ = ReadUInt32(); }
            else { SetPosition(GetPosition() + 4); }
            SetPosition(GetPosition() + 20);
            auto nsects = ReadUInt32();
            SetPosition(GetPosition() + 4);
            for (uint32_t j = 0; j < nsects; j++) {
                MachoSection section;
                auto snBytes = ReadBytes(16);
                section.sectname = std::string(snBytes.begin(), snBytes.end());
                section.sectname = section.sectname.c_str();
                SetPosition(GetPosition() + 16);
                section.addr = ReadUInt32();
                section.size = ReadUInt32();
                section.offset = ReadUInt32();
                SetPosition(GetPosition() + 12);
                section.flags = ReadUInt32();
                SetPosition(GetPosition() + 8);
                sections_.push_back(section);
            }
        }
        SetPosition(pos + cmdsize);
    }
}

void Macho::Init(uint64_t codeRegistration, uint64_t metadataRegistration) {
    Il2Cpp::Init(codeRegistration, metadataRegistration);
    for (auto& p : methodPointers) if (p > 0) p -= 1;
    for (auto& p : customAttributeGenerators) if (p > 0) p -= 1;
}

uint64_t Macho::MapVATR(uint64_t addr) {
    for (auto& s : sections_) {
        if (addr >= s.addr && addr <= s.addr + s.size) return addr - s.addr + s.offset;
    }
    throw std::runtime_error("MapVATR: address not found");
}

uint64_t Macho::MapRTVA(uint64_t addr) {
    for (auto& s : sections_) {
        if (addr >= s.offset && addr <= s.offset + s.size) return addr - s.offset + s.addr;
    }
    return 0;
}

bool Macho::Search() { return false; } // ARM32 search skipped for brevity
bool Macho::PlusSearch(int methodCount, int typeDefinitionsCount, int imageCount) {
    auto sh = GetSectionHelper(methodCount, typeDefinitionsCount, imageCount);
    return AutoPlusInit(sh->FindCodeRegistration(), sh->FindMetadataRegistration());
}
bool Macho::SymbolSearch() { return false; }
uint64_t Macho::GetRVA(uint64_t pointer) { return pointer - vmaddr_; }
bool Macho::CheckDump() { return false; }

std::unique_ptr<SectionHelper> Macho::GetSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) {
    auto sh = std::make_unique<SectionHelper>(this, methodCount, typeDefinitionsCount, metadataUsagesCount_, imageCount);
    std::vector<SearchSection> dataSecs, codeSecs, bssSecs;
    for (auto& s : sections_) {
        SearchSection ss { s.offset, s.offset + s.size, s.addr, s.addr + s.size };
        if (s.sectname == "__const") dataSecs.push_back(ss);
        if (s.flags == 0x80000400) codeSecs.push_back(ss);
        if (s.flags == 1u) bssSecs.push_back(ss);
    }
    sh->SetSection(SearchSectionType::Exec, codeSecs);
    sh->SetSection(SearchSectionType::Data, dataSecs);
    sh->SetSection(SearchSectionType::Bss, bssSecs);
    return sh;
}

// ============================================================
// ElfBase
// ============================================================
bool ElfBase::CheckDump() { return !CheckSection(); }

// ============================================================
// Elf32
// ============================================================
Elf32::Elf32(const std::vector<uint8_t>& data, LogCallback logCb) : ElfBase(data, logCb) {
    Is32Bit = true;
    Load();
}

void Elf32::Load() {
    SetPosition(0);
    elfHeader_ = ReadElf32Ehdr();
    SetPosition(elfHeader_.e_phoff);
    programSegment_.clear();
    for (int i = 0; i < elfHeader_.e_phnum; i++) programSegment_.push_back(ReadElf32Phdr());
    if (IsDumped) FixedProgramSegment();
    for (auto& p : programSegment_) { if (p.p_type == ElfConstants::PT_DYNAMIC) { pt_dynamic_ = p; break; } }
    SetPosition(pt_dynamic_.p_offset);
    dynamicSection_.clear();
    for (uint32_t i = 0; i < pt_dynamic_.p_filesz / 8; i++) dynamicSection_.push_back(ReadElf32Dyn());
    if (IsDumped) FixedDynamicSection();
    ReadSymbol();
    if (!IsDumped) RelocationProcessing();
}

void Elf32::Reload() { Load(); }
bool Elf32::CheckSection() { return true; } // simplified

uint64_t Elf32::MapVATR(uint64_t addr) {
    for (auto& p : programSegment_) {
        if (addr >= p.p_vaddr && addr <= p.p_vaddr + p.p_memsz)
            return addr - p.p_vaddr + p.p_offset;
    }
    throw std::runtime_error("Elf32::MapVATR failed");
}

uint64_t Elf32::MapRTVA(uint64_t addr) {
    for (auto& p : programSegment_) {
        if (addr >= p.p_offset && addr <= p.p_offset + p.p_filesz)
            return addr - p.p_offset + p.p_vaddr;
    }
    return 0;
}

bool Elf32::Search() { return false; }
bool Elf32::PlusSearch(int mc, int tdc, int ic) {
    auto sh = GetSectionHelper(mc, tdc, ic);
    return AutoPlusInit(sh->FindCodeRegistration(), sh->FindMetadataRegistration());
}

bool Elf32::SymbolSearch() {
    uint32_t codeReg = 0, metaReg = 0;
    auto dynstrIt = std::find_if(dynamicSection_.begin(), dynamicSection_.end(), [](const Elf32_Dyn& d) { return d.d_tag == ElfConstants::DT_STRTAB; });
    if (dynstrIt == dynamicSection_.end()) return false;
    auto dynstrOffset = MapVATR(dynstrIt->d_un);
    for (auto& sym : symbolTable_) {
        auto name = ReadStringToNull(dynstrOffset + sym.st_name);
        if (name == "g_CodeRegistration") codeReg = sym.st_value;
        else if (name == "g_MetadataRegistration") metaReg = sym.st_value;
    }
    if (codeReg > 0 && metaReg > 0) { Il2Cpp::Init(codeReg, metaReg); return true; }
    return false;
}

uint64_t Elf32::GetRVA(uint64_t pointer) { return IsDumped ? pointer - ImageBase : pointer; }

std::unique_ptr<SectionHelper> Elf32::GetSectionHelper(int mc, int tdc, int ic) {
    auto sh = std::make_unique<SectionHelper>(this, mc, tdc, metadataUsagesCount_, ic);
    std::vector<SearchSection> dataList, execList;
    for (auto& p : programSegment_) {
        if (p.p_memsz == 0) continue;
        SearchSection ss { p.p_offset, p.p_offset + p.p_filesz, p.p_vaddr, p.p_vaddr + p.p_memsz };
        switch (p.p_flags) {
            case 1: case 3: case 5: case 7: execList.push_back(ss); break;
            case 2: case 4: case 6: dataList.push_back(ss); break;
        }
    }
    sh->SetSection(SearchSectionType::Exec, execList);
    sh->SetSection(SearchSectionType::Data, dataList);
    sh->SetSection(SearchSectionType::Bss, dataList);
    return sh;
}

void Elf32::ReadSymbol() {
    try {
        uint32_t symbolCount = 0;
        auto hashIt = std::find_if(dynamicSection_.begin(), dynamicSection_.end(), [](const Elf32_Dyn& d){ return d.d_tag == ElfConstants::DT_HASH; });
        if (hashIt != dynamicSection_.end()) {
            SetPosition(MapVATR(hashIt->d_un));
            ReadUInt32(); symbolCount = ReadUInt32();
        } else {
            hashIt = std::find_if(dynamicSection_.begin(), dynamicSection_.end(), [](const Elf32_Dyn& d){ return d.d_tag == ElfConstants::DT_GNU_HASH; });
            if (hashIt == dynamicSection_.end()) return;
            auto addr = MapVATR(hashIt->d_un);
            SetPosition(addr);
            auto nbuckets = ReadUInt32(); auto symoffset = ReadUInt32();
            auto bloom_size = ReadUInt32(); ReadUInt32();
            auto buckets_addr = addr + 16 + (4 * bloom_size);
            auto buckets = ReadUInt32Array(buckets_addr, nbuckets);
            auto last = *std::max_element(buckets.begin(), buckets.end());
            if (last < symoffset) { symbolCount = symoffset; }
            else {
                SetPosition(buckets_addr + 4 * nbuckets + (last - symoffset) * 4);
                while (true) { auto e = ReadUInt32(); ++last; if ((e & 1) != 0) break; }
                symbolCount = last;
            }
        }
        auto dynsymIt = std::find_if(dynamicSection_.begin(), dynamicSection_.end(), [](const Elf32_Dyn& d){ return d.d_tag == ElfConstants::DT_SYMTAB; });
        if (dynsymIt == dynamicSection_.end()) return;
        SetPosition(MapVATR(dynsymIt->d_un));
        symbolTable_.clear();
        for (uint32_t i = 0; i < symbolCount; i++) symbolTable_.push_back(ReadElf32Sym());
    } catch (...) {}
}

void Elf32::RelocationProcessing() {
    try {
        auto relIt = std::find_if(dynamicSection_.begin(), dynamicSection_.end(), [](const Elf32_Dyn& d){ return d.d_tag == ElfConstants::DT_REL; });
        if (relIt == dynamicSection_.end()) return;
        auto relszIt = std::find_if(dynamicSection_.begin(), dynamicSection_.end(), [](const Elf32_Dyn& d){ return d.d_tag == ElfConstants::DT_RELSZ; });
        if (relszIt == dynamicSection_.end()) return;
        SetPosition(MapVATR(relIt->d_un));
        std::vector<Elf32_Rel> relTable;
        for (uint32_t i = 0; i < relszIt->d_un / 8; i++) relTable.push_back(ReadElf32Rel());
        bool isx86 = elfHeader_.e_machine == 0x3;
        for (auto& rel : relTable) {
            auto type = rel.r_info & 0xff;
            auto sym = rel.r_info >> 8;
            if ((isx86 && type == ElfConstants::R_386_32) || (!isx86 && type == ElfConstants::R_ARM_ABS32)) {
                if (sym < symbolTable_.size()) {
                    SetPosition(MapVATR(rel.r_offset));
                    WriteUInt32(symbolTable_[sym].st_value);
                }
            }
        }
    } catch (...) {}
}

void Elf32::FixedProgramSegment() {
    for (uint32_t i = 0; i < programSegment_.size(); i++) {
        auto& p = programSegment_[i];
        SetPosition(elfHeader_.e_phoff + i * 32u + 4u);
        p.p_offset = p.p_vaddr;
        WriteUInt32(p.p_offset);
        p.p_vaddr += (uint32_t)ImageBase;
        WriteUInt32(p.p_vaddr);
        SetPosition(GetPosition() + 4);
        p.p_filesz = p.p_memsz;
        WriteUInt32(p.p_filesz);
    }
}

void Elf32::FixedDynamicSection() {
    for (uint32_t i = 0; i < dynamicSection_.size(); i++) {
        auto& d = dynamicSection_[i];
        SetPosition(pt_dynamic_.p_offset + i * 8 + 4);
        switch (d.d_tag) {
            case ElfConstants::DT_PLTGOT: case ElfConstants::DT_HASH: case ElfConstants::DT_STRTAB:
            case ElfConstants::DT_SYMTAB: case ElfConstants::DT_RELA: case ElfConstants::DT_INIT:
            case ElfConstants::DT_FINI: case ElfConstants::DT_REL: case ElfConstants::DT_JMPREL:
            case ElfConstants::DT_INIT_ARRAY: case ElfConstants::DT_FINI_ARRAY:
                d.d_un += (uint32_t)ImageBase;
                WriteUInt32(d.d_un);
                break;
        }
    }
}

// ============================================================
// Elf64Impl
// ============================================================
Elf64Impl::Elf64Impl(const std::vector<uint8_t>& data, LogCallback logCb) : ElfBase(data, logCb) {
    Load();
}

void Elf64Impl::Load() {
    SetPosition(0);
    elfHeader_ = ReadElf64Ehdr();
    SetPosition(elfHeader_.e_phoff);
    programSegment_.clear();
    for (int i = 0; i < elfHeader_.e_phnum; i++) programSegment_.push_back(ReadElf64Phdr());
    if (IsDumped) FixedProgramSegment();
    for (auto& p : programSegment_) { if (p.p_type == ElfConstants::PT_DYNAMIC) { pt_dynamic_ = p; break; } }
    SetPosition(pt_dynamic_.p_offset);
    dynamicSection_.clear();
    for (int64_t i = 0; i < (int64_t)(pt_dynamic_.p_filesz / 16); i++) dynamicSection_.push_back(ReadElf64Dyn());
    if (IsDumped) FixedDynamicSection();
    ReadSymbol();
    if (!IsDumped) RelocationProcessing();
}

void Elf64Impl::Reload() { Load(); }
bool Elf64Impl::CheckSection() { return true; }

uint64_t Elf64Impl::MapVATR(uint64_t addr) {
    for (auto& p : programSegment_) {
        if (addr >= p.p_vaddr && addr <= p.p_vaddr + p.p_memsz) return addr - p.p_vaddr + p.p_offset;
    }
    throw std::runtime_error("Elf64::MapVATR failed");
}

uint64_t Elf64Impl::MapRTVA(uint64_t addr) {
    for (auto& p : programSegment_) {
        if (addr >= p.p_offset && addr <= p.p_offset + p.p_filesz) return addr - p.p_offset + p.p_vaddr;
    }
    return 0;
}

bool Elf64Impl::Search() { return false; }
bool Elf64Impl::PlusSearch(int mc, int tdc, int ic) {
    auto sh = GetSectionHelper(mc, tdc, ic);
    return AutoPlusInit(sh->FindCodeRegistration(), sh->FindMetadataRegistration());
}

bool Elf64Impl::SymbolSearch() {
    uint64_t codeReg = 0, metaReg = 0;
    auto dynstrIt = std::find_if(dynamicSection_.begin(), dynamicSection_.end(), [](const Elf64_Dyn& d) { return d.d_tag == ElfConstants::DT_STRTAB; });
    if (dynstrIt == dynamicSection_.end()) return false;
    auto dynstrOffset = MapVATR(dynstrIt->d_un);
    for (auto& sym : symbolTable_) {
        auto name = ReadStringToNull(dynstrOffset + sym.st_name);
        if (name == "g_CodeRegistration") codeReg = sym.st_value;
        else if (name == "g_MetadataRegistration") metaReg = sym.st_value;
    }
    if (codeReg > 0 && metaReg > 0) { Il2Cpp::Init(codeReg, metaReg); return true; }
    return false;
}

uint64_t Elf64Impl::GetRVA(uint64_t pointer) { return IsDumped ? pointer - ImageBase : pointer; }

std::unique_ptr<SectionHelper> Elf64Impl::GetSectionHelper(int mc, int tdc, int ic) {
    auto sh = std::make_unique<SectionHelper>(this, mc, tdc, metadataUsagesCount_, ic);
    std::vector<SearchSection> dataList, execList;
    for (auto& p : programSegment_) {
        if (p.p_memsz == 0) continue;
        SearchSection ss { p.p_offset, p.p_offset + p.p_filesz, p.p_vaddr, p.p_vaddr + p.p_memsz };
        switch (p.p_flags) {
            case 1: case 3: case 5: case 7: execList.push_back(ss); break;
            case 2: case 4: case 6: dataList.push_back(ss); break;
        }
    }
    sh->SetSection(SearchSectionType::Exec, execList);
    sh->SetSection(SearchSectionType::Data, dataList);
    sh->SetSection(SearchSectionType::Bss, dataList);
    return sh;
}

void Elf64Impl::ReadSymbol() {
    try {
        uint32_t symbolCount = 0;
        auto hashIt = std::find_if(dynamicSection_.begin(), dynamicSection_.end(), [](const Elf64_Dyn& d){ return d.d_tag == ElfConstants::DT_HASH; });
        if (hashIt != dynamicSection_.end()) {
            SetPosition(MapVATR(hashIt->d_un));
            ReadUInt32(); symbolCount = ReadUInt32();
        } else {
            hashIt = std::find_if(dynamicSection_.begin(), dynamicSection_.end(), [](const Elf64_Dyn& d){ return d.d_tag == ElfConstants::DT_GNU_HASH; });
            if (hashIt == dynamicSection_.end()) return;
            auto addr = MapVATR(hashIt->d_un);
            SetPosition(addr);
            auto nbuckets = ReadUInt32(); auto symoffset = ReadUInt32();
            auto bloom_size = ReadUInt32(); ReadUInt32();
            auto buckets_addr = addr + 16 + (8 * bloom_size);
            auto buckets = ReadUInt32Array(buckets_addr, nbuckets);
            auto last = *std::max_element(buckets.begin(), buckets.end());
            if (last < symoffset) { symbolCount = symoffset; }
            else {
                SetPosition(buckets_addr + 4 * nbuckets + (last - symoffset) * 4);
                while (true) { auto e = ReadUInt32(); ++last; if ((e & 1) != 0) break; }
                symbolCount = last;
            }
        }
        auto dynsymIt = std::find_if(dynamicSection_.begin(), dynamicSection_.end(), [](const Elf64_Dyn& d){ return d.d_tag == ElfConstants::DT_SYMTAB; });
        if (dynsymIt == dynamicSection_.end()) return;
        SetPosition(MapVATR(dynsymIt->d_un));
        symbolTable_.clear();
        for (uint32_t i = 0; i < symbolCount; i++) symbolTable_.push_back(ReadElf64Sym());
    } catch (...) {}
}

void Elf64Impl::RelocationProcessing() {
    try {
        auto relaIt = std::find_if(dynamicSection_.begin(), dynamicSection_.end(), [](const Elf64_Dyn& d){ return d.d_tag == ElfConstants::DT_RELA; });
        if (relaIt == dynamicSection_.end()) return;
        auto relaszIt = std::find_if(dynamicSection_.begin(), dynamicSection_.end(), [](const Elf64_Dyn& d){ return d.d_tag == ElfConstants::DT_RELASZ; });
        if (relaszIt == dynamicSection_.end()) return;
        SetPosition(MapVATR(relaIt->d_un));
        std::vector<Elf64_Rela> relaTable;
        for (int64_t i = 0; i < (int64_t)(relaszIt->d_un / 24); i++) relaTable.push_back(ReadElf64Rela());
        for (auto& rela : relaTable) {
            auto type = rela.r_info & 0xffffffff;
            auto sym = rela.r_info >> 32;
            uint64_t value = 0; bool recognized = false;
            if (elfHeader_.e_machine == ElfConstants::EM_AARCH64) {
                if (type == ElfConstants::R_AARCH64_ABS64) { value = symbolTable_[sym].st_value + rela.r_addend; recognized = true; }
                else if (type == ElfConstants::R_AARCH64_RELATIVE) { value = rela.r_addend; recognized = true; }
            } else if (elfHeader_.e_machine == ElfConstants::EM_X86_64) {
                if (type == ElfConstants::R_X86_64_64) { value = symbolTable_[sym].st_value + rela.r_addend; recognized = true; }
                else if (type == ElfConstants::R_X86_64_RELATIVE) { value = rela.r_addend; recognized = true; }
            }
            if (recognized) { SetPosition(MapVATR(rela.r_offset)); WriteUInt64(value); }
        }
    } catch (...) {}
}

bool Elf64Impl::CheckProtection() { return false; }

void Elf64Impl::FixedProgramSegment() {
    for (uint32_t i = 0; i < programSegment_.size(); i++) {
        auto& p = programSegment_[i];
        SetPosition(elfHeader_.e_phoff + i * 56u + 8u);
        p.p_offset = p.p_vaddr;
        WriteUInt64(p.p_offset);
        p.p_vaddr += ImageBase;
        WriteUInt64(p.p_vaddr);
        SetPosition(GetPosition() + 8);
        p.p_filesz = p.p_memsz;
        WriteUInt64(p.p_filesz);
    }
}

void Elf64Impl::FixedDynamicSection() {
    for (uint32_t i = 0; i < dynamicSection_.size(); i++) {
        auto& d = dynamicSection_[i];
        SetPosition(pt_dynamic_.p_offset + i * 16 + 8);
        switch (d.d_tag) {
            case ElfConstants::DT_PLTGOT: case ElfConstants::DT_HASH: case ElfConstants::DT_STRTAB:
            case ElfConstants::DT_SYMTAB: case ElfConstants::DT_RELA: case ElfConstants::DT_INIT:
            case ElfConstants::DT_FINI: case ElfConstants::DT_REL: case ElfConstants::DT_JMPREL:
            case ElfConstants::DT_INIT_ARRAY: case ElfConstants::DT_FINI_ARRAY:
                d.d_un += ImageBase;
                WriteUInt64(d.d_un);
                break;
        }
    }
}

// ============================================================
// PE
// ============================================================
PE::PE(const std::vector<uint8_t>& data, LogCallback logCb) : Il2Cpp(data, logCb) {
    auto dosHeader = ReadDosHeader();
    if (dosHeader.Magic != 0x5A4D) throw std::runtime_error("Invalid PE file");
    SetPosition(dosHeader.Lfanew);
    if (ReadUInt32() != 0x4550u) throw std::runtime_error("Invalid PE file");
    auto fileHeader = ReadPEFileHeader();
    auto pos = GetPosition();
    auto magic = ReadUInt16();
    SetPosition(GetPosition() - 2);
    if (magic == 0x10b) {
        Is32Bit = true;
        SetPosition(GetPosition() + 2+1+1+4+4+4+4+4+4); // skip to ImageBase (32-bit)
        ImageBase = ReadUInt32();
    } else if (magic == 0x20b) {
        SetPosition(GetPosition() + 2+1+1+4+4+4+4+4); // skip to ImageBase (64-bit, no BaseOfData)
        ImageBase = ReadUInt64();
    }
    SetPosition(pos + fileHeader.SizeOfOptionalHeader);
    for (int i = 0; i < fileHeader.NumberOfSections; i++) {
        sections_.push_back(ReadPESectionHeader());
    }
}

uint64_t PE::MapVATR(uint64_t absAddr) {
    auto addr = absAddr - ImageBase;
    for (auto& s : sections_) {
        if (addr >= s.VirtualAddress && addr <= s.VirtualAddress + s.VirtualSize)
            return addr - s.VirtualAddress + s.PointerToRawData;
    }
    return 0;
}

uint64_t PE::MapRTVA(uint64_t addr) {
    for (auto& s : sections_) {
        if (addr >= s.PointerToRawData && addr <= s.PointerToRawData + s.SizeOfRawData)
            return addr - s.PointerToRawData + s.VirtualAddress + ImageBase;
    }
    return 0;
}

bool PE::Search() { return false; }
bool PE::PlusSearch(int mc, int tdc, int ic) {
    auto sh = GetSectionHelper(mc, tdc, ic);
    return AutoPlusInit(sh->FindCodeRegistration(), sh->FindMetadataRegistration());
}
bool PE::SymbolSearch() { return false; }
uint64_t PE::GetRVA(uint64_t pointer) { return pointer - ImageBase; }
bool PE::CheckDump() { return Is32Bit ? ImageBase != 0x10000000 : ImageBase != 0x180000000; }

std::unique_ptr<SectionHelper> PE::GetSectionHelper(int mc, int tdc, int ic) {
    auto sh = std::make_unique<SectionHelper>(this, mc, tdc, metadataUsagesCount_, ic);
    std::vector<SearchSection> execList, dataList;
    for (auto& s : sections_) {
        SearchSection ss { s.PointerToRawData, s.PointerToRawData + s.SizeOfRawData, s.VirtualAddress + ImageBase, s.VirtualAddress + s.VirtualSize + ImageBase };
        if (s.Characteristics == 0x60000020) execList.push_back(ss);
        else if (s.Characteristics == 0x40000040 || s.Characteristics == 0xC0000040) dataList.push_back(ss);
    }
    sh->SetSection(SearchSectionType::Exec, execList);
    sh->SetSection(SearchSectionType::Data, dataList);
    sh->SetSection(SearchSectionType::Bss, dataList);
    return sh;
}

// ============================================================
// MachoFat
// ============================================================
MachoFatInfo MachoFatInfo::Parse(const std::vector<uint8_t>& data) {
    MachoFatInfo info;
    BinaryStream bs(data);
    bs.SetPosition(4);
    auto sizeBytes = bs.ReadBytes(4);
    int32_t size = ((int32_t)sizeBytes[0] << 24) | ((int32_t)sizeBytes[1] << 16) | ((int32_t)sizeBytes[2] << 8) | sizeBytes[3];
    for (int i = 0; i < size; i++) {
        bs.SetPosition(bs.GetPosition() + 8);
        auto offBytes = bs.ReadBytes(4);
        auto szBytes = bs.ReadBytes(4);
        Fat fat;
        fat.offset = ((uint32_t)offBytes[0] << 24) | ((uint32_t)offBytes[1] << 16) | ((uint32_t)offBytes[2] << 8) | offBytes[3];
        fat.size = ((uint32_t)szBytes[0] << 24) | ((uint32_t)szBytes[1] << 16) | ((uint32_t)szBytes[2] << 8) | szBytes[3];
        bs.SetPosition(bs.GetPosition() + 4);
        info.fats.push_back(fat);
    }
    for (auto& fat : info.fats) {
        bs.SetPosition(fat.offset);
        fat.magic = bs.ReadUInt32();
    }
    return info;
}

std::vector<uint8_t> MachoFatInfo::GetMacho(const std::vector<uint8_t>& data, const Fat& fat) {
    return std::vector<uint8_t>(data.begin() + fat.offset, data.begin() + fat.offset + fat.size);
}
