#pragma once
#include "il2cpp_metadata.h"

// Forward
class SectionHelper;

// ============================================================
// Il2Cpp - abstract base for all binary format handlers
// ============================================================
class Il2Cpp : public BinaryStream {
public:
    std::vector<uint64_t> methodPointers;
    std::vector<uint64_t> genericMethodPointers;
    std::vector<uint64_t> invokerPointers;
    std::vector<uint64_t> customAttributeGenerators;
    std::vector<uint64_t> reversePInvokeWrappers;
    std::vector<uint64_t> unresolvedVirtualCallPointers;
    std::vector<Il2CppType> types;
    std::vector<uint64_t> metadataUsages;
    std::vector<uint64_t> genericInstPointers;
    std::vector<Il2CppGenericInst> genericInsts;
    std::vector<Il2CppMethodSpec> methodSpecs;
    std::map<int, std::vector<Il2CppMethodSpec>> methodDefinitionMethodSpecs;
    std::map<int, uint64_t> methodSpecGenericMethodPointers; // methodSpec index -> pointer
    // For version >= 24.2
    std::map<std::string, Il2CppCodeGenModule> codeGenModules;
    std::map<std::string, std::vector<uint64_t>> codeGenModuleMethodPointers;
    std::map<std::string, std::map<uint32_t, std::vector<Il2CppRGCTXDefinition>>> rgctxsDictionary;

    LogCallback log;

    Il2Cpp(const std::vector<uint8_t>& data, LogCallback logCb);
    virtual ~Il2Cpp() = default;

    void SetProperties(double version, int64_t metadataUsagesCount);

    virtual uint64_t MapVATR(uint64_t addr) = 0;
    virtual uint64_t MapRTVA(uint64_t addr) = 0;
    virtual bool Search() = 0;
    virtual bool PlusSearch(int methodCount, int typeDefinitionsCount, int imageCount) = 0;
    virtual bool SymbolSearch() = 0;
    virtual bool CheckDump() = 0;
    virtual uint64_t GetRVA(uint64_t pointer) { return pointer; }
    virtual std::unique_ptr<SectionHelper> GetSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) = 0;

    virtual void Init(uint64_t codeRegistration, uint64_t metadataRegistration);
    int GetFieldOffsetFromIndex(int typeIndex, int fieldIndexInType, int fieldIndex, bool isValueType, bool isStatic);
    Il2CppType* GetIl2CppType(uint64_t pointer);
    uint64_t GetMethodPointer(const std::string& imageName, const Il2CppMethodDefinition& methodDef);

    // MapVATR template helpers
    template<typename T, typename ReadFunc>
    T MapVATRRead(uint64_t addr, ReadFunc readFunc) {
        SetPosition(MapVATR(addr));
        return (this->*readFunc)();
    }

    template<typename T, typename ReadFunc>
    std::vector<T> MapVATRReadArray(uint64_t addr, int64_t count, ReadFunc readFunc) {
        SetPosition(MapVATR(addr));
        std::vector<T> result;
        result.reserve((size_t)count);
        for (int64_t i = 0; i < count; i++) {
            result.push_back((this->*readFunc)());
        }
        return result;
    }

    std::vector<uint64_t> MapVATRReadUIntPtrArray(uint64_t addr, int64_t count) {
        SetPosition(MapVATR(addr));
        std::vector<uint64_t> result;
        result.reserve((size_t)count);
        for (int64_t i = 0; i < count; i++) {
            result.push_back(ReadUIntPtr());
        }
        return result;
    }

protected:
    int64_t metadataUsagesCount_ = 0;
    std::vector<uint64_t> fieldOffsets_;
    bool fieldOffsetsArePointers_ = false;
    std::unordered_map<uint64_t, Il2CppType*> typeDic_;

    bool AutoPlusInit(uint64_t codeRegistration, uint64_t metadataRegistration);
};

// ============================================================
// SectionHelper
// ============================================================
class SectionHelper {
public:
    std::vector<SearchSection> exec;
    std::vector<SearchSection> data;
    std::vector<SearchSection> bss;

    SectionHelper(Il2Cpp* il2Cpp, int methodCount, int typeDefinitionsCount, int64_t metadataUsagesCount, int imageCount);

    void SetSection(SearchSectionType type, const std::vector<SearchSection>& secs);

    uint64_t FindCodeRegistration();
    uint64_t FindMetadataRegistration();

private:
    Il2Cpp* il2Cpp_;
    int methodCount_;
    int typeDefinitionsCount_;
    int64_t metadataUsagesCount_;
    int imageCount_;
    bool pointerInExec_ = false;

    uint64_t FindCodeRegistrationOld();
    uint64_t FindMetadataRegistrationOld();
    uint64_t FindMetadataRegistrationV21();
    uint64_t FindCodeRegistrationData();
    uint64_t FindCodeRegistrationExec();
    uint64_t FindCodeRegistration2019(const std::vector<SearchSection>& secs);
    std::vector<uint64_t> FindReference(uint64_t addr);

    bool CheckPointerRangeDataRa(uint64_t pointer) const;
    bool CheckPointerRangeExecVa(const std::vector<uint64_t>& pointers) const;
    bool CheckPointerRangeDataVa(const std::vector<uint64_t>& pointers) const;
    bool CheckPointerRangeBssVa(const std::vector<uint64_t>& pointers) const;
};

// Boyer-Moore-Horspool search
std::vector<int> SearchBytes(const uint8_t* source, size_t sourceLen, const uint8_t* pattern, size_t patternLen);

// ============================================================
// ARM instruction decoding
// ============================================================
namespace ArmUtils {
    uint32_t DecodeMov(const uint8_t* asm_bytes);
    uint64_t DecodeAdr(uint64_t pc, const uint8_t* inst);
    uint64_t DecodeAdrp(uint64_t pc, const uint8_t* inst);
    uint64_t DecodeAdd(const uint8_t* inst);
    bool IsAdr(const uint8_t* inst);
    std::string HexToBin(uint8_t b);
    std::string HexToBin(const uint8_t* bytes, size_t count);
}

// ============================================================
// Concrete format implementations
// ============================================================

class Macho64 : public Il2Cpp {
public:
    Macho64(const std::vector<uint8_t>& data, LogCallback log);
    uint64_t MapVATR(uint64_t addr) override;
    uint64_t MapRTVA(uint64_t addr) override;
    bool Search() override;
    bool PlusSearch(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool SymbolSearch() override;
    uint64_t GetRVA(uint64_t pointer) override;
    bool CheckDump() override;
    std::unique_ptr<SectionHelper> GetSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) override;
    uint64_t ReadUIntPtr() override;
private:
    std::vector<MachoSection64Bit> sections_;
    uint64_t vmaddr_ = 0;
};

class Macho : public Il2Cpp {
public:
    Macho(const std::vector<uint8_t>& data, LogCallback log);
    void Init(uint64_t codeRegistration, uint64_t metadataRegistration) override;
    uint64_t MapVATR(uint64_t addr) override;
    uint64_t MapRTVA(uint64_t addr) override;
    bool Search() override;
    bool PlusSearch(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool SymbolSearch() override;
    uint64_t GetRVA(uint64_t pointer) override;
    bool CheckDump() override;
    std::unique_ptr<SectionHelper> GetSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) override;
private:
    std::vector<MachoSection> sections_;
    uint64_t vmaddr_ = 0;
};

class ElfBase : public Il2Cpp {
public:
    ElfBase(const std::vector<uint8_t>& data, LogCallback log) : Il2Cpp(data, log) {}
    bool CheckDump() override;
    virtual void Reload() = 0;
protected:
    virtual void Load() = 0;
    virtual bool CheckSection() = 0;
};

class Elf32 : public ElfBase {
public:
    Elf32(const std::vector<uint8_t>& data, LogCallback log);
    uint64_t MapVATR(uint64_t addr) override;
    uint64_t MapRTVA(uint64_t addr) override;
    bool Search() override;
    bool PlusSearch(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool SymbolSearch() override;
    uint64_t GetRVA(uint64_t pointer) override;
    std::unique_ptr<SectionHelper> GetSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) override;
    void Reload() override;
protected:
    void Load() override;
    bool CheckSection() override;
private:
    Elf32_Ehdr elfHeader_;
    std::vector<Elf32_Phdr> programSegment_;
    std::vector<Elf32_Dyn> dynamicSection_;
    std::vector<Elf32_Sym> symbolTable_;
    std::vector<Elf32_Shdr> sectionTable_;
    Elf32_Phdr pt_dynamic_;
    void ReadSymbol();
    void RelocationProcessing();
    bool CheckProtection();
    void FixedProgramSegment();
    void FixedDynamicSection();
};

class Elf64Impl : public ElfBase {
public:
    Elf64Impl(const std::vector<uint8_t>& data, LogCallback log);
    uint64_t MapVATR(uint64_t addr) override;
    uint64_t MapRTVA(uint64_t addr) override;
    bool Search() override;
    bool PlusSearch(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool SymbolSearch() override;
    uint64_t GetRVA(uint64_t pointer) override;
    std::unique_ptr<SectionHelper> GetSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) override;
    void Reload() override;
protected:
    void Load() override;
    bool CheckSection() override;
private:
    Elf64_Ehdr elfHeader_;
    std::vector<Elf64_Phdr> programSegment_;
    std::vector<Elf64_Dyn> dynamicSection_;
    std::vector<Elf64_Sym> symbolTable_;
    std::vector<Elf64_Shdr> sectionTable_;
    Elf64_Phdr pt_dynamic_;
    void ReadSymbol();
    void RelocationProcessing();
    bool CheckProtection();
    void FixedProgramSegment();
    void FixedDynamicSection();
};

class PE : public Il2Cpp {
public:
    PE(const std::vector<uint8_t>& data, LogCallback log);
    uint64_t MapVATR(uint64_t addr) override;
    uint64_t MapRTVA(uint64_t addr) override;
    bool Search() override;
    bool PlusSearch(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool SymbolSearch() override;
    uint64_t GetRVA(uint64_t pointer) override;
    bool CheckDump() override;
    std::unique_ptr<SectionHelper> GetSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) override;
private:
    std::vector<PESectionHeader> sections_;
};

// ============================================================
// MachoFat helper
// ============================================================
struct MachoFatInfo {
    std::vector<Fat> fats;
    static MachoFatInfo Parse(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> GetMacho(const std::vector<uint8_t>& data, const Fat& fat);
};
