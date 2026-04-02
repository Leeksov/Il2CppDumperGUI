#include "il2cpp_metadata.h"

Metadata::Metadata(const std::vector<uint8_t>& data, LogCallback log)
    : BinaryStream(data), log_(log)
{
    auto sanity = ReadUInt32();
    if (sanity != 0xFAB11BAF) {
        throw std::runtime_error("ERROR: Metadata file supplied is not valid metadata file.");
    }
    auto version = ReadInt32();
    if (version < 0 || version > 1000) {
        throw std::runtime_error("ERROR: Metadata file supplied is not valid metadata file.");
    }
    if (version < 16 || version > 31) {
        throw std::runtime_error("ERROR: Metadata file supplied is not a supported version[" + std::to_string(version) + "].");
    }
    Version = version;
    SetPosition(0);
    header = ReadGlobalMetadataHeader();

    if (version == 24) {
        if (header.stringLiteralOffset == 264) {
            Version = 24.2;
            SetPosition(0);
            header = ReadGlobalMetadataHeader();
        } else {
            imageDefs = ReadMetadataClassArray<Il2CppImageDefinition>(
                header.imagesOffset, header.imagesSize, SizeOfImageDef(),
                &BinaryStream::ReadImageDefinition);
            for (auto& img : imageDefs) {
                if (img.token != 1) {
                    Version = 24.1;
                    break;
                }
            }
        }
    }

    imageDefs = ReadMetadataClassArray<Il2CppImageDefinition>(
        header.imagesOffset, header.imagesSize, SizeOfImageDef(),
        &BinaryStream::ReadImageDefinition);

    if (Version == 24.2 && header.assembliesSize / 68 < (int)imageDefs.size()) {
        Version = 24.4;
    }
    bool v241Plus = false;
    if (Version == 24.1 && header.assembliesSize / 64 == (int)imageDefs.size()) {
        v241Plus = true;
    }
    if (v241Plus) {
        Version = 24.4;
    }

    assemblyDefs = ReadMetadataClassArray<Il2CppAssemblyDefinition>(
        header.assembliesOffset, header.assembliesSize, SizeOfAssemblyDef(),
        &BinaryStream::ReadAssemblyDefinition);

    if (v241Plus) {
        Version = 24.1;
    }

    typeDefs = ReadMetadataClassArray<Il2CppTypeDefinition>(
        header.typeDefinitionsOffset, header.typeDefinitionsSize, SizeOfTypeDef(),
        &BinaryStream::ReadTypeDefinition);

    methodDefs = ReadMetadataClassArray<Il2CppMethodDefinition>(
        header.methodsOffset, header.methodsSize, SizeOfMethodDef(),
        &BinaryStream::ReadMethodDefinition);

    parameterDefs = ReadMetadataClassArray<Il2CppParameterDefinition>(
        header.parametersOffset, header.parametersSize, SizeOfParameterDef(),
        &BinaryStream::ReadParameterDefinition);

    fieldDefs = ReadMetadataClassArray<Il2CppFieldDefinition>(
        header.fieldsOffset, header.fieldsSize, SizeOfFieldDef(),
        &BinaryStream::ReadFieldDefinition);

    {
        auto fdv = ReadMetadataClassArray<Il2CppFieldDefaultValue>(
            header.fieldDefaultValuesOffset, header.fieldDefaultValuesSize, SizeOfFieldDefaultValue(),
            &BinaryStream::ReadFieldDefaultValue);
        for (auto& v : fdv) fieldDefaultValuesDic[v.fieldIndex] = v;
    }
    {
        auto pdv = ReadMetadataClassArray<Il2CppParameterDefaultValue>(
            header.parameterDefaultValuesOffset, header.parameterDefaultValuesSize, SizeOfParameterDefaultValue(),
            &BinaryStream::ReadParameterDefaultValue);
        for (auto& v : pdv) parameterDefaultValuesDic[v.parameterIndex] = v;
    }

    propertyDefs = ReadMetadataClassArray<Il2CppPropertyDefinition>(
        header.propertiesOffset, header.propertiesSize, SizeOfPropertyDef(),
        &BinaryStream::ReadPropertyDefinition);

    interfaceIndices = ReadInt32Array(header.interfacesOffset, header.interfacesSize / 4);
    nestedTypeIndices = ReadInt32Array(header.nestedTypesOffset, header.nestedTypesSize / 4);

    eventDefs = ReadMetadataClassArray<Il2CppEventDefinition>(
        header.eventsOffset, header.eventsSize, SizeOfEventDef(),
        &BinaryStream::ReadEventDefinition);

    genericContainers = ReadMetadataClassArray<Il2CppGenericContainer>(
        header.genericContainersOffset, header.genericContainersSize, SizeOfGenericContainer(),
        &BinaryStream::ReadGenericContainer);

    genericParameters = ReadMetadataClassArray<Il2CppGenericParameter>(
        header.genericParametersOffset, header.genericParametersSize, SizeOfGenericParameter(),
        &BinaryStream::ReadGenericParameter);

    constraintIndices = ReadInt32Array(header.genericParameterConstraintsOffset, header.genericParameterConstraintsSize / 4);
    vtableMethods = ReadUInt32Array(header.vtableMethodsOffset, header.vtableMethodsSize / 4);

    stringLiterals = ReadMetadataClassArray<Il2CppStringLiteral>(
        header.stringLiteralOffset, header.stringLiteralSize, SizeOfStringLiteral(),
        &BinaryStream::ReadStringLiteral);

    if (Version > 16) {
        fieldRefs = ReadMetadataClassArray<Il2CppFieldRef>(
            header.fieldRefsOffset, header.fieldRefsSize, SizeOfFieldRef(),
            &BinaryStream::ReadFieldRef);
        if (Version < 27) {
            metadataUsageLists = ReadMetadataClassArray<Il2CppMetadataUsageList>(
                header.metadataUsageListsOffset, header.metadataUsageListsCount, SizeOfMetadataUsageList(),
                &BinaryStream::ReadMetadataUsageList);
            metadataUsagePairs = ReadMetadataClassArray<Il2CppMetadataUsagePair>(
                header.metadataUsagePairsOffset, header.metadataUsagePairsCount, SizeOfMetadataUsagePair(),
                &BinaryStream::ReadMetadataUsagePair);
            ProcessingMetadataUsage();
        }
    }

    if (Version > 20 && Version < 29) {
        attributeTypeRanges = ReadMetadataClassArray<Il2CppCustomAttributeTypeRange>(
            header.attributesInfoOffset, header.attributesInfoCount, SizeOfCustomAttributeTypeRange(),
            &BinaryStream::ReadCustomAttributeTypeRange);
        attributeTypes = ReadInt32Array(header.attributeTypesOffset, header.attributeTypesCount / 4);
    }

    if (Version >= 29) {
        attributeDataRanges = ReadMetadataClassArray<Il2CppCustomAttributeDataRange>(
            header.attributeDataRangeOffset, header.attributeDataRangeSize, SizeOfCustomAttributeDataRange(),
            &BinaryStream::ReadCustomAttributeDataRange);
    }

    if (Version > 24) {
        for (int idx = 0; idx < (int)imageDefs.size(); idx++) {
            auto& imageDef = imageDefs[idx];
            std::map<uint32_t, int> dic;
            auto end = imageDef.customAttributeStart + (int)imageDef.customAttributeCount;
            for (int i = imageDef.customAttributeStart; i < end; i++) {
                if (Version >= 29) {
                    dic[attributeDataRanges[i].token] = i;
                } else {
                    dic[attributeTypeRanges[i].token] = i;
                }
            }
            attributeTypeRangesDic[idx] = dic;
        }
    }

    if (Version <= 24.1) {
        rgctxEntries = ReadMetadataClassArray<Il2CppRGCTXDefinition>(
            header.rgctxEntriesOffset, header.rgctxEntriesCount, SizeOfRGCTXDefinition(),
            &BinaryStream::ReadRGCTXDefinition);
    }
}

bool Metadata::GetFieldDefaultValueFromIndex(int index, Il2CppFieldDefaultValue& value) const {
    auto it = fieldDefaultValuesDic.find(index);
    if (it != fieldDefaultValuesDic.end()) {
        value = it->second;
        return true;
    }
    return false;
}

bool Metadata::GetParameterDefaultValueFromIndex(int index, Il2CppParameterDefaultValue& value) const {
    auto it = parameterDefaultValuesDic.find(index);
    if (it != parameterDefaultValuesDic.end()) {
        value = it->second;
        return true;
    }
    return false;
}

uint32_t Metadata::GetDefaultValueFromIndex(int index) const {
    return (uint32_t)(header.fieldAndParameterDefaultValueDataOffset + index);
}

std::string Metadata::GetStringFromIndex(uint32_t index) {
    auto it = stringCache.find(index);
    if (it != stringCache.end()) return it->second;
    auto result = ReadStringToNull(header.stringOffset + index);
    stringCache[index] = result;
    return result;
}

int Metadata::GetCustomAttributeIndex(int imageIndex, int customAttributeIndex, uint32_t token) const {
    if (Version > 24) {
        auto it = attributeTypeRangesDic.find(imageIndex);
        if (it != attributeTypeRangesDic.end()) {
            auto it2 = it->second.find(token);
            if (it2 != it->second.end()) return it2->second;
        }
        return -1;
    }
    return customAttributeIndex;
}

std::string Metadata::GetStringLiteralFromIndex(uint32_t index) {
    auto& sl = stringLiterals[index];
    SetPosition((uint32_t)(header.stringLiteralDataOffset + sl.dataIndex));
    auto bytes = ReadBytes((int)sl.length);
    return std::string(bytes.begin(), bytes.end());
}

uint32_t Metadata::GetEncodedIndexType(uint32_t index) {
    return (index & 0xE0000000) >> 29;
}

uint32_t Metadata::GetDecodedMethodIndex(uint32_t index) const {
    if (Version >= 27) {
        return (index & 0x1FFFFFFEU) >> 1;
    }
    return index & 0x1FFFFFFFU;
}

void Metadata::ProcessingMetadataUsage() {
    for (uint32_t i = 1; i <= 6; i++) {
        metadataUsageDic[(Il2CppMetadataUsage)i] = {};
    }
    for (auto& list : metadataUsageLists) {
        for (uint32_t i = 0; i < list.count; i++) {
            auto offset = list.start + i;
            if (offset >= metadataUsagePairs.size()) continue;
            auto& pair = metadataUsagePairs[offset];
            auto usage = GetEncodedIndexType(pair.encodedSourceIndex);
            auto decodedIndex = GetDecodedMethodIndex(pair.encodedSourceIndex);
            metadataUsageDic[(Il2CppMetadataUsage)usage][pair.destinationIndex] = decodedIndex;
        }
    }
    uint32_t maxDest = 0;
    for (auto& kv : metadataUsageDic) {
        for (auto& kv2 : kv.second) {
            if (kv2.first > maxDest) maxDest = kv2.first;
        }
    }
    metadataUsagesCount = maxDest + 1;
}

// ============================================================
// SizeOf calculations (matching C# Metadata.SizeOf)
// ============================================================

#define V_IN(min, max) (Version >= (min) && Version <= (max))
#define V_MIN(min) (Version >= (min))
#define V_MAX(max) (Version <= (max))

int Metadata::SizeOfImageDef() const {
    int s = 4+4+4+4; // nameIndex, assemblyIndex, typeStart, typeCount
    if (V_MIN(24)) s += 4+4; // exportedTypeStart, exportedTypeCount
    s += 4; // entryPointIndex
    if (V_MIN(19)) s += 4; // token
    if (V_MIN(24.1)) s += 4+4; // customAttributeStart, customAttributeCount
    return s;
}

int Metadata::SizeOfAssemblyDef() const {
    int s = 4; // imageIndex
    if (V_MIN(24.1)) s += 4; // token
    if (V_MAX(24)) s += 4; // customAttributeIndex
    if (V_MIN(20)) s += 4+4; // referencedAssemblyStart, referencedAssemblyCount
    // aname
    s += 4+4; // nameIndex, cultureIndex
    if (V_MAX(24.3)) s += 4; // hashValueIndex
    s += 4+4+4+4+4+4+4+4; // publicKeyIndex..revision
    s += 8; // public_key_token
    return s;
}

int Metadata::SizeOfTypeDef() const {
    int s = 4+4; // nameIndex, namespaceIndex
    if (V_MAX(24)) s += 4; // customAttributeIndex
    s += 4; // byvalTypeIndex
    if (V_MAX(24.5)) s += 4; // byrefTypeIndex
    s += 4+4+4; // declaringTypeIndex, parentIndex, elementTypeIndex
    if (V_MAX(24.1)) s += 4+4; // rgctxStartIndex, rgctxCount
    s += 4; // genericContainerIndex
    if (V_MAX(22)) s += 4+4; // delegateWrapper, marshalingFunctions
    if (V_IN(21, 22)) s += 4+4; // ccwFunction, guid
    s += 4; // flags
    s += 4*8; // fieldStart..interfaceOffsetsStart
    s += 2*8; // method_count..interface_offsets_count
    s += 4; // bitfield
    if (V_MIN(19)) s += 4; // token
    return s;
}

int Metadata::SizeOfMethodDef() const {
    int s = 4+4+4; // nameIndex, declaringType, returnType
    if (V_MIN(31)) s += 4; // returnParameterToken
    s += 4; // parameterStart
    if (V_MAX(24)) s += 4; // customAttributeIndex
    s += 4; // genericContainerIndex
    if (V_MAX(24.1)) s += 4*5; // methodIndex..rgctxCount
    s += 4; // token
    s += 2+2+2+2; // flags, iflags, slot, parameterCount
    return s;
}

int Metadata::SizeOfParameterDef() const {
    int s = 4+4; // nameIndex, token
    if (V_MAX(24)) s += 4; // customAttributeIndex
    s += 4; // typeIndex
    return s;
}

int Metadata::SizeOfFieldDef() const {
    int s = 4+4; // nameIndex, typeIndex
    if (V_MAX(24)) s += 4; // customAttributeIndex
    if (V_MIN(19)) s += 4; // token
    return s;
}

int Metadata::SizeOfFieldDefaultValue() const { return 4+4+4; }
int Metadata::SizeOfParameterDefaultValue() const { return 4+4+4; }

int Metadata::SizeOfPropertyDef() const {
    int s = 4+4+4+4; // nameIndex, get, set, attrs
    if (V_MAX(24)) s += 4; // customAttributeIndex
    if (V_MIN(19)) s += 4; // token
    return s;
}

int Metadata::SizeOfCustomAttributeTypeRange() const {
    int s = 0;
    if (V_MIN(24.1)) s += 4; // token
    s += 4+4; // start, count
    return s;
}

int Metadata::SizeOfCustomAttributeDataRange() const { return 4+4; }
int Metadata::SizeOfMetadataUsageList() const { return 4+4; }
int Metadata::SizeOfMetadataUsagePair() const { return 4+4; }
int Metadata::SizeOfStringLiteral() const { return 4+4; }

int Metadata::SizeOfEventDef() const {
    int s = 4+4+4+4+4; // nameIndex..raise
    if (V_MAX(24)) s += 4;
    if (V_MIN(19)) s += 4;
    return s;
}

int Metadata::SizeOfGenericContainer() const { return 4*4; }
int Metadata::SizeOfFieldRef() const { return 4+4; }
int Metadata::SizeOfGenericParameter() const { return 4+4+2+2+2+2; }

int Metadata::SizeOfRGCTXDefinition() const {
    int s = 0;
    if (V_MAX(27.1)) s += 4+4; // type_pre29 + data
    if (V_MIN(29)) s += 8; // type_post29 (pointer)
    if (V_MIN(27.2)) s += 8; // _data
    return s;
}

#undef V_IN
#undef V_MIN
#undef V_MAX
