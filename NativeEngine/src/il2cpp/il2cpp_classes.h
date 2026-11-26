#pragma once

#include "io/binary_stream.h"

#include <cstdint>
#include <cstddef>

namespace il2cppdumper {


enum class Il2CppTypeEnum : int {
    IL2CPP_TYPE_END = 0x00,             /* End of List */
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
    IL2CPP_TYPE_IL2CPP_TYPE_INDEX = 0xff,
};

struct Il2CppType {
    uint64_t datapoint = 0;
    uint32_t bits = 0;
    uint32_t attrs = 0;
    int type = 0;
    uint32_t num_mods = 0;
    uint32_t byref = 0;
    uint32_t pinned = 0;
    uint32_t valuetype = 0;

    void init(double v) {
        attrs = bits & 0xffff;
        type = static_cast<int>((bits >> 16) & 0xff);
        if (v >= 27.2) {
            num_mods = (bits >> 24) & 0x1f;
            byref = (bits >> 29) & 1;
            pinned = (bits >> 30) & 1;
            valuetype = bits >> 31;
        } else {
            num_mods = (bits >> 24) & 0x3f;
            byref = (bits >> 30) & 1;
            pinned = bits >> 31;
        }
    }

    int64_t klassIndex() const { return static_cast<int64_t>(datapoint); }
    uint64_t typeHandle() const { return datapoint; }
    uint64_t typePtr() const { return datapoint; }
    uint64_t arrayPtr() const { return datapoint; }
    int64_t genericParameterIndex() const { return static_cast<int64_t>(datapoint); }
    uint64_t genericParameterHandle() const { return datapoint; }
    uint64_t genericClassPtr() const { return datapoint; }
};

struct Il2CppCodeRegistration {
    uint64_t methodPointersCount = 0;
    uint64_t methodPointers = 0;
    uint64_t delegateWrappersFromNativeToManagedCount = 0;
    uint64_t delegateWrappersFromNativeToManaged = 0;
    uint64_t reversePInvokeWrapperCount = 0;
    uint64_t reversePInvokeWrappers = 0;
    uint64_t delegateWrappersFromManagedToNativeCount = 0;
    uint64_t delegateWrappersFromManagedToNative = 0;
    uint64_t marshalingFunctionsCount = 0;
    uint64_t marshalingFunctions = 0;
    uint64_t ccwMarshalingFunctionsCount = 0;
    uint64_t ccwMarshalingFunctions = 0;
    uint64_t genericMethodPointersCount = 0;
    uint64_t genericMethodPointers = 0;
    uint64_t genericAdjustorThunks = 0;
    uint64_t invokerPointersCount = 0;
    uint64_t invokerPointers = 0;
    uint64_t customAttributeCount = 0;
    uint64_t customAttributeGenerators = 0;
    uint64_t guidCount = 0;
    uint64_t guids = 0;
    uint64_t unresolvedVirtualCallCount = 0;
    uint64_t unresolvedVirtualCallPointers = 0;
    uint64_t unresolvedInstanceCallPointers = 0;
    uint64_t unresolvedStaticCallPointers = 0;
    uint64_t interopDataCount = 0;
    uint64_t interopData = 0;
    uint64_t windowsRuntimeFactoryCount = 0;
    uint64_t windowsRuntimeFactoryTable = 0;
    uint64_t codeGenModulesCount = 0;
    uint64_t codeGenModules = 0;
};

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
    int64_t methodReferencesCount = 0;
    uint64_t methodReferences = 0;
    int64_t fieldOffsetsCount = 0;
    uint64_t fieldOffsets = 0;
    int64_t typeDefinitionsSizesCount = 0;
    uint64_t typeDefinitionsSizes = 0;
    uint64_t metadataUsagesCount = 0;
    uint64_t metadataUsages = 0;
};

struct Il2CppGenericContext {
    uint64_t class_inst = 0;
    uint64_t method_inst = 0;
};

struct Il2CppGenericClass {
    int64_t typeDefinitionIndex = 0;
    uint64_t type = 0;
    Il2CppGenericContext context;
    uint64_t cached_class = 0;
};

struct Il2CppGenericInst {
    int64_t type_argc = 0;
    uint64_t type_argv = 0;
};

struct Il2CppArrayType {
    uint64_t etype = 0;
    uint8_t rank = 0;
    uint8_t numsizes = 0;
    uint8_t numlobounds = 0;
    uint64_t sizes = 0;
    uint64_t lobounds = 0;
};

struct Il2CppGenericMethodIndices {
    int32_t methodIndex = 0;
    int32_t invokerIndex = 0;
    int32_t adjustorThunk = 0;
};

struct Il2CppGenericMethodFunctionsDefinitions {
    int32_t genericMethodIndex = 0;
    Il2CppGenericMethodIndices indices;
};

struct Il2CppMethodSpec {
    int32_t methodDefinitionIndex = 0;
    int32_t classIndexIndex = 0;
    int32_t methodIndexIndex = 0;

    bool operator==(const Il2CppMethodSpec& o) const {
        return methodDefinitionIndex == o.methodDefinitionIndex && classIndexIndex == o.classIndexIndex &&
               methodIndexIndex == o.methodIndexIndex;
    }
};

struct MethodSpecHash {
    size_t operator()(const Il2CppMethodSpec& m) const {
        size_t h = 1469598103934665603ull;
        auto mix = [&h](int32_t v) {
            h ^= static_cast<size_t>(v) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        };
        mix(m.methodDefinitionIndex);
        mix(m.classIndexIndex);
        mix(m.methodIndexIndex);
        return h;
    }
};

struct Il2CppCodeGenModule {
    uint64_t moduleName = 0;
    int64_t methodPointerCount = 0;
    uint64_t methodPointers = 0;
    int64_t adjustorThunkCount = 0;
    uint64_t adjustorThunks = 0;
    uint64_t invokerIndices = 0;
    uint64_t reversePInvokeWrapperCount = 0;
    uint64_t reversePInvokeWrapperIndices = 0;
    int64_t rgctxRangesCount = 0;
    uint64_t rgctxRanges = 0;
    int64_t rgctxsCount = 0;
    uint64_t rgctxs = 0;
    uint64_t debuggerMetadata = 0;
    uint64_t customAttributeCacheGenerator = 0;
    uint64_t moduleInitializer = 0;
    uint64_t staticConstructorTypeIndices = 0;
    uint64_t metadataRegistration = 0;
    uint64_t codeRegistaration = 0;
};

struct Il2CppRange {
    int32_t start = 0;
    int32_t length = 0;
};

struct Il2CppTokenRangePair {
    uint32_t token = 0;
    Il2CppRange range;
};

template <>
inline Il2CppType BinaryStream::readClass<Il2CppType>() {
    Il2CppType t{};
    t.datapoint = readUPtr();
    t.bits = readU32();
    return t;
}

template <>
inline Il2CppCodeRegistration BinaryStream::readClass<Il2CppCodeRegistration>() {
    Il2CppCodeRegistration t{};
    if (inRange(0, 24.1)) {
        t.methodPointersCount = readUPtr();
        t.methodPointers = readUPtr();
    }
    if (inRange(0, 21)) {
        t.delegateWrappersFromNativeToManagedCount = readUPtr();
        t.delegateWrappersFromNativeToManaged = readUPtr();
    }
    if (inRange(22, 99)) {
        t.reversePInvokeWrapperCount = readUPtr();
        t.reversePInvokeWrappers = readUPtr();
    }
    if (inRange(0, 22)) {
        t.delegateWrappersFromManagedToNativeCount = readUPtr();
        t.delegateWrappersFromManagedToNative = readUPtr();
        t.marshalingFunctionsCount = readUPtr();
        t.marshalingFunctions = readUPtr();
    }
    if (inRange(21, 22)) {
        t.ccwMarshalingFunctionsCount = readUPtr();
        t.ccwMarshalingFunctions = readUPtr();
    }
    t.genericMethodPointersCount = readUPtr();
    t.genericMethodPointers = readUPtr();
    if (inRange(24.5, 24.5) || inRange(27.1, 99)) {
        t.genericAdjustorThunks = readUPtr();
    }
    t.invokerPointersCount = readUPtr();
    t.invokerPointers = readUPtr();
    if (inRange(0, 24.5)) {
        t.customAttributeCount = readUPtr();
        t.customAttributeGenerators = readUPtr();
    }
    if (inRange(21, 22)) {
        t.guidCount = readUPtr();
        t.guids = readUPtr();
    }
    if (inRange(22, 99)) {
        t.unresolvedVirtualCallCount = readUPtr();
        t.unresolvedVirtualCallPointers = readUPtr();
    }
    if (inRange(29.1, 99)) {
        t.unresolvedInstanceCallPointers = readUPtr();
        t.unresolvedStaticCallPointers = readUPtr();
    }
    if (inRange(23, 99)) {
        t.interopDataCount = readUPtr();
        t.interopData = readUPtr();
    }
    if (inRange(24.3, 99)) {
        t.windowsRuntimeFactoryCount = readUPtr();
        t.windowsRuntimeFactoryTable = readUPtr();
    }
    if (inRange(24.2, 99)) {
        t.codeGenModulesCount = readUPtr();
        t.codeGenModules = readUPtr();
    }
    return t;
}

template <>
inline Il2CppMetadataRegistration BinaryStream::readClass<Il2CppMetadataRegistration>() {
    Il2CppMetadataRegistration t{};
    t.genericClassesCount = readIPtr();
    t.genericClasses = readUPtr();
    t.genericInstsCount = readIPtr();
    t.genericInsts = readUPtr();
    t.genericMethodTableCount = readIPtr();
    t.genericMethodTable = readUPtr();
    t.typesCount = readIPtr();
    t.types = readUPtr();
    t.methodSpecsCount = readIPtr();
    t.methodSpecs = readUPtr();
    if (inRange(0, 16)) {
        t.methodReferencesCount = readIPtr();
        t.methodReferences = readUPtr();
    }
    t.fieldOffsetsCount = readIPtr();
    t.fieldOffsets = readUPtr();
    t.typeDefinitionsSizesCount = readIPtr();
    t.typeDefinitionsSizes = readUPtr();
    if (inRange(19, 99)) {
        t.metadataUsagesCount = readUPtr();
        t.metadataUsages = readUPtr();
    }
    return t;
}

template <>
inline Il2CppGenericContext BinaryStream::readClass<Il2CppGenericContext>() {
    Il2CppGenericContext t{};
    t.class_inst = readUPtr();
    t.method_inst = readUPtr();
    return t;
}

template <>
inline Il2CppGenericClass BinaryStream::readClass<Il2CppGenericClass>() {
    Il2CppGenericClass t{};
    if (inRange(0, 24.5)) {
        t.typeDefinitionIndex = readIPtr();
    }
    if (inRange(27, 99)) {
        t.type = readUPtr();
    }
    t.context = readClass<Il2CppGenericContext>();
    t.cached_class = readUPtr();
    return t;
}

template <>
inline Il2CppGenericInst BinaryStream::readClass<Il2CppGenericInst>() {
    Il2CppGenericInst t{};
    t.type_argc = readIPtr();
    t.type_argv = readUPtr();
    return t;
}

template <>
inline Il2CppArrayType BinaryStream::readClass<Il2CppArrayType>() {
    Il2CppArrayType t{};
    t.etype = readUPtr();
    t.rank = readU8();
    t.numsizes = readU8();
    t.numlobounds = readU8();
    t.sizes = readUPtr();
    t.lobounds = readUPtr();
    return t;
}

template <>
inline Il2CppGenericMethodIndices BinaryStream::readClass<Il2CppGenericMethodIndices>() {
    Il2CppGenericMethodIndices t{};
    t.methodIndex = readI32();
    t.invokerIndex = readI32();
    if (inRange(24.5, 24.5) || inRange(27.1, 99)) {
        t.adjustorThunk = readI32();
    }
    return t;
}

template <>
inline Il2CppGenericMethodFunctionsDefinitions BinaryStream::readClass<Il2CppGenericMethodFunctionsDefinitions>() {
    Il2CppGenericMethodFunctionsDefinitions t{};
    t.genericMethodIndex = readI32();
    t.indices = readClass<Il2CppGenericMethodIndices>();
    return t;
}

template <>
inline Il2CppMethodSpec BinaryStream::readClass<Il2CppMethodSpec>() {
    Il2CppMethodSpec t{};
    t.methodDefinitionIndex = readI32();
    t.classIndexIndex = readI32();
    t.methodIndexIndex = readI32();
    return t;
}

template <>
inline Il2CppCodeGenModule BinaryStream::readClass<Il2CppCodeGenModule>() {
    Il2CppCodeGenModule t{};
    t.moduleName = readUPtr();
    t.methodPointerCount = readIPtr();
    t.methodPointers = readUPtr();
    if (inRange(24.5, 24.5) || inRange(27.1, 99)) {
        t.adjustorThunkCount = readIPtr();
        t.adjustorThunks = readUPtr();
    }
    t.invokerIndices = readUPtr();
    t.reversePInvokeWrapperCount = readUPtr();
    t.reversePInvokeWrapperIndices = readUPtr();
    t.rgctxRangesCount = readIPtr();
    t.rgctxRanges = readUPtr();
    t.rgctxsCount = readIPtr();
    t.rgctxs = readUPtr();
    t.debuggerMetadata = readUPtr();
    if (inRange(27, 27.2)) {
        t.customAttributeCacheGenerator = readUPtr();
    }
    if (inRange(27, 99)) {
        t.moduleInitializer = readUPtr();
        t.staticConstructorTypeIndices = readUPtr();
        t.metadataRegistration = readUPtr();
        t.codeRegistaration = readUPtr();
    }
    return t;
}

template <>
inline Il2CppRange BinaryStream::readClass<Il2CppRange>() {
    Il2CppRange t{};
    t.start = readI32();
    t.length = readI32();
    return t;
}

template <>
inline Il2CppTokenRangePair BinaryStream::readClass<Il2CppTokenRangePair>() {
    Il2CppTokenRangePair t{};
    t.token = readU32();
    t.range = readClass<Il2CppRange>();
    return t;
}

}
