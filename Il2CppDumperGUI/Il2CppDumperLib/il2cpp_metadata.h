#pragma once
#include "il2cpp_binary_stream.h"

class Metadata : public BinaryStream {
public:
    Il2CppGlobalMetadataHeader header;
    std::vector<Il2CppImageDefinition> imageDefs;
    std::vector<Il2CppAssemblyDefinition> assemblyDefs;
    std::vector<Il2CppTypeDefinition> typeDefs;
    std::vector<Il2CppMethodDefinition> methodDefs;
    std::vector<Il2CppParameterDefinition> parameterDefs;
    std::vector<Il2CppFieldDefinition> fieldDefs;
    std::vector<Il2CppPropertyDefinition> propertyDefs;
    std::vector<Il2CppCustomAttributeTypeRange> attributeTypeRanges;
    std::vector<Il2CppCustomAttributeDataRange> attributeDataRanges;
    std::vector<Il2CppStringLiteral> stringLiterals;
    std::vector<int32_t> attributeTypes;
    std::vector<int32_t> interfaceIndices;
    std::vector<int32_t> nestedTypeIndices;
    std::vector<Il2CppEventDefinition> eventDefs;
    std::vector<Il2CppGenericContainer> genericContainers;
    std::vector<Il2CppFieldRef> fieldRefs;
    std::vector<Il2CppGenericParameter> genericParameters;
    std::vector<int32_t> constraintIndices;
    std::vector<uint32_t> vtableMethods;
    std::vector<Il2CppRGCTXDefinition> rgctxEntries;

    std::map<Il2CppMetadataUsage, std::map<uint32_t, uint32_t>> metadataUsageDic;
    int64_t metadataUsagesCount = 0;

    // Per-image attribute lookup
    std::map<int, std::map<uint32_t, int>> attributeTypeRangesDic; // imageIndex -> token -> index

    Metadata(const std::vector<uint8_t>& data, LogCallback log);

    bool GetFieldDefaultValueFromIndex(int index, Il2CppFieldDefaultValue& value) const;
    bool GetParameterDefaultValueFromIndex(int index, Il2CppParameterDefaultValue& value) const;
    uint32_t GetDefaultValueFromIndex(int index) const;
    std::string GetStringFromIndex(uint32_t index);
    int GetCustomAttributeIndex(int imageIndex, int customAttributeIndex, uint32_t token) const;
    std::string GetStringLiteralFromIndex(uint32_t index);

    static uint32_t GetEncodedIndexType(uint32_t index);
    uint32_t GetDecodedMethodIndex(uint32_t index) const;

    int SizeOfType(const std::string& typeName) const;
    int SizeOfImageDef() const;
    int SizeOfAssemblyDef() const;
    int SizeOfTypeDef() const;
    int SizeOfMethodDef() const;
    int SizeOfParameterDef() const;
    int SizeOfFieldDef() const;
    int SizeOfFieldDefaultValue() const;
    int SizeOfParameterDefaultValue() const;
    int SizeOfPropertyDef() const;
    int SizeOfCustomAttributeTypeRange() const;
    int SizeOfCustomAttributeDataRange() const;
    int SizeOfMetadataUsageList() const;
    int SizeOfMetadataUsagePair() const;
    int SizeOfStringLiteral() const;
    int SizeOfEventDef() const;
    int SizeOfGenericContainer() const;
    int SizeOfFieldRef() const;
    int SizeOfGenericParameter() const;
    int SizeOfRGCTXDefinition() const;

private:
    std::map<int, Il2CppFieldDefaultValue> fieldDefaultValuesDic;
    std::map<int, Il2CppParameterDefaultValue> parameterDefaultValuesDic;
    std::vector<Il2CppMetadataUsageList> metadataUsageLists;
    std::vector<Il2CppMetadataUsagePair> metadataUsagePairs;
    std::unordered_map<uint32_t, std::string> stringCache;
    LogCallback log_;

    void ProcessingMetadataUsage();

    template<typename T, typename ReadFunc>
    std::vector<T> ReadMetadataClassArray(uint32_t addr, int count, int elemSize, ReadFunc readFunc) {
        int n = count / elemSize;
        SetPosition(addr);
        std::vector<T> result;
        result.reserve(n);
        for (int i = 0; i < n; i++) {
            result.push_back((this->*readFunc)());
        }
        return result;
    }
};
