#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <memory>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <cassert>
#include <fstream>
#include <iostream>
#include <numeric>

// ============================================================
// Config
// ============================================================
struct Il2CppDumperConfig {
    bool DumpMethod = true;
    bool DumpField = true;
    bool DumpProperty = false;
    bool DumpAttribute = false;
    bool DumpFieldOffset = true;
    bool DumpMethodOffset = true;
    bool DumpTypeDefIndex = true;
    bool GenerateStruct = true;
    bool ForceIl2CppVersion = false;
    double ForceVersion = 24.3;
    bool ForceDump = false;
    bool NoRedirectedPointer = false;
};

// ============================================================
// IL2CPP Constants
// ============================================================
namespace Il2CppConstants {
    // Field Attributes
    constexpr uint32_t FIELD_ATTRIBUTE_FIELD_ACCESS_MASK = 0x0007;
    constexpr uint32_t FIELD_ATTRIBUTE_PRIVATE = 0x0001;
    constexpr uint32_t FIELD_ATTRIBUTE_FAM_AND_ASSEM = 0x0002;
    constexpr uint32_t FIELD_ATTRIBUTE_ASSEMBLY = 0x0003;
    constexpr uint32_t FIELD_ATTRIBUTE_FAMILY = 0x0004;
    constexpr uint32_t FIELD_ATTRIBUTE_FAM_OR_ASSEM = 0x0005;
    constexpr uint32_t FIELD_ATTRIBUTE_PUBLIC = 0x0006;
    constexpr uint32_t FIELD_ATTRIBUTE_STATIC = 0x0010;
    constexpr uint32_t FIELD_ATTRIBUTE_INIT_ONLY = 0x0020;
    constexpr uint32_t FIELD_ATTRIBUTE_LITERAL = 0x0040;

    // Method Attributes
    constexpr uint32_t METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK = 0x0007;
    constexpr uint32_t METHOD_ATTRIBUTE_PRIVATE = 0x0001;
    constexpr uint32_t METHOD_ATTRIBUTE_FAM_AND_ASSEM = 0x0002;
    constexpr uint32_t METHOD_ATTRIBUTE_ASSEM = 0x0003;
    constexpr uint32_t METHOD_ATTRIBUTE_FAMILY = 0x0004;
    constexpr uint32_t METHOD_ATTRIBUTE_FAM_OR_ASSEM = 0x0005;
    constexpr uint32_t METHOD_ATTRIBUTE_PUBLIC = 0x0006;
    constexpr uint32_t METHOD_ATTRIBUTE_STATIC = 0x0010;
    constexpr uint32_t METHOD_ATTRIBUTE_FINAL = 0x0020;
    constexpr uint32_t METHOD_ATTRIBUTE_VIRTUAL = 0x0040;
    constexpr uint32_t METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK = 0x0100;
    constexpr uint32_t METHOD_ATTRIBUTE_REUSE_SLOT = 0x0000;
    constexpr uint32_t METHOD_ATTRIBUTE_NEW_SLOT = 0x0100;
    constexpr uint32_t METHOD_ATTRIBUTE_ABSTRACT = 0x0400;
    constexpr uint32_t METHOD_ATTRIBUTE_PINVOKE_IMPL = 0x2000;

    // Type Attributes
    constexpr uint32_t TYPE_ATTRIBUTE_VISIBILITY_MASK = 0x00000007;
    constexpr uint32_t TYPE_ATTRIBUTE_NOT_PUBLIC = 0x00000000;
    constexpr uint32_t TYPE_ATTRIBUTE_PUBLIC = 0x00000001;
    constexpr uint32_t TYPE_ATTRIBUTE_NESTED_PUBLIC = 0x00000002;
    constexpr uint32_t TYPE_ATTRIBUTE_NESTED_PRIVATE = 0x00000003;
    constexpr uint32_t TYPE_ATTRIBUTE_NESTED_FAMILY = 0x00000004;
    constexpr uint32_t TYPE_ATTRIBUTE_NESTED_ASSEMBLY = 0x00000005;
    constexpr uint32_t TYPE_ATTRIBUTE_NESTED_FAM_AND_ASSEM = 0x00000006;
    constexpr uint32_t TYPE_ATTRIBUTE_NESTED_FAM_OR_ASSEM = 0x00000007;
    constexpr uint32_t TYPE_ATTRIBUTE_INTERFACE = 0x00000020;
    constexpr uint32_t TYPE_ATTRIBUTE_ABSTRACT = 0x00000080;
    constexpr uint32_t TYPE_ATTRIBUTE_SEALED = 0x00000100;
    constexpr uint32_t TYPE_ATTRIBUTE_SERIALIZABLE = 0x00002000;

    // Param Attributes
    constexpr uint32_t PARAM_ATTRIBUTE_IN = 0x0001;
    constexpr uint32_t PARAM_ATTRIBUTE_OUT = 0x0002;
}

// ============================================================
// IL2CPP Type Enum
// ============================================================
enum class Il2CppTypeEnum : uint8_t {
    IL2CPP_TYPE_END = 0x00,
    IL2CPP_TYPE_VOID = 0x01,
    IL2CPP_TYPE_BOOLEAN = 0x02,
    IL2CPP_TYPE_CHAR = 0x03,
    IL2CPP_TYPE_I1 = 0x04,
    IL2CPP_TYPE_U1 = 0x05,
    IL2CPP_TYPE_I2 = 0x06,
    IL2CPP_TYPE_U2 = 0x07,
    IL2CPP_TYPE_I4 = 0x08,
    IL2CPP_TYPE_U4 = 0x09,
    IL2CPP_TYPE_I8 = 0x0a,
    IL2CPP_TYPE_U8 = 0x0b,
    IL2CPP_TYPE_R4 = 0x0c,
    IL2CPP_TYPE_R8 = 0x0d,
    IL2CPP_TYPE_STRING = 0x0e,
    IL2CPP_TYPE_PTR = 0x0f,
    IL2CPP_TYPE_BYREF = 0x10,
    IL2CPP_TYPE_VALUETYPE = 0x11,
    IL2CPP_TYPE_CLASS = 0x12,
    IL2CPP_TYPE_VAR = 0x13,
    IL2CPP_TYPE_ARRAY = 0x14,
    IL2CPP_TYPE_GENERICINST = 0x15,
    IL2CPP_TYPE_TYPEDBYREF = 0x16,
    IL2CPP_TYPE_I = 0x18,
    IL2CPP_TYPE_U = 0x19,
    IL2CPP_TYPE_FNPTR = 0x1b,
    IL2CPP_TYPE_OBJECT = 0x1c,
    IL2CPP_TYPE_SZARRAY = 0x1d,
    IL2CPP_TYPE_MVAR = 0x1e,
    IL2CPP_TYPE_CMOD_REQD = 0x1f,
    IL2CPP_TYPE_CMOD_OPT = 0x20,
    IL2CPP_TYPE_INTERNAL = 0x21,
    IL2CPP_TYPE_MODIFIER = 0x40,
    IL2CPP_TYPE_SENTINEL = 0x41,
    IL2CPP_TYPE_PINNED = 0x45,
    IL2CPP_TYPE_ENUM = 0x55,
    IL2CPP_TYPE_IL2CPP_TYPE_INDEX = 0xff
};

// ============================================================
// IL2CPP Metadata Usage Enum
// ============================================================
enum class Il2CppMetadataUsage : uint32_t {
    kIl2CppMetadataUsageInvalid = 0,
    kIl2CppMetadataUsageTypeInfo = 1,
    kIl2CppMetadataUsageIl2CppType = 2,
    kIl2CppMetadataUsageMethodDef = 3,
    kIl2CppMetadataUsageFieldInfo = 4,
    kIl2CppMetadataUsageStringLiteral = 5,
    kIl2CppMetadataUsageMethodRef = 6,
};

// ============================================================
// IL2CPP RGCTX Data Type
// ============================================================
enum class Il2CppRGCTXDataType : int32_t {
    IL2CPP_RGCTX_DATA_INVALID = 0,
    IL2CPP_RGCTX_DATA_TYPE,
    IL2CPP_RGCTX_DATA_CLASS,
    IL2CPP_RGCTX_DATA_METHOD,
    IL2CPP_RGCTX_DATA_ARRAY,
    IL2CPP_RGCTX_DATA_CONSTRAINED,
};

// ============================================================
// Forward declarations
// ============================================================
class BinaryStream;
class Metadata;
class Il2Cpp;

// ============================================================
// Il2CppType - runtime type representation
// ============================================================
struct Il2CppType {
    uint64_t datapoint = 0;
    uint32_t bits = 0;

    // Computed fields (from Init)
    uint64_t data_dummy = 0;
    uint32_t attrs = 0;
    Il2CppTypeEnum type = Il2CppTypeEnum::IL2CPP_TYPE_END;
    uint32_t num_mods = 0;
    uint32_t byref = 0;
    uint32_t pinned = 0;
    uint32_t valuetype = 0;

    void Init(double version) {
        attrs = bits & 0xffff;
        type = (Il2CppTypeEnum)((bits >> 16) & 0xff);
        if (version >= 27.2) {
            num_mods = (bits >> 24) & 0x1f;
            byref = (bits >> 29) & 1;
            pinned = (bits >> 30) & 1;
            valuetype = bits >> 31;
        } else {
            num_mods = (bits >> 24) & 0x3f;
            byref = (bits >> 30) & 1;
            pinned = bits >> 31;
        }
        data_dummy = datapoint;
    }

    // Union accessors (matching C# Union class)
    int64_t klassIndex() const { return (int64_t)data_dummy; }
    uint64_t typeHandle() const { return data_dummy; }
    uint64_t type_ptr() const { return data_dummy; } // "type" in C# Union
    uint64_t array() const { return data_dummy; }
    int64_t genericParameterIndex() const { return (int64_t)data_dummy; }
    uint64_t genericParameterHandle() const { return data_dummy; }
    uint64_t generic_class() const { return data_dummy; }
};

// ============================================================
// Il2CppCodeRegistration
// ============================================================
struct Il2CppCodeRegistration {
    // Version-dependent fields - read manually
    uint64_t methodPointersCount = 0;   // Max = 24.1
    uint64_t methodPointers = 0;         // Max = 24.1
    uint64_t delegateWrappersFromNativeToManagedCount = 0; // Max = 21
    uint64_t delegateWrappersFromNativeToManaged = 0; // Max = 21
    uint64_t reversePInvokeWrapperCount = 0; // Min = 22
    uint64_t reversePInvokeWrappers = 0;     // Min = 22
    uint64_t delegateWrappersFromManagedToNativeCount = 0; // Max = 22
    uint64_t delegateWrappersFromManagedToNative = 0; // Max = 22
    uint64_t marshalingFunctionsCount = 0; // Max = 22
    uint64_t marshalingFunctions = 0; // Max = 22
    uint64_t ccwMarshalingFunctionsCount = 0; // Min = 21, Max = 22
    uint64_t ccwMarshalingFunctions = 0; // Min = 21, Max = 22
    uint64_t genericMethodPointersCount = 0;
    uint64_t genericMethodPointers = 0;
    uint64_t genericAdjustorThunks = 0; // [Min = 24.5, Max = 24.5] OR [Min = 27.1]
    uint64_t invokerPointersCount = 0;
    uint64_t invokerPointers = 0;
    uint64_t customAttributeCount = 0; // Max = 24.5
    uint64_t customAttributeGenerators = 0; // Max = 24.5
    uint64_t guidCount = 0; // Min = 21, Max = 22
    uint64_t guids = 0; // Min = 21, Max = 22
    uint64_t unresolvedVirtualCallCount = 0; // Min = 22
    uint64_t unresolvedVirtualCallPointers = 0; // Min = 22
    uint64_t unresolvedInstanceCallPointers = 0; // Min = 29.1
    uint64_t unresolvedStaticCallPointers = 0; // Min = 29.1
    uint64_t interopDataCount = 0; // Min = 23
    uint64_t interopData = 0; // Min = 23
    uint64_t windowsRuntimeFactoryCount = 0; // Min = 24.3
    uint64_t windowsRuntimeFactoryTable = 0; // Min = 24.3
    uint64_t codeGenModulesCount = 0; // Min = 24.2
    uint64_t codeGenModules = 0; // Min = 24.2
};

// ============================================================
// Il2CppMetadataRegistration
// ============================================================
struct Il2CppMetadataRegistration {
    int64_t genericClassesCount = 0;
    uint64_t genericClasses = 0;
    int64_t genericInstsCount = 0;
    uint64_t genericInsts = 0;
    int64_t genericMethodTableCount = 0;
    uint64_t genericMethodTable = 0;
    int64_t typesCount = 0;
    uint64_t types = 0;
    int64_t methodSpecsCount = 0;
    uint64_t methodSpecs = 0;
    int64_t methodReferencesCount = 0; // Max = 16
    uint64_t methodReferences = 0; // Max = 16
    int64_t fieldOffsetsCount = 0;
    uint64_t fieldOffsets = 0;
    int64_t typeDefinitionsSizesCount = 0;
    uint64_t typeDefinitionsSizes = 0;
    uint64_t metadataUsagesCount = 0; // Min = 19
    uint64_t metadataUsages = 0; // Min = 19
};

// ============================================================
// Il2CppGenericClass
// ============================================================
struct Il2CppGenericClass {
    int64_t typeDefinitionIndex = 0; // Max = 24.5
    uint64_t type = 0; // Min = 27
    uint64_t class_inst = 0; // context
    uint64_t method_inst = 0; // context
    uint64_t cached_class = 0;
};

// ============================================================
// Il2CppGenericInst
// ============================================================
struct Il2CppGenericInst {
    int64_t type_argc = 0;
    uint64_t type_argv = 0;
};

// ============================================================
// Il2CppArrayType
// ============================================================
struct Il2CppArrayType {
    uint64_t etype = 0;
    uint8_t rank = 0;
    uint8_t numsizes = 0;
    uint8_t numlobounds = 0;
    uint64_t sizes = 0;
    uint64_t lobounds = 0;
};

// ============================================================
// Il2CppGenericMethodFunctionsDefinitions
// ============================================================
struct Il2CppGenericMethodIndices {
    int32_t methodIndex = 0;
    int32_t invokerIndex = 0;
    int32_t adjustorThunk = 0; // [Min = 24.5, Max = 24.5] OR [Min = 27.1]
};

struct Il2CppGenericMethodFunctionsDefinitions {
    int32_t genericMethodIndex = 0;
    Il2CppGenericMethodIndices indices;
};

// ============================================================
// Il2CppMethodSpec
// ============================================================
struct Il2CppMethodSpec {
    int32_t methodDefinitionIndex = 0;
    int32_t classIndexIndex = 0;
    int32_t methodIndexIndex = 0;
};

// ============================================================
// Il2CppCodeGenModule
// ============================================================
struct Il2CppCodeGenModule {
    uint64_t moduleName = 0;
    int64_t methodPointerCount = 0;
    uint64_t methodPointers = 0;
    int64_t adjustorThunkCount = 0; // [Min = 24.5, Max = 24.5] OR [Min = 27.1]
    uint64_t adjustorThunks = 0;
    uint64_t invokerIndices = 0;
    uint64_t reversePInvokeWrapperCount = 0;
    uint64_t reversePInvokeWrapperIndices = 0;
    int64_t rgctxRangesCount = 0;
    uint64_t rgctxRanges = 0;
    int64_t rgctxsCount = 0;
    uint64_t rgctxs = 0;
    uint64_t debuggerMetadata = 0;
    uint64_t customAttributeCacheGenerator = 0; // Min = 27, Max = 27.2
    uint64_t moduleInitializer = 0; // Min = 27
    uint64_t staticConstructorTypeIndices = 0; // Min = 27
    uint64_t metadataRegistration = 0; // Min = 27
    uint64_t codeRegistaration = 0; // Min = 27
};

// ============================================================
// Il2CppRange / Il2CppTokenRangePair
// ============================================================
struct Il2CppRange {
    int32_t start = 0;
    int32_t length = 0;
};

struct Il2CppTokenRangePair {
    uint32_t token = 0;
    Il2CppRange range;
};

// ============================================================
// Metadata structures
// ============================================================
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
    uint32_t rgctxEntriesOffset = 0; // Max = 24.1
    int32_t rgctxEntriesCount = 0; // Max = 24.1
    uint32_t imagesOffset = 0;
    int32_t imagesSize = 0;
    uint32_t assembliesOffset = 0;
    int32_t assembliesSize = 0;
    uint32_t metadataUsageListsOffset = 0; // Min = 19, Max = 24.5
    int32_t metadataUsageListsCount = 0;
    uint32_t metadataUsagePairsOffset = 0; // Min = 19, Max = 24.5
    int32_t metadataUsagePairsCount = 0;
    uint32_t fieldRefsOffset = 0; // Min = 19
    int32_t fieldRefsSize = 0;
    int32_t referencedAssembliesOffset = 0; // Min = 20
    int32_t referencedAssembliesSize = 0;
    uint32_t attributesInfoOffset = 0; // Min = 21, Max = 27.2
    int32_t attributesInfoCount = 0;
    uint32_t attributeTypesOffset = 0; // Min = 21, Max = 27.2
    int32_t attributeTypesCount = 0;
    uint32_t attributeDataOffset = 0; // Min = 29
    int32_t attributeDataSize = 0;
    uint32_t attributeDataRangeOffset = 0; // Min = 29
    int32_t attributeDataRangeSize = 0;
    int32_t unresolvedVirtualCallParameterTypesOffset = 0; // Min = 22
    int32_t unresolvedVirtualCallParameterTypesSize = 0;
    int32_t unresolvedVirtualCallParameterRangesOffset = 0; // Min = 22
    int32_t unresolvedVirtualCallParameterRangesSize = 0;
    int32_t windowsRuntimeTypeNamesOffset = 0; // Min = 23
    int32_t windowsRuntimeTypeNamesSize = 0;
    int32_t windowsRuntimeStringsOffset = 0; // Min = 27
    int32_t windowsRuntimeStringsSize = 0;
    int32_t exportedTypeDefinitionsOffset = 0; // Min = 24
    int32_t exportedTypeDefinitionsSize = 0;
};

struct Il2CppAssemblyNameDefinition {
    uint32_t nameIndex = 0;
    uint32_t cultureIndex = 0;
    int32_t hashValueIndex = 0; // Max = 24.3
    uint32_t publicKeyIndex = 0;
    uint32_t hash_alg = 0;
    int32_t hash_len = 0;
    uint32_t flags = 0;
    int32_t major = 0;
    int32_t minor = 0;
    int32_t build = 0;
    int32_t revision = 0;
    uint8_t public_key_token[8] = {};
};

struct Il2CppAssemblyDefinition {
    int32_t imageIndex = 0;
    uint32_t token = 0; // Min = 24.1
    int32_t customAttributeIndex = 0; // Max = 24
    int32_t referencedAssemblyStart = 0; // Min = 20
    int32_t referencedAssemblyCount = 0; // Min = 20
    Il2CppAssemblyNameDefinition aname;
};

struct Il2CppImageDefinition {
    uint32_t nameIndex = 0;
    int32_t assemblyIndex = 0;
    int32_t typeStart = 0;
    uint32_t typeCount = 0;
    int32_t exportedTypeStart = 0; // Min = 24
    uint32_t exportedTypeCount = 0; // Min = 24
    int32_t entryPointIndex = 0;
    uint32_t token = 0; // Min = 19
    int32_t customAttributeStart = 0; // Min = 24.1
    uint32_t customAttributeCount = 0; // Min = 24.1
};

struct Il2CppTypeDefinition {
    uint32_t nameIndex = 0;
    uint32_t namespaceIndex = 0;
    int32_t customAttributeIndex = 0; // Max = 24
    int32_t byvalTypeIndex = 0;
    int32_t byrefTypeIndex = 0; // Max = 24.5
    int32_t declaringTypeIndex = 0;
    int32_t parentIndex = 0;
    int32_t elementTypeIndex = 0;
    int32_t rgctxStartIndex = 0; // Max = 24.1
    int32_t rgctxCount = 0; // Max = 24.1
    int32_t genericContainerIndex = 0;
    int32_t delegateWrapperFromManagedToNativeIndex = 0; // Max = 22
    int32_t marshalingFunctionsIndex = 0; // Max = 22
    int32_t ccwFunctionIndex = 0; // Min = 21, Max = 22
    int32_t guidIndex = 0; // Min = 21, Max = 22
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
    uint32_t token = 0; // Min = 19

    bool IsValueType() const { return (bitfield & 0x1) == 1; }
    bool IsEnum() const { return ((bitfield >> 1) & 0x1) == 1; }
};

struct Il2CppMethodDefinition {
    uint32_t nameIndex = 0;
    int32_t declaringType = 0;
    int32_t returnType = 0;
    int32_t returnParameterToken = 0; // Min = 31
    int32_t parameterStart = 0;
    int32_t customAttributeIndex = 0; // Max = 24
    int32_t genericContainerIndex = 0;
    int32_t methodIndex = 0; // Max = 24.1
    int32_t invokerIndex = 0; // Max = 24.1
    int32_t delegateWrapperIndex = 0; // Max = 24.1
    int32_t rgctxStartIndex = 0; // Max = 24.1
    int32_t rgctxCount = 0; // Max = 24.1
    uint32_t token = 0;
    uint16_t flags = 0;
    uint16_t iflags = 0;
    uint16_t slot = 0;
    uint16_t parameterCount = 0;
};

struct Il2CppParameterDefinition {
    uint32_t nameIndex = 0;
    uint32_t token = 0;
    int32_t customAttributeIndex = 0; // Max = 24
    int32_t typeIndex = 0;
};

struct Il2CppFieldDefinition {
    uint32_t nameIndex = 0;
    int32_t typeIndex = 0;
    int32_t customAttributeIndex = 0; // Max = 24
    uint32_t token = 0; // Min = 19
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
    int32_t customAttributeIndex = 0; // Max = 24
    uint32_t token = 0; // Min = 19
};

struct Il2CppCustomAttributeTypeRange {
    uint32_t token = 0; // Min = 24.1
    int32_t start = 0;
    int32_t count = 0;
};

struct Il2CppCustomAttributeDataRange {
    uint32_t token = 0;
    uint32_t startOffset = 0;
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
    int32_t customAttributeIndex = 0; // Max = 24
    uint32_t token = 0; // Min = 19
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

struct Il2CppRGCTXDefinition {
    int32_t type_pre29 = 0; // Max = 27.1
    uint64_t type_post29 = 0; // Min = 29
    int32_t rgctxDataDummy = 0; // Max = 27.1
    uint64_t _data = 0; // Min = 27.2

    Il2CppRGCTXDataType getType() const {
        return type_post29 == 0 ? (Il2CppRGCTXDataType)type_pre29 : (Il2CppRGCTXDataType)type_post29;
    }
};

// ============================================================
// Search section types
// ============================================================
enum class SearchSectionType {
    Exec,
    Data,
    Bss
};

struct SearchSection {
    uint64_t offset = 0;
    uint64_t offsetEnd = 0;
    uint64_t address = 0;
    uint64_t addressEnd = 0;
};

// ============================================================
// ELF structures
// ============================================================
struct Elf32_Ehdr {
    uint32_t ei_mag = 0;
    uint8_t ei_class = 0;
    uint8_t ei_data = 0;
    uint8_t ei_version = 0;
    uint8_t ei_osabi = 0;
    uint8_t ei_abiversion = 0;
    uint8_t ei_pad[7] = {};
    uint16_t e_type = 0;
    uint16_t e_machine = 0;
    uint32_t e_version = 0;
    uint32_t e_entry = 0;
    uint32_t e_phoff = 0;
    uint32_t e_shoff = 0;
    uint32_t e_flags = 0;
    uint16_t e_ehsize = 0;
    uint16_t e_phentsize = 0;
    uint16_t e_phnum = 0;
    uint16_t e_shentsize = 0;
    uint16_t e_shnum = 0;
    uint16_t e_shstrndx = 0;
};

struct Elf32_Phdr {
    uint32_t p_type = 0;
    uint32_t p_offset = 0;
    uint32_t p_vaddr = 0;
    uint32_t p_paddr = 0;
    uint32_t p_filesz = 0;
    uint32_t p_memsz = 0;
    uint32_t p_flags = 0;
    uint32_t p_align = 0;
};

struct Elf32_Dyn {
    int32_t d_tag = 0;
    uint32_t d_un = 0;
};

struct Elf32_Sym {
    uint32_t st_name = 0;
    uint32_t st_value = 0;
    uint32_t st_size = 0;
    uint8_t st_info = 0;
    uint8_t st_other = 0;
    uint16_t st_shndx = 0;
};

struct Elf32_Shdr {
    uint32_t sh_name = 0;
    uint32_t sh_type = 0;
    uint32_t sh_flags = 0;
    uint32_t sh_addr = 0;
    uint32_t sh_offset = 0;
    uint32_t sh_size = 0;
    uint32_t sh_link = 0;
    uint32_t sh_info = 0;
    uint32_t sh_addralign = 0;
    uint32_t sh_entsize = 0;
};

struct Elf32_Rel {
    uint32_t r_offset = 0;
    uint32_t r_info = 0;
};

struct Elf64_Ehdr {
    uint32_t ei_mag = 0;
    uint8_t ei_class = 0;
    uint8_t ei_data = 0;
    uint8_t ei_version = 0;
    uint8_t ei_osabi = 0;
    uint8_t ei_abiversion = 0;
    uint8_t ei_pad[7] = {};
    uint16_t e_type = 0;
    uint16_t e_machine = 0;
    uint32_t e_version = 0;
    uint64_t e_entry = 0;
    uint64_t e_phoff = 0;
    uint64_t e_shoff = 0;
    uint32_t e_flags = 0;
    uint16_t e_ehsize = 0;
    uint16_t e_phentsize = 0;
    uint16_t e_phnum = 0;
    uint16_t e_shentsize = 0;
    uint16_t e_shnum = 0;
    uint16_t e_shstrndx = 0;
};

struct Elf64_Phdr {
    uint32_t p_type = 0;
    uint32_t p_flags = 0;
    uint64_t p_offset = 0;
    uint64_t p_vaddr = 0;
    uint64_t p_paddr = 0;
    uint64_t p_filesz = 0;
    uint64_t p_memsz = 0;
    uint64_t p_align = 0;
};

struct Elf64_Dyn {
    int64_t d_tag = 0;
    uint64_t d_un = 0;
};

struct Elf64_Sym {
    uint32_t st_name = 0;
    uint8_t st_info = 0;
    uint8_t st_other = 0;
    uint16_t st_shndx = 0;
    uint64_t st_value = 0;
    uint64_t st_size = 0;
};

struct Elf64_Shdr {
    uint32_t sh_name = 0;
    uint32_t sh_type = 0;
    uint64_t sh_flags = 0;
    uint64_t sh_addr = 0;
    uint64_t sh_offset = 0;
    uint64_t sh_size = 0;
    uint32_t sh_link = 0;
    uint32_t sh_info = 0;
    uint64_t sh_addralign = 0;
    uint64_t sh_entsize = 0;
};

struct Elf64_Rela {
    uint64_t r_offset = 0;
    uint64_t r_info = 0;
    uint64_t r_addend = 0;
};

namespace ElfConstants {
    constexpr int EM_386 = 3;
    constexpr int EM_ARM = 40;
    constexpr int EM_X86_64 = 62;
    constexpr int EM_AARCH64 = 183;
    constexpr int PT_LOAD = 1;
    constexpr int PT_DYNAMIC = 2;
    constexpr int PF_X = 1;
    constexpr int DT_PLTGOT = 3;
    constexpr int DT_HASH = 4;
    constexpr int DT_STRTAB = 5;
    constexpr int DT_SYMTAB = 6;
    constexpr int DT_RELA = 7;
    constexpr int DT_RELASZ = 8;
    constexpr int DT_INIT = 12;
    constexpr int DT_FINI = 13;
    constexpr int DT_REL = 17;
    constexpr int DT_RELSZ = 18;
    constexpr int DT_JMPREL = 23;
    constexpr int DT_INIT_ARRAY = 25;
    constexpr int DT_FINI_ARRAY = 26;
    constexpr int DT_GNU_HASH = 0x6ffffef5;
    constexpr uint32_t SHT_LOUSER = 0x80000000;
    constexpr int R_ARM_ABS32 = 2;
    constexpr int R_386_32 = 1;
    constexpr int R_AARCH64_ABS64 = 257;
    constexpr int R_AARCH64_RELATIVE = 1027;
    constexpr int R_X86_64_64 = 1;
    constexpr int R_X86_64_RELATIVE = 8;
}

// ============================================================
// Mach-O structures
// ============================================================
struct MachoSection {
    std::string sectname;
    uint32_t addr = 0;
    uint32_t size = 0;
    uint32_t offset = 0;
    uint32_t flags = 0;
};

struct MachoSection64Bit {
    std::string sectname;
    uint64_t addr = 0;
    uint64_t size = 0;
    uint64_t offset = 0;
    uint32_t flags = 0;
};

struct Fat {
    uint32_t offset = 0;
    uint32_t size = 0;
    uint32_t magic = 0;
};

// ============================================================
// PE structures
// ============================================================
struct DosHeader {
    uint16_t Magic = 0;
    uint16_t Cblp = 0; uint16_t Cp = 0; uint16_t Crlc = 0;
    uint16_t Cparhdr = 0; uint16_t Minalloc = 0; uint16_t Maxalloc = 0;
    uint16_t Ss = 0; uint16_t Sp = 0; uint16_t Csum = 0;
    uint16_t Ip = 0; uint16_t Cs = 0; uint16_t Lfarlc = 0; uint16_t Ovno = 0;
    uint16_t Res[4] = {};
    uint16_t Oemid = 0; uint16_t Oeminfo = 0;
    uint16_t Res2[10] = {};
    uint32_t Lfanew = 0;
};

struct PEFileHeader {
    uint16_t Machine = 0;
    uint16_t NumberOfSections = 0;
    uint32_t TimeDateStamp = 0;
    uint32_t PointerToSymbolTable = 0;
    uint32_t NumberOfSymbols = 0;
    uint16_t SizeOfOptionalHeader = 0;
    uint16_t Characteristics = 0;
};

struct PESectionHeader {
    uint8_t Name[8] = {};
    uint32_t VirtualSize = 0;
    uint32_t VirtualAddress = 0;
    uint32_t SizeOfRawData = 0;
    uint32_t PointerToRawData = 0;
    uint32_t PointerToRelocations = 0;
    uint32_t PointerToLinenumbers = 0;
    uint16_t NumberOfRelocations = 0;
    uint16_t NumberOfLinenumbers = 0;
    uint32_t Characteristics = 0;
};

// ============================================================
// Logging callback
// ============================================================
using LogCallback = std::function<void(const std::string&)>;
