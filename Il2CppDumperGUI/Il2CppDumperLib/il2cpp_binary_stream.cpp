#include "il2cpp_binary_stream.h"

BinaryStream::BinaryStream(const std::vector<uint8_t>& data)
    : data_(data), position_(0) {}

BinaryStream::BinaryStream(const uint8_t* data, size_t size)
    : data_(data, data + size), position_(0) {}

void BinaryStream::EnsureBytes(size_t count) const {
    if (position_ + count > data_.size()) {
        throw std::runtime_error("Read past end of stream");
    }
}

// Primitives
bool BinaryStream::ReadBoolean() { return ReadRaw<uint8_t>() != 0; }
uint8_t BinaryStream::ReadByte() { return ReadRaw<uint8_t>(); }
int8_t BinaryStream::ReadSByte() { return ReadRaw<int8_t>(); }

std::vector<uint8_t> BinaryStream::ReadBytes(int count) {
    EnsureBytes(count);
    std::vector<uint8_t> result(data_.begin() + position_, data_.begin() + position_ + count);
    position_ += count;
    return result;
}

int16_t BinaryStream::ReadInt16() { return ReadRaw<int16_t>(); }
uint16_t BinaryStream::ReadUInt16() { return ReadRaw<uint16_t>(); }
int32_t BinaryStream::ReadInt32() { return ReadRaw<int32_t>(); }
uint32_t BinaryStream::ReadUInt32() { return ReadRaw<uint32_t>(); }
int64_t BinaryStream::ReadInt64() { return ReadRaw<int64_t>(); }
uint64_t BinaryStream::ReadUInt64() { return ReadRaw<uint64_t>(); }
float BinaryStream::ReadSingle() { return ReadRaw<float>(); }
double BinaryStream::ReadDouble() { return ReadRaw<double>(); }

int64_t BinaryStream::ReadIntPtr() {
    return Is32Bit ? (int64_t)ReadInt32() : ReadInt64();
}

uint64_t BinaryStream::ReadUIntPtr() {
    return Is32Bit ? (uint64_t)ReadUInt32() : ReadUInt64();
}

std::string BinaryStream::ReadStringToNull(uint64_t addr) {
    SetPosition(addr);
    std::string result;
    uint8_t b;
    while (position_ < data_.size() && (b = ReadByte()) != 0) {
        result += (char)b;
    }
    return result;
}

// Write
void BinaryStream::WriteUInt32(uint32_t value) {
    EnsureBytes(4);
    memcpy(data_.data() + position_, &value, 4);
    position_ += 4;
}

void BinaryStream::WriteUInt64(uint64_t value) {
    EnsureBytes(8);
    memcpy(data_.data() + position_, &value, 8);
    position_ += 8;
}

void BinaryStream::WriteUIntPtr(uint64_t value) {
    if (Is32Bit) {
        uint32_t v = (uint32_t)value;
        WriteUInt32(v);
    } else {
        WriteUInt64(value);
    }
}

// Compressed reads
uint32_t BinaryStream::ReadCompressedUInt32() {
    uint32_t val;
    auto read = ReadByte();
    if ((read & 0x80) == 0) {
        val = read;
    } else if ((read & 0xC0) == 0x80) {
        val = (read & ~0x80u) << 8;
        val |= ReadByte();
    } else if ((read & 0xE0) == 0xC0) {
        val = (read & ~0xC0u) << 24;
        val |= ((uint32_t)ReadByte() << 16);
        val |= ((uint32_t)ReadByte() << 8);
        val |= ReadByte();
    } else if (read == 0xF0) {
        val = ReadUInt32();
    } else if (read == 0xFE) {
        val = UINT32_MAX - 1;
    } else if (read == 0xFF) {
        val = UINT32_MAX;
    } else {
        throw std::runtime_error("Invalid compressed integer format");
    }
    return val;
}

int32_t BinaryStream::ReadCompressedInt32() {
    auto encoded = ReadCompressedUInt32();
    if (encoded == UINT32_MAX)
        return INT32_MIN;
    bool isNegative = (encoded & 1) != 0;
    encoded >>= 1;
    if (isNegative)
        return -(int32_t)(encoded + 1);
    return (int32_t)encoded;
}

uint32_t BinaryStream::ReadULeb128() {
    uint32_t value = ReadByte();
    if (value >= 0x80) {
        int bitshift = 0;
        value &= 0x7f;
        while (true) {
            auto b = ReadByte();
            bitshift += 7;
            value |= (uint32_t)((b & 0x7f) << bitshift);
            if (b < 0x80) break;
        }
    }
    return value;
}

// Array reads
std::vector<uint64_t> BinaryStream::ReadUInt64Array(uint64_t addr, int64_t count) {
    SetPosition(addr);
    std::vector<uint64_t> result;
    result.reserve((size_t)count);
    for (int64_t i = 0; i < count; i++) {
        result.push_back(ReadUIntPtr());
    }
    return result;
}

std::vector<uint32_t> BinaryStream::ReadUInt32Array(uint64_t addr, int64_t count) {
    SetPosition(addr);
    std::vector<uint32_t> result;
    result.reserve((size_t)count);
    for (int64_t i = 0; i < count; i++) {
        result.push_back(ReadUInt32());
    }
    return result;
}

std::vector<int32_t> BinaryStream::ReadInt32Array(uint64_t addr, int64_t count) {
    SetPosition(addr);
    std::vector<int32_t> result;
    result.reserve((size_t)count);
    for (int64_t i = 0; i < count; i++) {
        result.push_back(ReadInt32());
    }
    return result;
}

// ============================================================
// Version-aware struct readers
// ============================================================

// Each read function checks Version to include/exclude fields,
// matching the C# [Version(Min=X, Max=Y)] attributes exactly.

#define V_IN(min, max) (Version >= (min) && Version <= (max))
#define V_MIN(min) (Version >= (min))
#define V_MAX(max) (Version <= (max))

Il2CppCodeRegistration BinaryStream::ReadCodeRegistration() {
    Il2CppCodeRegistration r;
    if (V_MAX(24.1)) { r.methodPointersCount = ReadUIntPtr(); }
    if (V_MAX(24.1)) { r.methodPointers = ReadUIntPtr(); }
    if (V_MAX(21)) { r.delegateWrappersFromNativeToManagedCount = ReadUIntPtr(); }
    if (V_MAX(21)) { r.delegateWrappersFromNativeToManaged = ReadUIntPtr(); }
    if (V_MIN(22)) { r.reversePInvokeWrapperCount = ReadUIntPtr(); }
    if (V_MIN(22)) { r.reversePInvokeWrappers = ReadUIntPtr(); }
    if (V_MAX(22)) { r.delegateWrappersFromManagedToNativeCount = ReadUIntPtr(); }
    if (V_MAX(22)) { r.delegateWrappersFromManagedToNative = ReadUIntPtr(); }
    if (V_MAX(22)) { r.marshalingFunctionsCount = ReadUIntPtr(); }
    if (V_MAX(22)) { r.marshalingFunctions = ReadUIntPtr(); }
    if (V_IN(21, 22)) { r.ccwMarshalingFunctionsCount = ReadUIntPtr(); }
    if (V_IN(21, 22)) { r.ccwMarshalingFunctions = ReadUIntPtr(); }
    r.genericMethodPointersCount = ReadUIntPtr();
    r.genericMethodPointers = ReadUIntPtr();
    if (V_IN(24.5, 24.5) || V_MIN(27.1)) { r.genericAdjustorThunks = ReadUIntPtr(); }
    r.invokerPointersCount = ReadUIntPtr();
    r.invokerPointers = ReadUIntPtr();
    if (V_MAX(24.5)) { r.customAttributeCount = ReadUIntPtr(); }
    if (V_MAX(24.5)) { r.customAttributeGenerators = ReadUIntPtr(); }
    if (V_IN(21, 22)) { r.guidCount = ReadUIntPtr(); }
    if (V_IN(21, 22)) { r.guids = ReadUIntPtr(); }
    if (V_MIN(22)) { r.unresolvedVirtualCallCount = ReadUIntPtr(); }
    if (V_MIN(22)) { r.unresolvedVirtualCallPointers = ReadUIntPtr(); }
    if (V_MIN(29.1)) { r.unresolvedInstanceCallPointers = ReadUIntPtr(); }
    if (V_MIN(29.1)) { r.unresolvedStaticCallPointers = ReadUIntPtr(); }
    if (V_MIN(23)) { r.interopDataCount = ReadUIntPtr(); }
    if (V_MIN(23)) { r.interopData = ReadUIntPtr(); }
    if (V_MIN(24.3)) { r.windowsRuntimeFactoryCount = ReadUIntPtr(); }
    if (V_MIN(24.3)) { r.windowsRuntimeFactoryTable = ReadUIntPtr(); }
    if (V_MIN(24.2)) { r.codeGenModulesCount = ReadUIntPtr(); }
    if (V_MIN(24.2)) { r.codeGenModules = ReadUIntPtr(); }
    return r;
}

Il2CppMetadataRegistration BinaryStream::ReadMetadataRegistration() {
    Il2CppMetadataRegistration r;
    r.genericClassesCount = ReadIntPtr();
    r.genericClasses = ReadUIntPtr();
    r.genericInstsCount = ReadIntPtr();
    r.genericInsts = ReadUIntPtr();
    r.genericMethodTableCount = ReadIntPtr();
    r.genericMethodTable = ReadUIntPtr();
    r.typesCount = ReadIntPtr();
    r.types = ReadUIntPtr();
    r.methodSpecsCount = ReadIntPtr();
    r.methodSpecs = ReadUIntPtr();
    if (V_MAX(16)) { r.methodReferencesCount = ReadIntPtr(); }
    if (V_MAX(16)) { r.methodReferences = ReadUIntPtr(); }
    r.fieldOffsetsCount = ReadIntPtr();
    r.fieldOffsets = ReadUIntPtr();
    r.typeDefinitionsSizesCount = ReadIntPtr();
    r.typeDefinitionsSizes = ReadUIntPtr();
    if (V_MIN(19)) { r.metadataUsagesCount = ReadUIntPtr(); }
    if (V_MIN(19)) { r.metadataUsages = ReadUIntPtr(); }
    return r;
}

Il2CppGenericClass BinaryStream::ReadGenericClass() {
    Il2CppGenericClass r;
    if (V_MAX(24.5)) { r.typeDefinitionIndex = ReadIntPtr(); }
    if (V_MIN(27)) { r.type = ReadUIntPtr(); }
    r.class_inst = ReadUIntPtr();
    r.method_inst = ReadUIntPtr();
    r.cached_class = ReadUIntPtr();
    return r;
}

Il2CppGenericInst BinaryStream::ReadGenericInst() {
    Il2CppGenericInst r;
    r.type_argc = ReadIntPtr();
    r.type_argv = ReadUIntPtr();
    return r;
}

Il2CppArrayType BinaryStream::ReadArrayType() {
    Il2CppArrayType r;
    r.etype = ReadUIntPtr();
    r.rank = ReadByte();
    r.numsizes = ReadByte();
    r.numlobounds = ReadByte();
    r.sizes = ReadUIntPtr();
    r.lobounds = ReadUIntPtr();
    return r;
}

Il2CppGenericMethodIndices BinaryStream::ReadGenericMethodIndices() {
    Il2CppGenericMethodIndices r;
    r.methodIndex = ReadInt32();
    r.invokerIndex = ReadInt32();
    if (V_IN(24.5, 24.5) || V_MIN(27.1)) { r.adjustorThunk = ReadInt32(); }
    return r;
}

Il2CppGenericMethodFunctionsDefinitions BinaryStream::ReadGenericMethodFunctionsDefinitions() {
    Il2CppGenericMethodFunctionsDefinitions r;
    r.genericMethodIndex = ReadInt32();
    r.indices = ReadGenericMethodIndices();
    return r;
}

Il2CppMethodSpec BinaryStream::ReadMethodSpec() {
    Il2CppMethodSpec r;
    r.methodDefinitionIndex = ReadInt32();
    r.classIndexIndex = ReadInt32();
    r.methodIndexIndex = ReadInt32();
    return r;
}

Il2CppCodeGenModule BinaryStream::ReadCodeGenModule() {
    Il2CppCodeGenModule r;
    r.moduleName = ReadUIntPtr();
    r.methodPointerCount = ReadIntPtr();
    r.methodPointers = ReadUIntPtr();
    if (V_IN(24.5, 24.5) || V_MIN(27.1)) { r.adjustorThunkCount = ReadIntPtr(); }
    if (V_IN(24.5, 24.5) || V_MIN(27.1)) { r.adjustorThunks = ReadUIntPtr(); }
    r.invokerIndices = ReadUIntPtr();
    r.reversePInvokeWrapperCount = ReadUIntPtr();
    r.reversePInvokeWrapperIndices = ReadUIntPtr();
    r.rgctxRangesCount = ReadIntPtr();
    r.rgctxRanges = ReadUIntPtr();
    r.rgctxsCount = ReadIntPtr();
    r.rgctxs = ReadUIntPtr();
    r.debuggerMetadata = ReadUIntPtr();
    if (V_IN(27, 27.2)) { r.customAttributeCacheGenerator = ReadUIntPtr(); }
    if (V_MIN(27)) { r.moduleInitializer = ReadUIntPtr(); }
    if (V_MIN(27)) { r.staticConstructorTypeIndices = ReadUIntPtr(); }
    if (V_MIN(27)) { r.metadataRegistration = ReadUIntPtr(); }
    if (V_MIN(27)) { r.codeRegistaration = ReadUIntPtr(); }
    return r;
}

Il2CppTokenRangePair BinaryStream::ReadTokenRangePair() {
    Il2CppTokenRangePair r;
    r.token = ReadUInt32();
    r.range.start = ReadInt32();
    r.range.length = ReadInt32();
    return r;
}

Il2CppRGCTXDefinition BinaryStream::ReadRGCTXDefinition() {
    Il2CppRGCTXDefinition r;
    if (V_MAX(27.1)) { r.type_pre29 = ReadInt32(); }
    if (V_MIN(29)) { r.type_post29 = ReadUIntPtr(); }
    if (V_MAX(27.1)) { r.rgctxDataDummy = ReadInt32(); }
    if (V_MIN(27.2)) { r._data = ReadUIntPtr(); }
    return r;
}

Il2CppType BinaryStream::ReadIl2CppType() {
    Il2CppType r;
    r.datapoint = ReadUIntPtr();
    r.bits = ReadUInt32();
    return r;
}

// ============================================================
// Metadata struct readers
// ============================================================

Il2CppGlobalMetadataHeader BinaryStream::ReadGlobalMetadataHeader() {
    Il2CppGlobalMetadataHeader h;
    h.sanity = ReadUInt32();
    h.version = ReadInt32();
    h.stringLiteralOffset = ReadUInt32();
    h.stringLiteralSize = ReadInt32();
    h.stringLiteralDataOffset = ReadUInt32();
    h.stringLiteralDataSize = ReadInt32();
    h.stringOffset = ReadUInt32();
    h.stringSize = ReadInt32();
    h.eventsOffset = ReadUInt32();
    h.eventsSize = ReadInt32();
    h.propertiesOffset = ReadUInt32();
    h.propertiesSize = ReadInt32();
    h.methodsOffset = ReadUInt32();
    h.methodsSize = ReadInt32();
    h.parameterDefaultValuesOffset = ReadUInt32();
    h.parameterDefaultValuesSize = ReadInt32();
    h.fieldDefaultValuesOffset = ReadUInt32();
    h.fieldDefaultValuesSize = ReadInt32();
    h.fieldAndParameterDefaultValueDataOffset = ReadUInt32();
    h.fieldAndParameterDefaultValueDataSize = ReadInt32();
    h.fieldMarshaledSizesOffset = ReadInt32();
    h.fieldMarshaledSizesSize = ReadInt32();
    h.parametersOffset = ReadUInt32();
    h.parametersSize = ReadInt32();
    h.fieldsOffset = ReadUInt32();
    h.fieldsSize = ReadInt32();
    h.genericParametersOffset = ReadUInt32();
    h.genericParametersSize = ReadInt32();
    h.genericParameterConstraintsOffset = ReadUInt32();
    h.genericParameterConstraintsSize = ReadInt32();
    h.genericContainersOffset = ReadUInt32();
    h.genericContainersSize = ReadInt32();
    h.nestedTypesOffset = ReadUInt32();
    h.nestedTypesSize = ReadInt32();
    h.interfacesOffset = ReadUInt32();
    h.interfacesSize = ReadInt32();
    h.vtableMethodsOffset = ReadUInt32();
    h.vtableMethodsSize = ReadInt32();
    h.interfaceOffsetsOffset = ReadInt32();
    h.interfaceOffsetsSize = ReadInt32();
    h.typeDefinitionsOffset = ReadUInt32();
    h.typeDefinitionsSize = ReadInt32();
    if (V_MAX(24.1)) { h.rgctxEntriesOffset = ReadUInt32(); }
    if (V_MAX(24.1)) { h.rgctxEntriesCount = ReadInt32(); }
    h.imagesOffset = ReadUInt32();
    h.imagesSize = ReadInt32();
    h.assembliesOffset = ReadUInt32();
    h.assembliesSize = ReadInt32();
    if (V_IN(19, 24.5)) { h.metadataUsageListsOffset = ReadUInt32(); }
    if (V_IN(19, 24.5)) { h.metadataUsageListsCount = ReadInt32(); }
    if (V_IN(19, 24.5)) { h.metadataUsagePairsOffset = ReadUInt32(); }
    if (V_IN(19, 24.5)) { h.metadataUsagePairsCount = ReadInt32(); }
    if (V_MIN(19)) { h.fieldRefsOffset = ReadUInt32(); }
    if (V_MIN(19)) { h.fieldRefsSize = ReadInt32(); }
    if (V_MIN(20)) { h.referencedAssembliesOffset = ReadInt32(); }
    if (V_MIN(20)) { h.referencedAssembliesSize = ReadInt32(); }
    if (V_IN(21, 27.2)) { h.attributesInfoOffset = ReadUInt32(); }
    if (V_IN(21, 27.2)) { h.attributesInfoCount = ReadInt32(); }
    if (V_IN(21, 27.2)) { h.attributeTypesOffset = ReadUInt32(); }
    if (V_IN(21, 27.2)) { h.attributeTypesCount = ReadInt32(); }
    if (V_MIN(29)) { h.attributeDataOffset = ReadUInt32(); }
    if (V_MIN(29)) { h.attributeDataSize = ReadInt32(); }
    if (V_MIN(29)) { h.attributeDataRangeOffset = ReadUInt32(); }
    if (V_MIN(29)) { h.attributeDataRangeSize = ReadInt32(); }
    if (V_MIN(22)) { h.unresolvedVirtualCallParameterTypesOffset = ReadInt32(); }
    if (V_MIN(22)) { h.unresolvedVirtualCallParameterTypesSize = ReadInt32(); }
    if (V_MIN(22)) { h.unresolvedVirtualCallParameterRangesOffset = ReadInt32(); }
    if (V_MIN(22)) { h.unresolvedVirtualCallParameterRangesSize = ReadInt32(); }
    if (V_MIN(23)) { h.windowsRuntimeTypeNamesOffset = ReadInt32(); }
    if (V_MIN(23)) { h.windowsRuntimeTypeNamesSize = ReadInt32(); }
    if (V_MIN(27)) { h.windowsRuntimeStringsOffset = ReadInt32(); }
    if (V_MIN(27)) { h.windowsRuntimeStringsSize = ReadInt32(); }
    if (V_MIN(24)) { h.exportedTypeDefinitionsOffset = ReadInt32(); }
    if (V_MIN(24)) { h.exportedTypeDefinitionsSize = ReadInt32(); }
    return h;
}

Il2CppImageDefinition BinaryStream::ReadImageDefinition() {
    Il2CppImageDefinition r;
    r.nameIndex = ReadUInt32();
    r.assemblyIndex = ReadInt32();
    r.typeStart = ReadInt32();
    r.typeCount = ReadUInt32();
    if (V_MIN(24)) { r.exportedTypeStart = ReadInt32(); }
    if (V_MIN(24)) { r.exportedTypeCount = ReadUInt32(); }
    r.entryPointIndex = ReadInt32();
    if (V_MIN(19)) { r.token = ReadUInt32(); }
    if (V_MIN(24.1)) { r.customAttributeStart = ReadInt32(); }
    if (V_MIN(24.1)) { r.customAttributeCount = ReadUInt32(); }
    return r;
}

Il2CppAssemblyDefinition BinaryStream::ReadAssemblyDefinition() {
    Il2CppAssemblyDefinition r;
    r.imageIndex = ReadInt32();
    if (V_MIN(24.1)) { r.token = ReadUInt32(); }
    if (V_MAX(24)) { r.customAttributeIndex = ReadInt32(); }
    if (V_MIN(20)) { r.referencedAssemblyStart = ReadInt32(); }
    if (V_MIN(20)) { r.referencedAssemblyCount = ReadInt32(); }
    // aname
    r.aname.nameIndex = ReadUInt32();
    r.aname.cultureIndex = ReadUInt32();
    if (V_MAX(24.3)) { r.aname.hashValueIndex = ReadInt32(); }
    r.aname.publicKeyIndex = ReadUInt32();
    r.aname.hash_alg = ReadUInt32();
    r.aname.hash_len = ReadInt32();
    r.aname.flags = ReadUInt32();
    r.aname.major = ReadInt32();
    r.aname.minor = ReadInt32();
    r.aname.build = ReadInt32();
    r.aname.revision = ReadInt32();
    auto pkToken = ReadBytes(8);
    memcpy(r.aname.public_key_token, pkToken.data(), 8);
    return r;
}

Il2CppTypeDefinition BinaryStream::ReadTypeDefinition() {
    Il2CppTypeDefinition r;
    r.nameIndex = ReadUInt32();
    r.namespaceIndex = ReadUInt32();
    if (V_MAX(24)) { r.customAttributeIndex = ReadInt32(); }
    r.byvalTypeIndex = ReadInt32();
    if (V_MAX(24.5)) { r.byrefTypeIndex = ReadInt32(); }
    r.declaringTypeIndex = ReadInt32();
    r.parentIndex = ReadInt32();
    r.elementTypeIndex = ReadInt32();
    if (V_MAX(24.1)) { r.rgctxStartIndex = ReadInt32(); }
    if (V_MAX(24.1)) { r.rgctxCount = ReadInt32(); }
    r.genericContainerIndex = ReadInt32();
    if (V_MAX(22)) { r.delegateWrapperFromManagedToNativeIndex = ReadInt32(); }
    if (V_MAX(22)) { r.marshalingFunctionsIndex = ReadInt32(); }
    if (V_IN(21, 22)) { r.ccwFunctionIndex = ReadInt32(); }
    if (V_IN(21, 22)) { r.guidIndex = ReadInt32(); }
    r.flags = ReadUInt32();
    r.fieldStart = ReadInt32();
    r.methodStart = ReadInt32();
    r.eventStart = ReadInt32();
    r.propertyStart = ReadInt32();
    r.nestedTypesStart = ReadInt32();
    r.interfacesStart = ReadInt32();
    r.vtableStart = ReadInt32();
    r.interfaceOffsetsStart = ReadInt32();
    r.method_count = ReadUInt16();
    r.property_count = ReadUInt16();
    r.field_count = ReadUInt16();
    r.event_count = ReadUInt16();
    r.nested_type_count = ReadUInt16();
    r.vtable_count = ReadUInt16();
    r.interfaces_count = ReadUInt16();
    r.interface_offsets_count = ReadUInt16();
    r.bitfield = ReadUInt32();
    if (V_MIN(19)) { r.token = ReadUInt32(); }
    return r;
}

Il2CppMethodDefinition BinaryStream::ReadMethodDefinition() {
    Il2CppMethodDefinition r;
    r.nameIndex = ReadUInt32();
    r.declaringType = ReadInt32();
    r.returnType = ReadInt32();
    if (V_MIN(31)) { r.returnParameterToken = ReadInt32(); }
    r.parameterStart = ReadInt32();
    if (V_MAX(24)) { r.customAttributeIndex = ReadInt32(); }
    r.genericContainerIndex = ReadInt32();
    if (V_MAX(24.1)) { r.methodIndex = ReadInt32(); }
    if (V_MAX(24.1)) { r.invokerIndex = ReadInt32(); }
    if (V_MAX(24.1)) { r.delegateWrapperIndex = ReadInt32(); }
    if (V_MAX(24.1)) { r.rgctxStartIndex = ReadInt32(); }
    if (V_MAX(24.1)) { r.rgctxCount = ReadInt32(); }
    r.token = ReadUInt32();
    r.flags = ReadUInt16();
    r.iflags = ReadUInt16();
    r.slot = ReadUInt16();
    r.parameterCount = ReadUInt16();
    return r;
}

Il2CppParameterDefinition BinaryStream::ReadParameterDefinition() {
    Il2CppParameterDefinition r;
    r.nameIndex = ReadUInt32();
    r.token = ReadUInt32();
    if (V_MAX(24)) { r.customAttributeIndex = ReadInt32(); }
    r.typeIndex = ReadInt32();
    return r;
}

Il2CppFieldDefinition BinaryStream::ReadFieldDefinition() {
    Il2CppFieldDefinition r;
    r.nameIndex = ReadUInt32();
    r.typeIndex = ReadInt32();
    if (V_MAX(24)) { r.customAttributeIndex = ReadInt32(); }
    if (V_MIN(19)) { r.token = ReadUInt32(); }
    return r;
}

Il2CppFieldDefaultValue BinaryStream::ReadFieldDefaultValue() {
    Il2CppFieldDefaultValue r;
    r.fieldIndex = ReadInt32();
    r.typeIndex = ReadInt32();
    r.dataIndex = ReadInt32();
    return r;
}

Il2CppParameterDefaultValue BinaryStream::ReadParameterDefaultValue() {
    Il2CppParameterDefaultValue r;
    r.parameterIndex = ReadInt32();
    r.typeIndex = ReadInt32();
    r.dataIndex = ReadInt32();
    return r;
}

Il2CppPropertyDefinition BinaryStream::ReadPropertyDefinition() {
    Il2CppPropertyDefinition r;
    r.nameIndex = ReadUInt32();
    r.get = ReadInt32();
    r.set = ReadInt32();
    r.attrs = ReadUInt32();
    if (V_MAX(24)) { r.customAttributeIndex = ReadInt32(); }
    if (V_MIN(19)) { r.token = ReadUInt32(); }
    return r;
}

Il2CppCustomAttributeTypeRange BinaryStream::ReadCustomAttributeTypeRange() {
    Il2CppCustomAttributeTypeRange r;
    if (V_MIN(24.1)) { r.token = ReadUInt32(); }
    r.start = ReadInt32();
    r.count = ReadInt32();
    return r;
}

Il2CppCustomAttributeDataRange BinaryStream::ReadCustomAttributeDataRange() {
    Il2CppCustomAttributeDataRange r;
    r.token = ReadUInt32();
    r.startOffset = ReadUInt32();
    return r;
}

Il2CppMetadataUsageList BinaryStream::ReadMetadataUsageList() {
    Il2CppMetadataUsageList r;
    r.start = ReadUInt32();
    r.count = ReadUInt32();
    return r;
}

Il2CppMetadataUsagePair BinaryStream::ReadMetadataUsagePair() {
    Il2CppMetadataUsagePair r;
    r.destinationIndex = ReadUInt32();
    r.encodedSourceIndex = ReadUInt32();
    return r;
}

Il2CppStringLiteral BinaryStream::ReadStringLiteral() {
    Il2CppStringLiteral r;
    r.length = ReadUInt32();
    r.dataIndex = ReadInt32();
    return r;
}

Il2CppEventDefinition BinaryStream::ReadEventDefinition() {
    Il2CppEventDefinition r;
    r.nameIndex = ReadUInt32();
    r.typeIndex = ReadInt32();
    r.add = ReadInt32();
    r.remove = ReadInt32();
    r.raise = ReadInt32();
    if (V_MAX(24)) { r.customAttributeIndex = ReadInt32(); }
    if (V_MIN(19)) { r.token = ReadUInt32(); }
    return r;
}

Il2CppGenericContainer BinaryStream::ReadGenericContainer() {
    Il2CppGenericContainer r;
    r.ownerIndex = ReadInt32();
    r.type_argc = ReadInt32();
    r.is_method = ReadInt32();
    r.genericParameterStart = ReadInt32();
    return r;
}

Il2CppFieldRef BinaryStream::ReadFieldRef() {
    Il2CppFieldRef r;
    r.typeIndex = ReadInt32();
    r.fieldIndex = ReadInt32();
    return r;
}

Il2CppGenericParameter BinaryStream::ReadGenericParameter() {
    Il2CppGenericParameter r;
    r.ownerIndex = ReadInt32();
    r.nameIndex = ReadUInt32();
    r.constraintsStart = ReadInt16();
    r.constraintsCount = ReadInt16();
    r.num = ReadUInt16();
    r.flags = ReadUInt16();
    return r;
}

// ============================================================
// ELF readers (fixed layout - no version checks)
// ============================================================

Elf32_Ehdr BinaryStream::ReadElf32Ehdr() {
    Elf32_Ehdr r;
    r.ei_mag = ReadUInt32();
    r.ei_class = ReadByte();
    r.ei_data = ReadByte();
    r.ei_version = ReadByte();
    r.ei_osabi = ReadByte();
    r.ei_abiversion = ReadByte();
    auto pad = ReadBytes(7);
    memcpy(r.ei_pad, pad.data(), 7);
    r.e_type = ReadUInt16();
    r.e_machine = ReadUInt16();
    r.e_version = ReadUInt32();
    r.e_entry = ReadUInt32();
    r.e_phoff = ReadUInt32();
    r.e_shoff = ReadUInt32();
    r.e_flags = ReadUInt32();
    r.e_ehsize = ReadUInt16();
    r.e_phentsize = ReadUInt16();
    r.e_phnum = ReadUInt16();
    r.e_shentsize = ReadUInt16();
    r.e_shnum = ReadUInt16();
    r.e_shstrndx = ReadUInt16();
    return r;
}

Elf32_Phdr BinaryStream::ReadElf32Phdr() {
    Elf32_Phdr r;
    r.p_type = ReadUInt32(); r.p_offset = ReadUInt32(); r.p_vaddr = ReadUInt32();
    r.p_paddr = ReadUInt32(); r.p_filesz = ReadUInt32(); r.p_memsz = ReadUInt32();
    r.p_flags = ReadUInt32(); r.p_align = ReadUInt32();
    return r;
}

Elf32_Dyn BinaryStream::ReadElf32Dyn() {
    Elf32_Dyn r;
    r.d_tag = ReadInt32(); r.d_un = ReadUInt32();
    return r;
}

Elf32_Sym BinaryStream::ReadElf32Sym() {
    Elf32_Sym r;
    r.st_name = ReadUInt32(); r.st_value = ReadUInt32(); r.st_size = ReadUInt32();
    r.st_info = ReadByte(); r.st_other = ReadByte(); r.st_shndx = ReadUInt16();
    return r;
}

Elf32_Shdr BinaryStream::ReadElf32Shdr() {
    Elf32_Shdr r;
    r.sh_name = ReadUInt32(); r.sh_type = ReadUInt32(); r.sh_flags = ReadUInt32();
    r.sh_addr = ReadUInt32(); r.sh_offset = ReadUInt32(); r.sh_size = ReadUInt32();
    r.sh_link = ReadUInt32(); r.sh_info = ReadUInt32(); r.sh_addralign = ReadUInt32();
    r.sh_entsize = ReadUInt32();
    return r;
}

Elf32_Rel BinaryStream::ReadElf32Rel() {
    Elf32_Rel r;
    r.r_offset = ReadUInt32(); r.r_info = ReadUInt32();
    return r;
}

Elf64_Ehdr BinaryStream::ReadElf64Ehdr() {
    Elf64_Ehdr r;
    r.ei_mag = ReadUInt32();
    r.ei_class = ReadByte(); r.ei_data = ReadByte(); r.ei_version = ReadByte();
    r.ei_osabi = ReadByte(); r.ei_abiversion = ReadByte();
    auto pad = ReadBytes(7);
    memcpy(r.ei_pad, pad.data(), 7);
    r.e_type = ReadUInt16(); r.e_machine = ReadUInt16(); r.e_version = ReadUInt32();
    r.e_entry = ReadUInt64(); r.e_phoff = ReadUInt64(); r.e_shoff = ReadUInt64();
    r.e_flags = ReadUInt32(); r.e_ehsize = ReadUInt16(); r.e_phentsize = ReadUInt16();
    r.e_phnum = ReadUInt16(); r.e_shentsize = ReadUInt16(); r.e_shnum = ReadUInt16();
    r.e_shstrndx = ReadUInt16();
    return r;
}

Elf64_Phdr BinaryStream::ReadElf64Phdr() {
    Elf64_Phdr r;
    r.p_type = ReadUInt32(); r.p_flags = ReadUInt32();
    r.p_offset = ReadUInt64(); r.p_vaddr = ReadUInt64(); r.p_paddr = ReadUInt64();
    r.p_filesz = ReadUInt64(); r.p_memsz = ReadUInt64(); r.p_align = ReadUInt64();
    return r;
}

Elf64_Dyn BinaryStream::ReadElf64Dyn() {
    Elf64_Dyn r;
    r.d_tag = ReadInt64(); r.d_un = ReadUInt64();
    return r;
}

Elf64_Sym BinaryStream::ReadElf64Sym() {
    Elf64_Sym r;
    r.st_name = ReadUInt32(); r.st_info = ReadByte(); r.st_other = ReadByte();
    r.st_shndx = ReadUInt16(); r.st_value = ReadUInt64(); r.st_size = ReadUInt64();
    return r;
}

Elf64_Shdr BinaryStream::ReadElf64Shdr() {
    Elf64_Shdr r;
    r.sh_name = ReadUInt32(); r.sh_type = ReadUInt32(); r.sh_flags = ReadUInt64();
    r.sh_addr = ReadUInt64(); r.sh_offset = ReadUInt64(); r.sh_size = ReadUInt64();
    r.sh_link = ReadUInt32(); r.sh_info = ReadUInt32(); r.sh_addralign = ReadUInt64();
    r.sh_entsize = ReadUInt64();
    return r;
}

Elf64_Rela BinaryStream::ReadElf64Rela() {
    Elf64_Rela r;
    r.r_offset = ReadUInt64(); r.r_info = ReadUInt64(); r.r_addend = ReadUInt64();
    return r;
}

// PE readers
DosHeader BinaryStream::ReadDosHeader() {
    DosHeader r;
    r.Magic = ReadUInt16();
    r.Cblp = ReadUInt16(); r.Cp = ReadUInt16(); r.Crlc = ReadUInt16();
    r.Cparhdr = ReadUInt16(); r.Minalloc = ReadUInt16(); r.Maxalloc = ReadUInt16();
    r.Ss = ReadUInt16(); r.Sp = ReadUInt16(); r.Csum = ReadUInt16();
    r.Ip = ReadUInt16(); r.Cs = ReadUInt16(); r.Lfarlc = ReadUInt16(); r.Ovno = ReadUInt16();
    for (int i = 0; i < 4; i++) r.Res[i] = ReadUInt16();
    r.Oemid = ReadUInt16(); r.Oeminfo = ReadUInt16();
    for (int i = 0; i < 10; i++) r.Res2[i] = ReadUInt16();
    r.Lfanew = ReadUInt32();
    return r;
}

PEFileHeader BinaryStream::ReadPEFileHeader() {
    PEFileHeader r;
    r.Machine = ReadUInt16(); r.NumberOfSections = ReadUInt16();
    r.TimeDateStamp = ReadUInt32(); r.PointerToSymbolTable = ReadUInt32();
    r.NumberOfSymbols = ReadUInt32(); r.SizeOfOptionalHeader = ReadUInt16();
    r.Characteristics = ReadUInt16();
    return r;
}

PESectionHeader BinaryStream::ReadPESectionHeader() {
    PESectionHeader r;
    auto name = ReadBytes(8);
    memcpy(r.Name, name.data(), 8);
    r.VirtualSize = ReadUInt32(); r.VirtualAddress = ReadUInt32();
    r.SizeOfRawData = ReadUInt32(); r.PointerToRawData = ReadUInt32();
    r.PointerToRelocations = ReadUInt32(); r.PointerToLinenumbers = ReadUInt32();
    r.NumberOfRelocations = ReadUInt16(); r.NumberOfLinenumbers = ReadUInt16();
    r.Characteristics = ReadUInt32();
    return r;
}

#undef V_IN
#undef V_MIN
#undef V_MAX
