#include "il2cpp/metadata.h"

#include <stdexcept>
#include <string>

namespace il2cppdumper {

Metadata::Metadata(std::vector<uint8_t> data) : BinaryStream(std::move(data)) {
    uint32_t sanity = readU32();
    if (sanity != 0xFAB11BAF) {
        throw std::runtime_error("ERROR: Metadata file supplied is not valid metadata file.");
    }
    int32_t fileVersion = readI32();
    if (fileVersion < 0 || fileVersion > 1000) {
        throw std::runtime_error("ERROR: Metadata file supplied is not valid metadata file.");
    }
    if (fileVersion < 16 || (fileVersion > 31 && fileVersion != 33 && fileVersion != 35 && fileVersion != 38 && fileVersion != 39 && (fileVersion < 104 || fileVersion > 111))) {
        throw std::runtime_error("ERROR: Metadata file supplied is not a supported version[" +
                                 std::to_string(fileVersion) + "].");
    }
    version = fileVersion;
    header = readClassAt<Il2CppGlobalMetadataHeader>(0);
    if (version >= 38) {
        typeIndexSize = header.interfaceOffsetsCount
            ? header.interfaceOffsetsSize / header.interfaceOffsetsCount - 4
            : (header.parametersCount ? header.parametersSize / header.parametersCount - 8 : 4);
        if (typeIndexSize != 1 && typeIndexSize != 2 && typeIndexSize != 4) {
            throw std::runtime_error("Invalid metadata type index width");
        }
        auto indexSize = [](uint32_t count) { return count < 0xff ? 1 : count < 0xffff ? 2 : 4; };
        typeDefinitionIndexSize = indexSize(header.typeDefinitionsCount);
        genericContainerIndexSize = indexSize(header.genericContainersCount);
        parameterIndexSize = version >= 39 ? indexSize(header.parametersCount) : 4;
        if (version >= 104) {
            eventIndexSize = indexSize(header.eventsCount);
            interfaceIndexSize = indexSize(header.interfaceOffsetsCount);
            nestedTypeIndexSize = indexSize(header.nestedTypesCount);
            propertyIndexSize = indexSize(header.propertiesCount);
        }
        if (version >= 105) methodIndexSize = indexSize(header.methodsCount);
        if (version >= 106) {
            genericParameterIndexSize = indexSize(header.genericParametersCount);
            fieldIndexSize = indexSize(header.fieldsCount);
            defaultValueDataIndexSize = indexSize(header.fieldAndParameterDefaultValueDataCount);
            genericContainerRowSize = header.genericContainersCount
                ? header.genericContainersSize / header.genericContainersCount
                : 7 + genericParameterIndexSize;
        }
        if (version >= 108) {
            auto methodSpecCount = header.methodSpecsOnGenericType.count +
                header.genericMethodSpecsOnType.count + header.methodSpecs.count;
            genericMethodIndexSize = indexSize(methodSpecCount);
            invokerTableIndexSize = header.invokerIndices.count
                ? header.invokerIndices.size / header.invokerIndices.count
                : indexSize(header.invokerIndices.count);
            adjustorThunkIndexSize = indexSize(header.genericMethodFunctionsDefinitionsWithAdjustor.count);
            auto genericSpecRowSize = header.genericMethodSpecsOnType.count
                ? header.genericMethodSpecsOnType.size / header.genericMethodSpecsOnType.count
                : (header.methodSpecsOnGenericType.count
                    ? header.methodSpecsOnGenericType.size / header.methodSpecsOnGenericType.count : 0);
            genericInstIndexSize = genericSpecRowSize - methodIndexSize;
            if (genericInstIndexSize != 1 && genericInstIndexSize != 2 && genericInstIndexSize != 4)
                genericInstIndexSize = 4;
            auto methodFunctionsRowSize = header.genericMethodFunctionsDefinitionsWithAdjustor.count
                ? header.genericMethodFunctionsDefinitionsWithAdjustor.size /
                    header.genericMethodFunctionsDefinitionsWithAdjustor.count
                : (header.genericMethodFunctionsDefinitions.count
                    ? header.genericMethodFunctionsDefinitions.size /
                        header.genericMethodFunctionsDefinitions.count : 0);
            methodPointerTableIndexSize = methodFunctionsRowSize - genericMethodIndexSize -
                invokerTableIndexSize - (header.genericMethodFunctionsDefinitionsWithAdjustor.count
                    ? adjustorThunkIndexSize : 0);
            if (methodPointerTableIndexSize != 1 && methodPointerTableIndexSize != 2 &&
                methodPointerTableIndexSize != 4) methodPointerTableIndexSize = 4;
        }
    }
    if (fileVersion == 24) {
        if (header.stringLiteralOffset == 264) {
            version = 24.2;
            header = readClassAt<Il2CppGlobalMetadataHeader>(0);
        } else {
            imageDefs = readMetadataClassArray<Il2CppImageDefinition>(header.imagesOffset, header.imagesSize);
            bool anyTokenNotOne = false;
            for (const auto& def : imageDefs) {
                if (def.token != 1) {
                    anyTokenNotOne = true;
                    break;
                }
            }
            if (anyTokenNotOne) {
                version = 24.1;
            }
        }
    }
    imageDefs = readMetadataClassArray<Il2CppImageDefinition>(header.imagesOffset, header.imagesSize);
    if (version == 24.2 && header.assembliesSize / 68 < static_cast<int>(imageDefs.size())) {
        version = 24.4;
    }
    bool v241Plus = false;
    if (version == 24.1 && header.assembliesSize / 64 == static_cast<int>(imageDefs.size())) {
        v241Plus = true;
    }
    if (v241Plus) {
        version = 24.4;
    }
    auto assemblyCount = version >= 38 ? header.assembliesCount : imageDefs.size();
    assemblyRowSize = assemblyCount ? header.assembliesSize / assemblyCount : 0;
    assemblyDefs = readMetadataClassArray<Il2CppAssemblyDefinition>(header.assembliesOffset, header.assembliesSize);
    if (v241Plus) {
        version = 24.1;
    }
    typeDefs = readMetadataClassArray<Il2CppTypeDefinition>(header.typeDefinitionsOffset, header.typeDefinitionsSize);
    methodDefs = readMetadataClassArray<Il2CppMethodDefinition>(header.methodsOffset, header.methodsSize);
    parameterDefs = readMetadataClassArray<Il2CppParameterDefinition>(header.parametersOffset, header.parametersSize);
    fieldDefs = readMetadataClassArray<Il2CppFieldDefinition>(header.fieldsOffset, header.fieldsSize);
    if (version >= 111 && header.methodSignatures.size > 0) {
        auto blobStart = static_cast<uint64_t>(header.methodSignatures.offset);
        auto blobEnd = blobStart + static_cast<uint64_t>(header.methodSignatures.size);
        for (auto& method : methodDefs) {
            if (method.signatureOffset < 0 || blobStart + method.signatureOffset >= blobEnd) {
                method.returnType = -1;
                continue;
            }
            setPosition(blobStart + method.signatureOffset);
            method.returnType = readIndex(typeIndexSize);
            method.parameterCount = readU8();
            for (int i = 0; i < method.parameterCount && position() + typeIndexSize <= blobEnd; ++i) {
                auto typeIndex = readIndex(typeIndexSize);
                auto parameterIndex = method.parameterStart + i;
                if (parameterIndex >= 0 && static_cast<size_t>(parameterIndex) < parameterDefs.size())
                    parameterDefs[parameterIndex].typeIndex = typeIndex;
            }
        }
    }
    if (version >= 35) {
        for (auto& typeDef : typeDefs) {
            if (!typeDef.isEnum() || typeDef.fieldStart < 0) continue;
            auto end = std::min<size_t>(fieldDefs.size(), (size_t)typeDef.fieldStart + typeDef.field_count);
            for (size_t i = typeDef.fieldStart; i < end; ++i) {
                if (getStringFromIndex(fieldDefs[i].nameIndex) == "value__") {
                    typeDef.elementTypeIndex = fieldDefs[i].typeIndex;
                    break;
                }
            }
        }
    }
    {
        auto fieldDefaultValues = readMetadataClassArray<Il2CppFieldDefaultValue>(header.fieldDefaultValuesOffset,
                                                                                  header.fieldDefaultValuesSize);
        for (const auto& v : fieldDefaultValues) {
            fieldDefaultValuesDic.emplace(v.fieldIndex, v);
        }
    }
    {
        auto parameterDefaultValues = readMetadataClassArray<Il2CppParameterDefaultValue>(
            header.parameterDefaultValuesOffset, header.parameterDefaultValuesSize);
        for (const auto& v : parameterDefaultValues) {
            parameterDefaultValuesDic.emplace(v.parameterIndex, v);
        }
    }
    propertyDefs = readMetadataClassArray<Il2CppPropertyDefinition>(header.propertiesOffset, header.propertiesSize);
    auto readIndexSection = [this](uint32_t offset, int32_t size, uint32_t count, int width) {
        std::vector<int32_t> result;
        if (!count) return result;
        if (size <= 0 || size % count != 0) throw std::runtime_error("Invalid metadata index section");
        auto actualWidth = size / count;
        if (actualWidth == 1 || actualWidth == 2 || actualWidth == 4) width = actualWidth;
        setPosition(offset);
        result.reserve(count);
        for (uint32_t i = 0; i < count; ++i) result.push_back(readIndex(width));
        return result;
    };
    interfaceIndices = version < 38
        ? readClassArrayAt<int>(header.interfacesOffset, header.interfacesSize / 4)
        : readIndexSection(header.interfacesOffset, header.interfacesSize, header.interfacesCount, typeIndexSize);
    nestedTypeIndices = version < 38
        ? readClassArrayAt<int>(header.nestedTypesOffset, header.nestedTypesSize / 4)
        : readIndexSection(header.nestedTypesOffset, header.nestedTypesSize, header.nestedTypesCount, typeDefinitionIndexSize);
    eventDefs = readMetadataClassArray<Il2CppEventDefinition>(header.eventsOffset, header.eventsSize);
    genericContainers = readMetadataClassArray<Il2CppGenericContainer>(header.genericContainersOffset,
                                                                       header.genericContainersSize);
    genericParameters = readMetadataClassArray<Il2CppGenericParameter>(header.genericParametersOffset,
                                                                       header.genericParametersSize);
    constraintIndices = version < 38
        ? readClassArrayAt<int>(header.genericParameterConstraintsOffset, header.genericParameterConstraintsSize / 4)
        : readIndexSection(header.genericParameterConstraintsOffset, header.genericParameterConstraintsSize,
                           header.genericParameterConstraintsCount, typeIndexSize);
    vtableMethods = readClassArrayAt<uint32_t>(header.vtableMethodsOffset, header.vtableMethodsSize / 4);
    stringLiterals = readMetadataClassArray<Il2CppStringLiteral>(header.stringLiteralOffset, header.stringLiteralSize);
    if (version > 16) {
        fieldRefs = readMetadataClassArray<Il2CppFieldRef>(header.fieldRefsOffset, header.fieldRefsSize);
        if (version < 27) {
            metadataUsageLists = readMetadataClassArray<Il2CppMetadataUsageList>(header.metadataUsageListsOffset,
                                                                                 header.metadataUsageListsCount);
            metadataUsagePairs = readMetadataClassArray<Il2CppMetadataUsagePair>(header.metadataUsagePairsOffset,
                                                                                 header.metadataUsagePairsCount);
            processingMetadataUsage();
        }
    }
    if (version > 20 && version < 29) {
        attributeTypeRanges = readMetadataClassArray<Il2CppCustomAttributeTypeRange>(header.attributesInfoOffset,
                                                                                     header.attributesInfoCount);
        attributeTypes = readClassArrayAt<int>(header.attributeTypesOffset, header.attributeTypesCount / 4);
    }
    if (version >= 29) {
        attributeDataRanges = readMetadataClassArray<Il2CppCustomAttributeDataRange>(header.attributeDataRangeOffset,
                                                                                     header.attributeDataRangeSize);
    }
    if (version > 24) {
        for (int i = 0; i < static_cast<int>(imageDefs.size()); i++) {
            const auto& imageDef = imageDefs[static_cast<size_t>(i)];
            auto& dic = attributeTypeRangesDic[i];
            int end = imageDef.customAttributeStart + static_cast<int>(imageDef.customAttributeCount);
            for (int j = imageDef.customAttributeStart; j < end; j++) {
                if (version >= 29) {
                    dic.emplace(attributeDataRanges[static_cast<size_t>(j)].token, j);
                } else {
                    dic.emplace(attributeTypeRanges[static_cast<size_t>(j)].token, j);
                }
            }
        }
    }
    if (version <= 24.1) {
        rgctxEntries = readMetadataClassArray<Il2CppRGCTXDefinition>(header.rgctxEntriesOffset, header.rgctxEntriesCount);
    }
    if (version >= 108) {
        auto readRecords = [this](const MetadataSection& section, auto reader) {
            using T = decltype(reader());
            std::vector<T> result;
            if (section.count == 0) return result;
            setPosition(section.offset);
            result.reserve(section.count);
            for (uint32_t i = 0; i < section.count; ++i) result.push_back(reader());
            return result;
        };
        methodSpecsOnGenericType = readRecords(header.methodSpecsOnGenericType, [this]() {
            Il2CppMethodSpecOnGenericType r;
            r.methodDefinitionIndex = readIndex(methodIndexSize);
            r.classIndexIndex = readIndex(genericInstIndexSize);
            return r;
        });
        genericMethodSpecsOnType = readRecords(header.genericMethodSpecsOnType, [this]() {
            Il2CppGenericMethodSpecOnType r;
            r.methodDefinitionIndex = readIndex(methodIndexSize);
            r.methodIndexIndex = readIndex(genericInstIndexSize);
            return r;
        });
        methodSpecs = readRecords(header.methodSpecs, [this]() {
            Il2CppMethodSpec r;
            r.methodDefinitionIndex = readIndex(methodIndexSize);
            r.classIndexIndex = readIndex(genericInstIndexSize);
            r.methodIndexIndex = readIndex(genericInstIndexSize);
            return r;
        });
        genericMethodFunctionsDefinitions = readRecords(header.genericMethodFunctionsDefinitions, [this]() {
            Il2CppGenericMethodFunctionsDefinitions r;
            r.genericMethodIndex = readIndex(genericMethodIndexSize);
            r.indices.methodIndex = readIndex(methodPointerTableIndexSize);
            r.indices.invokerIndex = readIndex(invokerTableIndexSize);
            return r;
        });
        genericMethodFunctionsDefinitionsWithAdjustor = readRecords(
            header.genericMethodFunctionsDefinitionsWithAdjustor, [this]() {
                Il2CppGenericMethodFunctionsDefinitionsWithAdjustor r;
                r.genericMethodIndex = readIndex(genericMethodIndexSize);
                r.methodIndex = readIndex(methodPointerTableIndexSize);
                r.invokerIndex = readIndex(invokerTableIndexSize);
                r.adjustorThunkIndex = readIndex(adjustorThunkIndexSize);
                return r;
            });
        invokerIndices = readRecords(header.invokerIndices, [this]() {
            return readIndex(invokerTableIndexSize);
        });
        rgctxRanges = readRecords(header.rgctxRanges, [this]() {
            Il2CppTokenRangePair r;
            r.token = readU32();
            r.range.start = readI32();
            r.range.length = readI32();
            return r;
        });
        if (header.rgctxValues.count) {
            auto rowSize = header.rgctxValues.size / header.rgctxValues.count;
            if (rowSize < 5) throw std::runtime_error("Invalid RGCTX row width");
            setPosition(header.rgctxValues.offset);
            rgctxEntries.reserve(header.rgctxValues.count);
            for (uint32_t i = 0; i < header.rgctxValues.count; ++i) {
                auto rowStart = position();
                Il2CppRGCTXDefinition r;
                r.type_post29 = readU8();
                r._data = readU32();
                r.data.rgctxDataDummy = static_cast<int32_t>(r._data);
                rgctxEntries.push_back(r);
                setPosition(rowStart + rowSize);
            }
        }
    }
    if (version >= 110) {
        setPosition(header.generatedMethodTypeInfos.offset);
        for (uint32_t i = 0; i < header.generatedMethodTypeInfos.count; ++i) {
            Il2CppGeneratedMethodTypeInfo r;
            r.typeIndex = readI32();
            r.generatedMethodStart = readI32();
            r.generatedMethodCount = readI32();
            generatedMethodTypeInfoDic.emplace(r.typeIndex, r);
            generatedMethodTypeInfos.push_back(r);
        }
        setPosition(header.generatedMethodTokens.offset);
        for (uint32_t i = 0; i < header.generatedMethodTokens.count; ++i)
            generatedMethodTokens.push_back({readU32()});
        generatedMethodsStart = methodDefs.size() >= generatedMethodTokens.size()
            ? static_cast<int>(methodDefs.size() - generatedMethodTokens.size()) : 0;
        computeV110MetadataTokens();
    }
}

std::string Metadata::getStringFromIndex(uint32_t index) {
    auto it = stringCache.find(index);
    if (it != stringCache.end()) {
        return it->second;
    }
    if (header.stringSize <= 0 || index >= static_cast<uint32_t>(header.stringSize) ||
        static_cast<uint64_t>(header.stringOffset) + index >= length()) return {};
    std::string result = readStringToNull(static_cast<size_t>(header.stringOffset) + index);
    stringCache[index] = result;
    return result;
}

int Metadata::getCustomAttributeIndex(int imageIndex, int customAttributeIndex, uint32_t token) const {
    if (version > 24) {
        auto it = attributeTypeRangesDic.find(imageIndex);
        if (it != attributeTypeRangesDic.end()) {
            auto it2 = it->second.find(token);
            if (it2 != it->second.end()) {
                return it2->second;
            }
        }
        return -1;
    }
    return customAttributeIndex;
}

std::string Metadata::getStringLiteralFromIndex(uint32_t index) {
    const auto& stringLiteral = stringLiterals[index];
    setPosition(static_cast<size_t>(header.stringLiteralDataOffset) + stringLiteral.dataIndex);
    auto length = version <= 31 ? stringLiteral.length :
        (index + 1 < stringLiterals.size()
            ? stringLiterals[index + 1].dataIndex - stringLiteral.dataIndex
            : header.stringLiteralDataSize - stringLiteral.dataIndex);
    auto bytes = readBytes(length);
    return std::string(bytes.begin(), bytes.end());
}

int Metadata::getMethodIndexFromTypeDefinition(int typeIndex, int methodIndexInType) const {
    const auto& type = typeDefs.at(static_cast<size_t>(typeIndex));
    if (version >= 110 && type.hasGeneratedMethods()) {
        auto it = generatedMethodTypeInfoDic.find(typeIndex);
        if (it != generatedMethodTypeInfoDic.end()) {
            int ordinaryCount = type.method_count - it->second.generatedMethodCount;
            if (methodIndexInType >= ordinaryCount)
                return it->second.generatedMethodStart + methodIndexInType - ordinaryCount;
        }
    }
    return type.methodStart + methodIndexInType;
}

Il2CppMethodSpec Metadata::getMethodSpec(int index) const {
    if (version < 108) return methodSpecs.at(static_cast<size_t>(index));
    if (index < static_cast<int>(methodSpecsOnGenericType.size())) {
        const auto& item = methodSpecsOnGenericType[index];
        return {item.methodDefinitionIndex, item.classIndexIndex, -1};
    }
    index -= static_cast<int>(methodSpecsOnGenericType.size());
    if (index < static_cast<int>(genericMethodSpecsOnType.size())) {
        const auto& item = genericMethodSpecsOnType[index];
        return {item.methodDefinitionIndex, -1, item.methodIndexIndex};
    }
    index -= static_cast<int>(genericMethodSpecsOnType.size());
    return methodSpecs.at(static_cast<size_t>(index));
}

uint32_t Metadata::getEncodedIndexTypeForVersion(uint32_t index) const {
    auto usage = getEncodedIndexType(index);
    if (((version >= 106.1 && version < 107) || version >= 108) && usage >= 2) ++usage;
    return usage;
}

uint32_t Metadata::getMethodToken(const Il2CppImageDefinition& image,
                                  const Il2CppTypeDefinition& type, int typeIndex,
                                  int methodIndex) const {
    if (version < 110) return methodDefs.at(static_cast<size_t>(methodIndex)).token;
    if (type.hasGeneratedMethods()) {
        auto it = generatedMethodTypeInfoDic.find(typeIndex);
        if (it != generatedMethodTypeInfoDic.end() &&
            methodIndex >= it->second.generatedMethodStart &&
            methodIndex < it->second.generatedMethodStart + it->second.generatedMethodCount) {
            auto tokenIndex = methodIndex - generatedMethodsStart;
            if (tokenIndex >= 0 && static_cast<size_t>(tokenIndex) < generatedMethodTokens.size())
                return generatedMethodTokens[tokenIndex].token;
        }
    }
    return 0x06000000u | static_cast<uint32_t>(methodIndex - image.methodStart + 1);
}

void Metadata::computeV110MetadataTokens() {
    for (const auto& image : imageDefs) {
        for (int typeIndex = image.typeStart;
             typeIndex < image.typeStart + static_cast<int>(image.typeCount); ++typeIndex) {
            if (typeIndex < 0 || static_cast<size_t>(typeIndex) >= typeDefs.size()) continue;
            auto& type = typeDefs[typeIndex];
            type.token = 0x02000000u | static_cast<uint32_t>(typeIndex - image.typeStart + 1);
            for (int i = 0; i < type.field_count; ++i) {
                int index = type.fieldStart + i;
                if (index >= 0 && static_cast<size_t>(index) < fieldDefs.size())
                    fieldDefs[index].token = 0x04000000u | static_cast<uint32_t>(index - image.fieldStart + 1);
            }
            for (int i = 0; i < type.property_count; ++i) {
                int index = type.propertyStart + i;
                if (index >= 0 && static_cast<size_t>(index) < propertyDefs.size())
                    propertyDefs[index].token = 0x17000000u | static_cast<uint32_t>(index - image.propertyStart + 1);
            }
            for (int i = 0; i < type.event_count; ++i) {
                int index = type.eventStart + i;
                if (index >= 0 && static_cast<size_t>(index) < eventDefs.size())
                    eventDefs[index].token = 0x14000000u | static_cast<uint32_t>(index - image.eventStart + 1);
            }
            for (int i = 0; i < type.method_count; ++i) {
                int index = getMethodIndexFromTypeDefinition(typeIndex, i);
                if (index >= 0 && static_cast<size_t>(index) < methodDefs.size())
                    methodDefs[index].token = getMethodToken(image, type, typeIndex, index);
            }
        }
    }
}

void Metadata::processingMetadataUsage() {
    for (const auto& metadataUsageList : metadataUsageLists) {
        for (uint32_t i = 0; i < metadataUsageList.count; i++) {
            uint32_t offset = metadataUsageList.start + i;
            if (offset >= metadataUsagePairs.size()) {
                continue;
            }
            const auto& metadataUsagePair = metadataUsagePairs[offset];
            uint32_t usage = getEncodedIndexTypeForVersion(metadataUsagePair.encodedSourceIndex);
            uint32_t decodedIndex = getDecodedMethodIndex(metadataUsagePair.encodedSourceIndex);
            if (usage >= 1 && usage <= 6) {
                metadataUsageDic[usage][metadataUsagePair.destinationIndex] = decodedIndex;
            }
        }
    }
    uint32_t maxDestination = 0;
    for (const auto& entry : metadataUsageDic) {
        if (!entry.empty()) {
            uint32_t last = entry.rbegin()->first;
            if (last > maxDestination) {
                maxDestination = last;
            }
        }
    }
    metadataUsagesCount = static_cast<int64_t>(maxDestination) + 1;
}

}
