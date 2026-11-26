#pragma once

#include "io/binary_stream.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace il2cppdumper {


enum class Il2CppRGCTXDataType : int {
    IL2CPP_RGCTX_DATA_INVALID = 0,
    IL2CPP_RGCTX_DATA_TYPE = 1,
    IL2CPP_RGCTX_DATA_CLASS = 2,
    IL2CPP_RGCTX_DATA_METHOD = 3,
    IL2CPP_RGCTX_DATA_ARRAY = 4,
    IL2CPP_RGCTX_DATA_CONSTRAINED = 5,
};

enum class Il2CppMetadataUsage : int {
    kIl2CppMetadataUsageInvalid = 0,
    kIl2CppMetadataUsageTypeInfo = 1,
    kIl2CppMetadataUsageIl2CppType = 2,
    kIl2CppMetadataUsageMethodDef = 3,
    kIl2CppMetadataUsageFieldInfo = 4,
    kIl2CppMetadataUsageStringLiteral = 5,
    kIl2CppMetadataUsageMethodRef = 6,
};

struct Il2CppGlobalMetadataHeader {
    uint32_t sanity = 0;
    int32_t version = 0;
    uint32_t stringLiteralOffset = 0;
    int32_t stringLiteralSize = 0;
    uint32_t stringLiteralDataOffset = 0;
    int32_t stringLiteralDataSize = 0;
    uint32_t stringOffset = 0;
    int32_t stringSize = 0;
    uint32_t eventsOffset = 0;
    int32_t eventsSize = 0;
    uint32_t propertiesOffset = 0;
    int32_t propertiesSize = 0;
    uint32_t methodsOffset = 0;
    int32_t methodsSize = 0;
    uint32_t parameterDefaultValuesOffset = 0;
    int32_t parameterDefaultValuesSize = 0;
    uint32_t fieldDefaultValuesOffset = 0;
    int32_t fieldDefaultValuesSize = 0;
    uint32_t fieldAndParameterDefaultValueDataOffset = 0;
    int32_t fieldAndParameterDefaultValueDataSize = 0;
    int32_t fieldMarshaledSizesOffset = 0;
    int32_t fieldMarshaledSizesSize = 0;
    uint32_t parametersOffset = 0;
    int32_t parametersSize = 0;
    uint32_t fieldsOffset = 0;
    int32_t fieldsSize = 0;
    uint32_t genericParametersOffset = 0;
    int32_t genericParametersSize = 0;
    uint32_t genericParameterConstraintsOffset = 0;
    int32_t genericParameterConstraintsSize = 0;
    uint32_t genericContainersOffset = 0;
    int32_t genericContainersSize = 0;
    uint32_t nestedTypesOffset = 0;
    int32_t nestedTypesSize = 0;
    uint32_t interfacesOffset = 0;
    int32_t interfacesSize = 0;
    uint32_t vtableMethodsOffset = 0;
    int32_t vtableMethodsSize = 0;
    int32_t interfaceOffsetsOffset = 0;
    int32_t interfaceOffsetsSize = 0;
    uint32_t typeDefinitionsOffset = 0;
    int32_t typeDefinitionsSize = 0;
    uint32_t rgctxEntriesOffset = 0;
    int32_t rgctxEntriesCount = 0;
    uint32_t imagesOffset = 0;
    int32_t imagesSize = 0;
    uint32_t assembliesOffset = 0;
    int32_t assembliesSize = 0;
    uint32_t metadataUsageListsOffset = 0;
    int32_t metadataUsageListsCount = 0;
    uint32_t metadataUsagePairsOffset = 0;
    int32_t metadataUsagePairsCount = 0;
    uint32_t fieldRefsOffset = 0;
    int32_t fieldRefsSize = 0;
    int32_t referencedAssembliesOffset = 0;
    int32_t referencedAssembliesSize = 0;
    uint32_t attributesInfoOffset = 0;
    int32_t attributesInfoCount = 0;
    uint32_t attributeTypesOffset = 0;
    int32_t attributeTypesCount = 0;
    uint32_t attributeDataOffset = 0;
    int32_t attributeDataSize = 0;
    uint32_t attributeDataRangeOffset = 0;
    int32_t attributeDataRangeSize = 0;
    int32_t unresolvedVirtualCallParameterTypesOffset = 0;
    int32_t unresolvedVirtualCallParameterTypesSize = 0;
    int32_t unresolvedVirtualCallParameterRangesOffset = 0;
    int32_t unresolvedVirtualCallParameterRangesSize = 0;
    int32_t windowsRuntimeTypeNamesOffset = 0;
    int32_t windowsRuntimeTypeNamesSize = 0;
    int32_t windowsRuntimeStringsOffset = 0;
    int32_t windowsRuntimeStringsSize = 0;
    int32_t exportedTypeDefinitionsOffset = 0;
    int32_t exportedTypeDefinitionsSize = 0;
    uint32_t parametersCount = 0;
    uint32_t typeDefinitionsCount = 0;
    uint32_t genericContainersCount = 0;
    uint32_t interfacesCount = 0;
    uint32_t interfaceOffsetsCount = 0;
    uint32_t nestedTypesCount = 0;
    uint32_t genericParameterConstraintsCount = 0;
};

struct Il2CppAssemblyNameDefinition {
    uint32_t nameIndex = 0;
    uint32_t cultureIndex = 0;
    int32_t hashValueIndex = 0;
    uint32_t publicKeyIndex = 0;
    uint32_t hash_alg = 0;
    int32_t hash_len = 0;
    uint32_t flags = 0;
    int32_t major = 0;
    int32_t minor = 0;
    int32_t build = 0;
    int32_t revision = 0;
    uint8_t public_key_token[8] = {0};
};

struct Il2CppAssemblyDefinition {
    int32_t imageIndex = 0;
    uint32_t token = 0;
    uint32_t moduleToken = 0;
    int32_t customAttributeIndex = 0;
    int32_t referencedAssemblyStart = 0;
    int32_t referencedAssemblyCount = 0;
    Il2CppAssemblyNameDefinition aname;
};

struct Il2CppImageDefinition {
    uint32_t nameIndex = 0;
    int32_t assemblyIndex = 0;
    int32_t typeStart = 0;
    uint32_t typeCount = 0;
    int32_t exportedTypeStart = 0;
    uint32_t exportedTypeCount = 0;
    int32_t entryPointIndex = 0;
    uint32_t token = 0;
    int32_t customAttributeStart = 0;
    uint32_t customAttributeCount = 0;
};

struct Il2CppTypeDefinition {
    uint32_t nameIndex = 0;
    uint32_t namespaceIndex = 0;
    int32_t customAttributeIndex = 0;
    int32_t byvalTypeIndex = 0;
    int32_t byrefTypeIndex = 0;
    int32_t declaringTypeIndex = 0;
    int32_t parentIndex = 0;
    int32_t elementTypeIndex = 0;
    int32_t enumElementTypeIndex(double) const { return elementTypeIndex; }
    int32_t rgctxStartIndex = 0;
    int32_t rgctxCount = 0;
    int32_t genericContainerIndex = 0;
    int32_t delegateWrapperFromManagedToNativeIndex = 0;
    int32_t marshalingFunctionsIndex = 0;
    int32_t ccwFunctionIndex = 0;
    int32_t guidIndex = 0;
    uint32_t flags = 0;
    int32_t fieldStart = 0;
    int32_t methodStart = 0;
    int32_t eventStart = 0;
    int32_t propertyStart = 0;
    int32_t nestedTypesStart = 0;
    int32_t interfacesStart = 0;
    int32_t vtableStart = 0;
    int32_t interfaceOffsetsStart = 0;
    uint16_t method_count = 0;
    uint16_t property_count = 0;
    uint16_t field_count = 0;
    uint16_t event_count = 0;
    uint16_t nested_type_count = 0;
    uint16_t vtable_count = 0;
    uint16_t interfaces_count = 0;
    uint16_t interface_offsets_count = 0;
    uint32_t bitfield = 0;
    uint32_t token = 0;

    bool isValueType() const { return (bitfield & 0x1) == 1; }
    bool isEnum() const { return ((bitfield >> 1) & 0x1) == 1; }
};

struct Il2CppMethodDefinition {
    uint32_t nameIndex = 0;
    int32_t declaringType = 0;
    int32_t returnType = 0;
    int32_t returnParameterToken = 0;
    int32_t parameterStart = 0;
    int32_t customAttributeIndex = 0;
    int32_t genericContainerIndex = 0;
    int32_t methodIndex = 0;
    int32_t invokerIndex = 0;
    int32_t delegateWrapperIndex = 0;
    int32_t rgctxStartIndex = 0;
    int32_t rgctxCount = 0;
    uint32_t token = 0;
    uint16_t flags = 0;
    uint16_t iflags = 0;
    uint16_t slot = 0;
    uint16_t parameterCount = 0;
};

struct Il2CppParameterDefinition {
    uint32_t nameIndex = 0;
    uint32_t token = 0;
    int32_t customAttributeIndex = 0;
    int32_t typeIndex = 0;
};

struct Il2CppFieldDefinition {
    uint32_t nameIndex = 0;
    int32_t typeIndex = 0;
    int32_t customAttributeIndex = 0;
    uint32_t token = 0;
};

struct Il2CppFieldDefaultValue {
    int32_t fieldIndex = 0;
    int32_t typeIndex = 0;
    int32_t dataIndex = 0;
};

struct Il2CppPropertyDefinition {
    uint32_t nameIndex = 0;
    int32_t get = 0;
    int32_t set = 0;
    uint32_t attrs = 0;
    int32_t customAttributeIndex = 0;
    uint32_t token = 0;
};

struct Il2CppCustomAttributeTypeRange {
    uint32_t token = 0;
    int32_t start = 0;
    int32_t count = 0;
};

struct Il2CppMetadataUsageList {
    uint32_t start = 0;
    uint32_t count = 0;
};

struct Il2CppMetadataUsagePair {
    uint32_t destinationIndex = 0;
    uint32_t encodedSourceIndex = 0;
};

struct Il2CppStringLiteral {
    uint32_t length = 0;
    int32_t dataIndex = 0;
};

struct Il2CppParameterDefaultValue {
    int32_t parameterIndex = 0;
    int32_t typeIndex = 0;
    int32_t dataIndex = 0;
};

struct Il2CppEventDefinition {
    uint32_t nameIndex = 0;
    int32_t typeIndex = 0;
    int32_t add = 0;
    int32_t remove = 0;
    int32_t raise = 0;
    int32_t customAttributeIndex = 0;
    uint32_t token = 0;
};

struct Il2CppGenericContainer {
    int32_t ownerIndex = 0;
    int32_t type_argc = 0;
    int32_t is_method = 0;
    int32_t genericParameterStart = 0;
};

struct Il2CppFieldRef {
    int32_t typeIndex = 0;
    int32_t fieldIndex = 0;
};

struct Il2CppGenericParameter {
    int32_t ownerIndex = 0;
    uint32_t nameIndex = 0;
    int16_t constraintsStart = 0;
    int16_t constraintsCount = 0;
    uint16_t num = 0;
    uint16_t flags = 0;
};

struct Il2CppRGCTXDefinitionData {
    int32_t rgctxDataDummy = 0;
    int32_t methodIndex() const { return rgctxDataDummy; }
    int32_t typeIndex() const { return rgctxDataDummy; }
};

struct Il2CppRGCTXDefinition {
    int type() const {
        return type_post29 == 0 ? type_pre29 : static_cast<int>(type_post29);
    }
    int32_t type_pre29 = 0;
    uint64_t type_post29 = 0;
    Il2CppRGCTXDefinitionData data;
    uint64_t _data = 0;
};

struct Il2CppCustomAttributeDataRange {
    uint32_t token = 0;
    uint32_t startOffset = 0;
};

template <>
inline Il2CppGlobalMetadataHeader BinaryStream::readClass<Il2CppGlobalMetadataHeader>() {
    Il2CppGlobalMetadataHeader t{};
    t.sanity = readU32();
    t.version = readI32();
    if (version >= 38) {
        auto section = [this](auto& offset, auto& size, uint32_t* count = nullptr) {
            offset = readU32();
            size = readI32();
            auto n = readU32();
            if (count) *count = n;
        };
        section(t.stringLiteralOffset, t.stringLiteralSize);
        section(t.stringLiteralDataOffset, t.stringLiteralDataSize);
        section(t.stringOffset, t.stringSize);
        section(t.eventsOffset, t.eventsSize);
        section(t.propertiesOffset, t.propertiesSize);
        section(t.methodsOffset, t.methodsSize);
        section(t.parameterDefaultValuesOffset, t.parameterDefaultValuesSize);
        section(t.fieldDefaultValuesOffset, t.fieldDefaultValuesSize);
        section(t.fieldAndParameterDefaultValueDataOffset, t.fieldAndParameterDefaultValueDataSize);
        section(t.fieldMarshaledSizesOffset, t.fieldMarshaledSizesSize);
        section(t.parametersOffset, t.parametersSize, &t.parametersCount);
        section(t.fieldsOffset, t.fieldsSize);
        section(t.genericParametersOffset, t.genericParametersSize);
        section(t.genericParameterConstraintsOffset, t.genericParameterConstraintsSize, &t.genericParameterConstraintsCount);
        section(t.genericContainersOffset, t.genericContainersSize, &t.genericContainersCount);
        section(t.nestedTypesOffset, t.nestedTypesSize, &t.nestedTypesCount);
        section(t.interfacesOffset, t.interfacesSize, &t.interfacesCount);
        section(t.vtableMethodsOffset, t.vtableMethodsSize);
        section(t.interfaceOffsetsOffset, t.interfaceOffsetsSize, &t.interfaceOffsetsCount);
        section(t.typeDefinitionsOffset, t.typeDefinitionsSize, &t.typeDefinitionsCount);
        section(t.imagesOffset, t.imagesSize);
        section(t.assembliesOffset, t.assembliesSize);
        section(t.fieldRefsOffset, t.fieldRefsSize);
        section(t.referencedAssembliesOffset, t.referencedAssembliesSize);
        section(t.attributeDataOffset, t.attributeDataSize);
        section(t.attributeDataRangeOffset, t.attributeDataRangeSize);
        section(t.unresolvedVirtualCallParameterTypesOffset, t.unresolvedVirtualCallParameterTypesSize);
        section(t.unresolvedVirtualCallParameterRangesOffset, t.unresolvedVirtualCallParameterRangesSize);
        section(t.windowsRuntimeTypeNamesOffset, t.windowsRuntimeTypeNamesSize);
        section(t.windowsRuntimeStringsOffset, t.windowsRuntimeStringsSize);
        section(t.exportedTypeDefinitionsOffset, t.exportedTypeDefinitionsSize);
        return t;
    }
    t.stringLiteralOffset = readU32();
    t.stringLiteralSize = readI32();
    t.stringLiteralDataOffset = readU32();
    t.stringLiteralDataSize = readI32();
    t.stringOffset = readU32();
    t.stringSize = readI32();
    t.eventsOffset = readU32();
    t.eventsSize = readI32();
    t.propertiesOffset = readU32();
    t.propertiesSize = readI32();
    t.methodsOffset = readU32();
    t.methodsSize = readI32();
    t.parameterDefaultValuesOffset = readU32();
    t.parameterDefaultValuesSize = readI32();
    t.fieldDefaultValuesOffset = readU32();
    t.fieldDefaultValuesSize = readI32();
    t.fieldAndParameterDefaultValueDataOffset = readU32();
    t.fieldAndParameterDefaultValueDataSize = readI32();
    t.fieldMarshaledSizesOffset = readI32();
    t.fieldMarshaledSizesSize = readI32();
    t.parametersOffset = readU32();
    t.parametersSize = readI32();
    t.fieldsOffset = readU32();
    t.fieldsSize = readI32();
    t.genericParametersOffset = readU32();
    t.genericParametersSize = readI32();
    t.genericParameterConstraintsOffset = readU32();
    t.genericParameterConstraintsSize = readI32();
    t.genericContainersOffset = readU32();
    t.genericContainersSize = readI32();
    t.nestedTypesOffset = readU32();
    t.nestedTypesSize = readI32();
    t.interfacesOffset = readU32();
    t.interfacesSize = readI32();
    t.vtableMethodsOffset = readU32();
    t.vtableMethodsSize = readI32();
    t.interfaceOffsetsOffset = readI32();
    t.interfaceOffsetsSize = readI32();
    t.typeDefinitionsOffset = readU32();
    t.typeDefinitionsSize = readI32();
    if (inRange(0, 24.1)) {
        t.rgctxEntriesOffset = readU32();
        t.rgctxEntriesCount = readI32();
    }
    t.imagesOffset = readU32();
    t.imagesSize = readI32();
    t.assembliesOffset = readU32();
    t.assembliesSize = readI32();
    if (inRange(19, 24.5)) {
        t.metadataUsageListsOffset = readU32();
        t.metadataUsageListsCount = readI32();
        t.metadataUsagePairsOffset = readU32();
        t.metadataUsagePairsCount = readI32();
    }
    if (inRange(19, 99)) {
        t.fieldRefsOffset = readU32();
        t.fieldRefsSize = readI32();
    }
    if (inRange(20, 99)) {
        t.referencedAssembliesOffset = readI32();
        t.referencedAssembliesSize = readI32();
    }
    if (inRange(21, 27.2)) {
        t.attributesInfoOffset = readU32();
        t.attributesInfoCount = readI32();
        t.attributeTypesOffset = readU32();
        t.attributeTypesCount = readI32();
    }
    if (inRange(29, 99)) {
        t.attributeDataOffset = readU32();
        t.attributeDataSize = readI32();
        t.attributeDataRangeOffset = readU32();
        t.attributeDataRangeSize = readI32();
    }
    if (inRange(22, 99)) {
        t.unresolvedVirtualCallParameterTypesOffset = readI32();
        t.unresolvedVirtualCallParameterTypesSize = readI32();
        t.unresolvedVirtualCallParameterRangesOffset = readI32();
        t.unresolvedVirtualCallParameterRangesSize = readI32();
    }
    if (inRange(23, 99)) {
        t.windowsRuntimeTypeNamesOffset = readI32();
        t.windowsRuntimeTypeNamesSize = readI32();
    }
    if (inRange(27, 99)) {
        t.windowsRuntimeStringsOffset = readI32();
        t.windowsRuntimeStringsSize = readI32();
    }
    if (inRange(24, 99)) {
        t.exportedTypeDefinitionsOffset = readI32();
        t.exportedTypeDefinitionsSize = readI32();
    }
    return t;
}

template <>
inline Il2CppAssemblyNameDefinition BinaryStream::readClass<Il2CppAssemblyNameDefinition>() {
    Il2CppAssemblyNameDefinition t{};
    t.nameIndex = readU32();
    t.cultureIndex = readU32();
    if (inRange(0, 24.3)) {
        t.hashValueIndex = readI32();
    }
    t.publicKeyIndex = readU32();
    t.hash_alg = readU32();
    t.hash_len = readI32();
    t.flags = readU32();
    t.major = readI32();
    t.minor = readI32();
    t.build = readI32();
    t.revision = readI32();
    for (int i = 0; i < 8; i++) {
        t.public_key_token[i] = readU8();
    }
    return t;
}

template <>
inline Il2CppAssemblyDefinition BinaryStream::readClass<Il2CppAssemblyDefinition>() {
    Il2CppAssemblyDefinition t{};
    t.imageIndex = readI32();
    if (inRange(24.1, 99)) {
        t.token = readU32();
    }
    if (version >= 38) t.moduleToken = readU32();
    if (inRange(0, 24)) {
        t.customAttributeIndex = readI32();
    }
    if (inRange(20, 99)) {
        t.referencedAssemblyStart = readI32();
        t.referencedAssemblyCount = readI32();
    }
    t.aname = readClass<Il2CppAssemblyNameDefinition>();
    return t;
}

template <>
inline Il2CppImageDefinition BinaryStream::readClass<Il2CppImageDefinition>() {
    Il2CppImageDefinition t{};
    t.nameIndex = readU32();
    t.assemblyIndex = readI32();
    t.typeStart = readIndex(typeDefinitionIndexSize);
    t.typeCount = readU32();
    if (inRange(24, 99)) {
        t.exportedTypeStart = readIndex(typeDefinitionIndexSize);
        t.exportedTypeCount = readU32();
    }
    t.entryPointIndex = readI32();
    if (inRange(19, 99)) {
        t.token = readU32();
    }
    if (inRange(24.1, 99)) {
        t.customAttributeStart = readI32();
        t.customAttributeCount = readU32();
    }
    return t;
}

template <>
inline Il2CppTypeDefinition BinaryStream::readClass<Il2CppTypeDefinition>() {
    Il2CppTypeDefinition t{};
    t.nameIndex = readU32();
    t.namespaceIndex = readU32();
    if (inRange(0, 24)) {
        t.customAttributeIndex = readI32();
    }
    t.byvalTypeIndex = readIndex(typeIndexSize);
    if (inRange(0, 24.5)) {
        t.byrefTypeIndex = readI32();
    }
    t.declaringTypeIndex = readIndex(typeIndexSize);
    t.parentIndex = readIndex(typeIndexSize);
    if (version <= 31) t.elementTypeIndex = readI32();
    if (inRange(0, 24.1)) {
        t.rgctxStartIndex = readI32();
        t.rgctxCount = readI32();
    }
    t.genericContainerIndex = readIndex(genericContainerIndexSize);
    if (inRange(0, 22)) {
        t.delegateWrapperFromManagedToNativeIndex = readI32();
        t.marshalingFunctionsIndex = readI32();
    }
    if (inRange(21, 22)) {
        t.ccwFunctionIndex = readI32();
        t.guidIndex = readI32();
    }
    t.flags = readU32();
    t.fieldStart = readI32();
    t.methodStart = readI32();
    t.eventStart = readI32();
    t.propertyStart = readI32();
    t.nestedTypesStart = readI32();
    t.interfacesStart = readI32();
    t.vtableStart = readI32();
    t.interfaceOffsetsStart = readI32();
    t.method_count = readU16();
    t.property_count = readU16();
    t.field_count = readU16();
    t.event_count = readU16();
    t.nested_type_count = readU16();
    t.vtable_count = readU16();
    t.interfaces_count = readU16();
    t.interface_offsets_count = readU16();
    t.bitfield = readU32();
    if (inRange(19, 99)) {
        t.token = readU32();
    }
    return t;
}

template <>
inline Il2CppMethodDefinition BinaryStream::readClass<Il2CppMethodDefinition>() {
    Il2CppMethodDefinition t{};
    t.nameIndex = readU32();
    t.declaringType = readIndex(typeDefinitionIndexSize);
    t.returnType = readIndex(typeIndexSize);
    if (inRange(31, 99)) {
        t.returnParameterToken = readI32();
    }
    t.parameterStart = readIndex(parameterIndexSize);
    if (inRange(0, 24)) {
        t.customAttributeIndex = readI32();
    }
    t.genericContainerIndex = readIndex(genericContainerIndexSize);
    if (inRange(0, 24.1)) {
        t.methodIndex = readI32();
        t.invokerIndex = readI32();
        t.delegateWrapperIndex = readI32();
        t.rgctxStartIndex = readI32();
        t.rgctxCount = readI32();
    }
    t.token = readU32();
    t.flags = readU16();
    t.iflags = readU16();
    t.slot = readU16();
    t.parameterCount = readU16();
    return t;
}

template <>
inline Il2CppParameterDefinition BinaryStream::readClass<Il2CppParameterDefinition>() {
    Il2CppParameterDefinition t{};
    t.nameIndex = readU32();
    t.token = readU32();
    if (inRange(0, 24)) {
        t.customAttributeIndex = readI32();
    }
    t.typeIndex = readIndex(typeIndexSize);
    return t;
}

template <>
inline Il2CppFieldDefinition BinaryStream::readClass<Il2CppFieldDefinition>() {
    Il2CppFieldDefinition t{};
    t.nameIndex = readU32();
    t.typeIndex = readIndex(typeIndexSize);
    if (inRange(0, 24)) {
        t.customAttributeIndex = readI32();
    }
    if (inRange(19, 99)) {
        t.token = readU32();
    }
    return t;
}

template <>
inline Il2CppFieldDefaultValue BinaryStream::readClass<Il2CppFieldDefaultValue>() {
    Il2CppFieldDefaultValue t{};
    t.fieldIndex = readI32();
    t.typeIndex = readIndex(typeIndexSize);
    t.dataIndex = readI32();
    return t;
}

template <>
inline Il2CppPropertyDefinition BinaryStream::readClass<Il2CppPropertyDefinition>() {
    Il2CppPropertyDefinition t{};
    t.nameIndex = readU32();
    t.get = readI32();
    t.set = readI32();
    t.attrs = readU32();
    if (inRange(0, 24)) {
        t.customAttributeIndex = readI32();
    }
    if (inRange(19, 99)) {
        t.token = readU32();
    }
    return t;
}

template <>
inline Il2CppCustomAttributeTypeRange BinaryStream::readClass<Il2CppCustomAttributeTypeRange>() {
    Il2CppCustomAttributeTypeRange t{};
    if (inRange(24.1, 99)) {
        t.token = readU32();
    }
    t.start = readI32();
    t.count = readI32();
    return t;
}

template <>
inline Il2CppMetadataUsageList BinaryStream::readClass<Il2CppMetadataUsageList>() {
    Il2CppMetadataUsageList t{};
    t.start = readU32();
    t.count = readU32();
    return t;
}

template <>
inline Il2CppMetadataUsagePair BinaryStream::readClass<Il2CppMetadataUsagePair>() {
    Il2CppMetadataUsagePair t{};
    t.destinationIndex = readU32();
    t.encodedSourceIndex = readU32();
    return t;
}

template <>
inline Il2CppStringLiteral BinaryStream::readClass<Il2CppStringLiteral>() {
    Il2CppStringLiteral t{};
    if (version <= 31) t.length = readU32();
    t.dataIndex = readI32();
    return t;
}

template <>
inline Il2CppParameterDefaultValue BinaryStream::readClass<Il2CppParameterDefaultValue>() {
    Il2CppParameterDefaultValue t{};
    t.parameterIndex = readIndex(parameterIndexSize);
    t.typeIndex = readIndex(typeIndexSize);
    t.dataIndex = readI32();
    return t;
}

template <>
inline Il2CppEventDefinition BinaryStream::readClass<Il2CppEventDefinition>() {
    Il2CppEventDefinition t{};
    t.nameIndex = readU32();
    t.typeIndex = readIndex(typeIndexSize);
    t.add = readI32();
    t.remove = readI32();
    t.raise = readI32();
    if (inRange(0, 24)) {
        t.customAttributeIndex = readI32();
    }
    if (inRange(19, 99)) {
        t.token = readU32();
    }
    return t;
}

template <>
inline Il2CppGenericContainer BinaryStream::readClass<Il2CppGenericContainer>() {
    Il2CppGenericContainer t{};
    t.ownerIndex = readI32();
    t.type_argc = readI32();
    t.is_method = readI32();
    t.genericParameterStart = readI32();
    return t;
}

template <>
inline Il2CppFieldRef BinaryStream::readClass<Il2CppFieldRef>() {
    Il2CppFieldRef t{};
    t.typeIndex = readIndex(typeIndexSize);
    t.fieldIndex = readI32();
    return t;
}

template <>
inline Il2CppGenericParameter BinaryStream::readClass<Il2CppGenericParameter>() {
    Il2CppGenericParameter t{};
    t.ownerIndex = readIndex(genericContainerIndexSize);
    t.nameIndex = readU32();
    t.constraintsStart = readI16();
    t.constraintsCount = readI16();
    t.num = readU16();
    t.flags = readU16();
    return t;
}

template <>
inline Il2CppRGCTXDefinitionData BinaryStream::readClass<Il2CppRGCTXDefinitionData>() {
    Il2CppRGCTXDefinitionData t{};
    t.rgctxDataDummy = readI32();
    return t;
}

template <>
inline Il2CppRGCTXDefinition BinaryStream::readClass<Il2CppRGCTXDefinition>() {
    Il2CppRGCTXDefinition t{};
    if (inRange(0, 27.1)) {
        t.type_pre29 = readI32();
        t.data = readClass<Il2CppRGCTXDefinitionData>();
    }
    if (inRange(29, 99)) {
        t.type_post29 = readUPtr();
    }
    if (inRange(27.2, 99)) {
        t._data = readUPtr();
    }
    return t;
}

template <>
inline Il2CppCustomAttributeDataRange BinaryStream::readClass<Il2CppCustomAttributeDataRange>() {
    Il2CppCustomAttributeDataRange t{};
    t.token = readU32();
    t.startOffset = readU32();
    return t;
}

class Metadata : public BinaryStream {
public:
    Il2CppGlobalMetadataHeader header;
    std::vector<Il2CppImageDefinition> imageDefs;
    std::vector<Il2CppAssemblyDefinition> assemblyDefs;
    std::vector<Il2CppTypeDefinition> typeDefs;
    std::vector<Il2CppMethodDefinition> methodDefs;
    std::vector<Il2CppParameterDefinition> parameterDefs;
    std::vector<Il2CppFieldDefinition> fieldDefs;
    std::unordered_map<int32_t, Il2CppFieldDefaultValue> fieldDefaultValuesDic;
    std::unordered_map<int32_t, Il2CppParameterDefaultValue> parameterDefaultValuesDic;
    std::vector<Il2CppPropertyDefinition> propertyDefs;
    std::vector<Il2CppCustomAttributeTypeRange> attributeTypeRanges;
    std::vector<Il2CppCustomAttributeDataRange> attributeDataRanges;
    std::unordered_map<int, std::unordered_map<uint32_t, int>> attributeTypeRangesDic;
    std::vector<Il2CppStringLiteral> stringLiterals;
    std::vector<Il2CppMetadataUsageList> metadataUsageLists;
    std::vector<Il2CppMetadataUsagePair> metadataUsagePairs;
    std::vector<int> attributeTypes;
    std::vector<int> interfaceIndices;
    std::array<std::map<uint32_t, uint32_t>, 7> metadataUsageDic;
    int64_t metadataUsagesCount = 0;
    std::vector<int> nestedTypeIndices;
    std::vector<Il2CppEventDefinition> eventDefs;
    std::vector<Il2CppGenericContainer> genericContainers;
    std::vector<Il2CppFieldRef> fieldRefs;
    std::vector<Il2CppGenericParameter> genericParameters;
    std::vector<int> constraintIndices;
    std::vector<uint32_t> vtableMethods;
    std::vector<Il2CppRGCTXDefinition> rgctxEntries;

    explicit Metadata(std::vector<uint8_t> data);

    bool getFieldDefaultValueFromIndex(int index, Il2CppFieldDefaultValue& value) const {
        auto it = fieldDefaultValuesDic.find(index);
        if (it == fieldDefaultValuesDic.end()) {
            return false;
        }
        value = it->second;
        return true;
    }

    bool getParameterDefaultValueFromIndex(int index, Il2CppParameterDefaultValue& value) const {
        auto it = parameterDefaultValuesDic.find(index);
        if (it == parameterDefaultValuesDic.end()) {
            return false;
        }
        value = it->second;
        return true;
    }

    uint32_t getDefaultValueFromIndex(int index) const {
        return header.fieldAndParameterDefaultValueDataOffset + static_cast<uint32_t>(index);
    }

    std::string getStringFromIndex(uint32_t index);

    int getCustomAttributeIndex(int imageIndex, int customAttributeIndex, uint32_t token) const;

    std::string getStringLiteralFromIndex(uint32_t index);

    static uint32_t getEncodedIndexType(uint32_t index) { return (index & 0xE0000000) >> 29; }

    uint32_t getDecodedMethodIndex(uint32_t index) const {
        if (version >= 27) {
            return (index & 0x1FFFFFFEu) >> 1;
        }
        return index & 0x1FFFFFFFu;
    }

private:
    template <typename T>
    std::vector<T> readMetadataClassArray(uint64_t addr, int64_t byteCount) {
        return readClassArrayAt<T>(addr, byteCount / static_cast<int64_t>(sizeOfClass<T>()));
    }
    void processingMetadataUsage();

    std::unordered_map<uint32_t, std::string> stringCache;
};

}
