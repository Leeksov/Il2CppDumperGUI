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
    if (fileVersion < 16 || (fileVersion > 31 && fileVersion != 33 && fileVersion != 35 && fileVersion != 38 && fileVersion != 39)) {
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
    assemblyDefs = readMetadataClassArray<Il2CppAssemblyDefinition>(header.assembliesOffset, header.assembliesSize);
    if (v241Plus) {
        version = 24.1;
    }
    typeDefs = readMetadataClassArray<Il2CppTypeDefinition>(header.typeDefinitionsOffset, header.typeDefinitionsSize);
    methodDefs = readMetadataClassArray<Il2CppMethodDefinition>(header.methodsOffset, header.methodsSize);
    parameterDefs = readMetadataClassArray<Il2CppParameterDefinition>(header.parametersOffset, header.parametersSize);
    fieldDefs = readMetadataClassArray<Il2CppFieldDefinition>(header.fieldsOffset, header.fieldsSize);
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
            fieldDefaultValuesDic[v.fieldIndex] = v;
        }
    }
    {
        auto parameterDefaultValues = readMetadataClassArray<Il2CppParameterDefaultValue>(
            header.parameterDefaultValuesOffset, header.parameterDefaultValuesSize);
        for (const auto& v : parameterDefaultValues) {
            parameterDefaultValuesDic[v.parameterIndex] = v;
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
                    dic[attributeDataRanges[static_cast<size_t>(j)].token] = j;
                } else {
                    dic[attributeTypeRanges[static_cast<size_t>(j)].token] = j;
                }
            }
        }
    }
    if (version <= 24.1) {
        rgctxEntries = readMetadataClassArray<Il2CppRGCTXDefinition>(header.rgctxEntriesOffset, header.rgctxEntriesCount);
    }
}

std::string Metadata::getStringFromIndex(uint32_t index) {
    auto it = stringCache.find(index);
    if (it != stringCache.end()) {
        return it->second;
    }
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

void Metadata::processingMetadataUsage() {
    for (const auto& metadataUsageList : metadataUsageLists) {
        for (uint32_t i = 0; i < metadataUsageList.count; i++) {
            uint32_t offset = metadataUsageList.start + i;
            if (offset >= metadataUsagePairs.size()) {
                continue;
            }
            const auto& metadataUsagePair = metadataUsagePairs[offset];
            uint32_t usage = getEncodedIndexType(metadataUsagePair.encodedSourceIndex);
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
