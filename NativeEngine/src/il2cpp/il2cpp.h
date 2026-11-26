#pragma once

#include "io/binary_stream.h"
#include "il2cpp/il2cpp_classes.h"
#include "il2cpp/metadata.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace il2cppdumper {

class SectionHelper;

class Il2Cpp : public BinaryStream {
public:
    explicit Il2Cpp(std::vector<uint8_t> data) : BinaryStream(std::move(data)) {}

    std::vector<uint64_t> methodPointers;
    std::vector<uint64_t> genericMethodPointers;
    std::vector<uint64_t> invokerPointers;
    std::vector<uint64_t> customAttributeGenerators;
    std::vector<uint64_t> reversePInvokeWrappers;
    std::vector<uint64_t> unresolvedVirtualCallPointers;
    std::vector<uint64_t> fieldOffsets;
    std::vector<Il2CppType> types;
    std::unordered_map<uint64_t, Il2CppType> typeDic;
    std::vector<uint64_t> metadataUsages;
    std::vector<Il2CppGenericMethodFunctionsDefinitions> genericMethodTable;
    std::vector<uint64_t> genericInstPointers;
    std::vector<Il2CppGenericInst> genericInsts;
    std::vector<Il2CppMethodSpec> methodSpecs;
    std::unordered_map<int, std::vector<Il2CppMethodSpec>> methodDefinitionMethodSpecs;
    std::unordered_map<Il2CppMethodSpec, uint64_t, MethodSpecHash> methodSpecGenericMethodPointers;
    bool fieldOffsetsArePointers = false;
    int64_t metadataUsagesCount = 0;
    std::unordered_map<std::string, Il2CppCodeGenModule> codeGenModules;
    std::unordered_map<std::string, std::vector<uint64_t>> codeGenModuleMethodPointers;
    std::unordered_map<std::string, std::unordered_map<uint32_t, std::vector<Il2CppRGCTXDefinition>>> rgctxsDictionary;
    bool isDumped = false;

    virtual uint64_t mapVATR(uint64_t addr) = 0;
    virtual uint64_t mapRTVA(uint64_t addr) = 0;
    virtual bool search() = 0;
    virtual bool plusSearch(int methodCount, int typeDefinitionsCount, int imageCount) = 0;
    virtual bool symbolSearch() = 0;
    virtual SectionHelper getSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) = 0;
    virtual bool checkDump() = 0;

    void setProperties(double v, int64_t count);

    bool autoPlusInit(uint64_t codeRegistration, uint64_t metadataRegistration);
    virtual void init(uint64_t codeRegistration, uint64_t metadataRegistration);
    virtual uint64_t getRVA(uint64_t pointer) { return pointer; }

    template <typename T>
    T mapVATRClass(uint64_t addr) {
        return readClassAt<T>(mapVATR(addr));
    }

    template <typename T>
    std::vector<T> mapVATRArray(uint64_t addr, int64_t count) {
        return readClassArrayAt<T>(mapVATR(addr), count);
    }

    int getFieldOffsetFromIndex(int typeIndex, int fieldIndexInType, int fieldIndex, bool isValueType, bool isStatic);

    Il2CppType* getIl2CppType(uint64_t pointer) {
        auto it = typeDic.find(pointer);
        if (it == typeDic.end()) {
            return nullptr;
        }
        return &it->second;
    }

    uint64_t getMethodPointer(const std::string& imageName, const Il2CppMethodDefinition& methodDef);

    bool isWasmMemory() const;

protected:
    Il2CppCodeRegistration pCodeRegistration;
    Il2CppMetadataRegistration pMetadataRegistration;
};

}
