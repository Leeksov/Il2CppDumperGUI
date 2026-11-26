#include "il2cpp/il2cpp.h"

#include "formats/webassembly.h"

#include <cstdio>
#include <stdexcept>
#include <string>

namespace il2cppdumper {

void Il2Cpp::setProperties(double v, int64_t count) {
    version = v;
    this->metadataUsagesCount = count;
}

bool Il2Cpp::isWasmMemory() const {
    return dynamic_cast<const WebAssemblyMemory*>(this) != nullptr;
}

bool Il2Cpp::autoPlusInit(uint64_t codeRegistration, uint64_t metadataRegistration) {
    if (codeRegistration != 0) {
        uint32_t limit = isWasmMemory() ? 0x35000u : 0x50000u;
        if (version >= 24.2) {
            pCodeRegistration = mapVATRClass<Il2CppCodeRegistration>(codeRegistration);
            if (version == 31) {
                if (pCodeRegistration.genericMethodPointersCount > limit) {
                    codeRegistration -= pointerSize() * 2;
                } else {
                    version = 29;
                    std::printf("Change il2cpp version to: %g\n", version);
                }
            }
            if (version == 29) {
                if (pCodeRegistration.genericMethodPointersCount > limit) {
                    version = 29.1;
                    codeRegistration -= pointerSize() * 2;
                    std::printf("Change il2cpp version to: %g\n", version);
                }
            }
            if (version == 27) {
                if (pCodeRegistration.reversePInvokeWrapperCount > limit) {
                    version = 27.1;
                    codeRegistration -= pointerSize();
                    std::printf("Change il2cpp version to: %g\n", version);
                }
            }
            if (version == 24.4) {
                codeRegistration -= pointerSize() * 2;
                if (pCodeRegistration.reversePInvokeWrapperCount > limit) {
                    version = 24.5;
                    codeRegistration -= pointerSize();
                    std::printf("Change il2cpp version to: %g\n", version);
                }
            }
            if (version == 24.2) {
                if (pCodeRegistration.interopDataCount == 0) {
                    version = 24.3;
                    codeRegistration -= pointerSize() * 2;
                    std::printf("Change il2cpp version to: %g\n", version);
                }
            }
        }
    }
    std::printf("CodeRegistration : %llx\n", static_cast<unsigned long long>(codeRegistration));
    std::printf("MetadataRegistration : %llx\n", static_cast<unsigned long long>(metadataRegistration));
    if (codeRegistration != 0 && metadataRegistration != 0) {
        init(codeRegistration, metadataRegistration);
        return true;
    }
    return false;
}

void Il2Cpp::init(uint64_t codeRegistration, uint64_t metadataRegistration) {
    pCodeRegistration = mapVATRClass<Il2CppCodeRegistration>(codeRegistration);
    uint32_t limit = isWasmMemory() ? 0x35000u : 0x50000u;
    if (version == 27 && pCodeRegistration.invokerPointersCount > limit) {
        version = 27.1;
        std::printf("Change il2cpp version to: %g\n", version);
        pCodeRegistration = mapVATRClass<Il2CppCodeRegistration>(codeRegistration);
    }
    if (version == 27.1) {
        auto pCodeGenModules = mapVATRArray<uint64_t>(pCodeRegistration.codeGenModules,
                                                      static_cast<int64_t>(pCodeRegistration.codeGenModulesCount));
        for (uint64_t pCodeGenModule : pCodeGenModules) {
            auto codeGenModule = mapVATRClass<Il2CppCodeGenModule>(pCodeGenModule);
            if (codeGenModule.rgctxsCount > 0) {
                auto rgctxs = mapVATRArray<Il2CppRGCTXDefinition>(codeGenModule.rgctxs, codeGenModule.rgctxsCount);
                bool allAboveLimit = true;
                for (const auto& def : rgctxs) {
                    if (static_cast<uint64_t>(def.data.rgctxDataDummy) <= limit) {
                        allAboveLimit = false;
                        break;
                    }
                }
                if (allAboveLimit) {
                    version = 27.2;
                    std::printf("Change il2cpp version to: %g\n", version);
                }
                break;
            }
        }
    }
    if (version == 24.4 && pCodeRegistration.invokerPointersCount > limit) {
        version = 24.5;
        std::printf("Change il2cpp version to: %g\n", version);
        pCodeRegistration = mapVATRClass<Il2CppCodeRegistration>(codeRegistration);
    }
    if (version == 24.2 && pCodeRegistration.codeGenModules == 0) {
        version = 24.3;
        std::printf("Change il2cpp version to: %g\n", version);
        pCodeRegistration = mapVATRClass<Il2CppCodeRegistration>(codeRegistration);
    }
    pMetadataRegistration = mapVATRClass<Il2CppMetadataRegistration>(metadataRegistration);
    genericMethodPointers = mapVATRArray<uint64_t>(pCodeRegistration.genericMethodPointers,
                                                   static_cast<int64_t>(pCodeRegistration.genericMethodPointersCount));
    invokerPointers = mapVATRArray<uint64_t>(pCodeRegistration.invokerPointers,
                                             static_cast<int64_t>(pCodeRegistration.invokerPointersCount));
    if (version < 27) {
        customAttributeGenerators = mapVATRArray<uint64_t>(pCodeRegistration.customAttributeGenerators,
                                                           static_cast<int64_t>(pCodeRegistration.customAttributeCount));
    }
    if (version > 16 && version < 27) {
        metadataUsages = mapVATRArray<uint64_t>(pMetadataRegistration.metadataUsages, metadataUsagesCount);
    }
    if (version >= 22) {
        if (pCodeRegistration.reversePInvokeWrapperCount != 0) {
            reversePInvokeWrappers = mapVATRArray<uint64_t>(pCodeRegistration.reversePInvokeWrappers,
                                                            static_cast<int64_t>(pCodeRegistration.reversePInvokeWrapperCount));
        }
        if (pCodeRegistration.unresolvedVirtualCallCount != 0) {
            unresolvedVirtualCallPointers = mapVATRArray<uint64_t>(pCodeRegistration.unresolvedVirtualCallPointers,
                                                                   static_cast<int64_t>(pCodeRegistration.unresolvedVirtualCallCount));
        }
    }
    genericInstPointers = mapVATRArray<uint64_t>(pMetadataRegistration.genericInsts,
                                                 static_cast<int64_t>(pMetadataRegistration.genericInstsCount));
    genericInsts.clear();
    genericInsts.reserve(genericInstPointers.size());
    for (uint64_t p : genericInstPointers) {
        genericInsts.push_back(mapVATRClass<Il2CppGenericInst>(p));
    }
    fieldOffsetsArePointers = version > 21;
    if (version == 21) {
        auto fieldTest = mapVATRArray<uint32_t>(pMetadataRegistration.fieldOffsets, 6);
        fieldOffsetsArePointers = fieldTest.size() > 5 && fieldTest[0] == 0 && fieldTest[1] == 0 && fieldTest[2] == 0 &&
                                  fieldTest[3] == 0 && fieldTest[4] == 0 && fieldTest[5] > 0;
    }
    if (fieldOffsetsArePointers) {
        fieldOffsets = mapVATRArray<uint64_t>(pMetadataRegistration.fieldOffsets,
                                              static_cast<int64_t>(pMetadataRegistration.fieldOffsetsCount));
    } else {
        auto offsets32 = mapVATRArray<uint32_t>(pMetadataRegistration.fieldOffsets,
                                                static_cast<int64_t>(pMetadataRegistration.fieldOffsetsCount));
        fieldOffsets.clear();
        fieldOffsets.reserve(offsets32.size());
        for (uint32_t v : offsets32) {
            fieldOffsets.push_back(v);
        }
    }
    auto pTypes = mapVATRArray<uint64_t>(pMetadataRegistration.types,
                                         static_cast<int64_t>(pMetadataRegistration.typesCount));
    types.clear();
    types.resize(pTypes.size());
    for (size_t i = 0; i < pTypes.size(); i++) {
        types[i] = mapVATRClass<Il2CppType>(pTypes[i]);
        types[i].init(version);
        typeDic[pTypes[i]] = types[i];
    }
    if (version >= 24.2) {
        auto pCodeGenModules = mapVATRArray<uint64_t>(pCodeRegistration.codeGenModules,
                                                      static_cast<int64_t>(pCodeRegistration.codeGenModulesCount));
        codeGenModules.clear();
        codeGenModuleMethodPointers.clear();
        rgctxsDictionary.clear();
        for (uint64_t pCodeGenModule : pCodeGenModules) {
            auto codeGenModule = mapVATRClass<Il2CppCodeGenModule>(pCodeGenModule);
            std::string moduleName = readStringToNull(static_cast<size_t>(mapVATR(codeGenModule.moduleName)));
            codeGenModules[moduleName] = codeGenModule;
            std::vector<uint64_t> moduleMethodPointers;
            try {
                moduleMethodPointers = mapVATRArray<uint64_t>(codeGenModule.methodPointers, codeGenModule.methodPointerCount);
            } catch (const std::exception&) {
                moduleMethodPointers.assign(static_cast<size_t>(codeGenModule.methodPointerCount), 0);
            }
            codeGenModuleMethodPointers[moduleName] = std::move(moduleMethodPointers);

            auto& rgctxsDefDictionary = rgctxsDictionary[moduleName];
            if (codeGenModule.rgctxsCount > 0) {
                auto rgctxs = mapVATRArray<Il2CppRGCTXDefinition>(codeGenModule.rgctxs, codeGenModule.rgctxsCount);
                auto rgctxRanges = mapVATRArray<Il2CppTokenRangePair>(codeGenModule.rgctxRanges,
                                                                      codeGenModule.rgctxRangesCount);
                for (const auto& rgctxRange : rgctxRanges) {
                    std::vector<Il2CppRGCTXDefinition> defs(
                        rgctxs.begin() + rgctxRange.range.start,
                        rgctxs.begin() + rgctxRange.range.start + rgctxRange.range.length);
                    rgctxsDefDictionary[rgctxRange.token] = std::move(defs);
                }
            }
        }
    } else {
        methodPointers = mapVATRArray<uint64_t>(pCodeRegistration.methodPointers,
                                                static_cast<int64_t>(pCodeRegistration.methodPointersCount));
    }
    genericMethodTable = mapVATRArray<Il2CppGenericMethodFunctionsDefinitions>(
        pMetadataRegistration.genericMethodTable, static_cast<int64_t>(pMetadataRegistration.genericMethodTableCount));
    methodSpecs = mapVATRArray<Il2CppMethodSpec>(pMetadataRegistration.methodSpecs,
                                                 static_cast<int64_t>(pMetadataRegistration.methodSpecsCount));
    methodDefinitionMethodSpecs.clear();
    methodSpecGenericMethodPointers.clear();
    for (const auto& table : genericMethodTable) {
        const auto& methodSpec = methodSpecs[static_cast<size_t>(table.genericMethodIndex)];
        auto& list = methodDefinitionMethodSpecs[methodSpec.methodDefinitionIndex];
        list.push_back(methodSpec);
        methodSpecGenericMethodPointers[methodSpec] = genericMethodPointers[static_cast<size_t>(table.indices.methodIndex)];
    }
}

int Il2Cpp::getFieldOffsetFromIndex(int typeIndex, int fieldIndexInType, int fieldIndex, bool isValueType, bool isStatic) {
    try {
        int offset = -1;
        if (fieldOffsetsArePointers) {
            if (typeIndex >= 0 && typeIndex < static_cast<int>(fieldOffsets.size())) {
                uint64_t ptr = fieldOffsets[static_cast<size_t>(typeIndex)];
                if (ptr > 0) {
                    setPosition(static_cast<size_t>(mapVATR(ptr)) + 4ull * static_cast<uint64_t>(fieldIndexInType));
                    offset = readI32();
                }
            }
        } else {
            if (fieldIndex >= 0 && fieldIndex < static_cast<int>(fieldOffsets.size())) {
                offset = static_cast<int>(fieldOffsets[static_cast<size_t>(fieldIndex)]);
            }
        }
        if (offset > 0) {
            if (isValueType && !isStatic) {
                if (is32Bit) {
                    offset -= 8;
                } else {
                    offset -= 16;
                }
            }
        }
        return offset;
    } catch (const std::exception&) {
        return -1;
    }
}

uint64_t Il2Cpp::getMethodPointer(const std::string& imageName, const Il2CppMethodDefinition& methodDef) {
    if (version >= 24.2) {
        uint32_t methodToken = methodDef.token;
        auto it = codeGenModuleMethodPointers.find(imageName);
        if (it != codeGenModuleMethodPointers.end()) {
            const auto& ptrs = it->second;
            uint32_t methodPointerIndex = methodToken & 0x00FFFFFFu;
            if (methodPointerIndex >= 1 && methodPointerIndex <= ptrs.size()) {
                return ptrs[methodPointerIndex - 1];
            }
        }
        return 0;
    }
    int32_t methodIndex = methodDef.methodIndex;
    if (methodIndex >= 0 && methodIndex < static_cast<int32_t>(methodPointers.size())) {
        return methodPointers[static_cast<size_t>(methodIndex)];
    }
    return 0;
}

}
