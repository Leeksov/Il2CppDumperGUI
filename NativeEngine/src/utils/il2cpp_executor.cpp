#include "utils/il2cpp_executor.h"

#include "utils/section_helper.h"

#include <stdexcept>
#include <string>

namespace il2cppdumper {

namespace {

const std::map<int, std::string>& typeString() {
    static const std::map<int, std::string> m = {
        {1, "void"},   {2, "bool"},     {3, "char"},   {4, "sbyte"},  {5, "byte"},  {6, "short"},
        {7, "ushort"}, {8, "int"},      {9, "uint"},   {10, "long"},  {11, "ulong"}, {12, "float"},
        {13, "double"}, {14, "string"}, {22, "TypedReference"}, {24, "IntPtr"}, {25, "UIntPtr"}, {28, "object"},
    };
    return m;
}

std::string readUtf8StringN(BinaryStream& stream, int numChars) {
    if (numChars <= 0) {
        return {};
    }
    size_t start = stream.position();
    std::vector<uint8_t> bytes = stream.readBytes(static_cast<size_t>(numChars) * 4);
    size_t chars = 0;
    size_t i = 0;
    while (i < bytes.size() && chars < static_cast<size_t>(numChars)) {
        uint8_t c = bytes[i];
        if (c < 0x80) {
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            i += 4;
        } else {
            i += 1;
        }
        chars++;
    }
    if (i > bytes.size()) {
        i = bytes.size();
    }
    stream.setPosition(start);
    stream.readBytes(i);
    return std::string(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(i));
}

}

Il2CppExecutor::Il2CppExecutor(Metadata& metadata, Il2Cpp& il2Cpp) : metadata(&metadata), il2Cpp(&il2Cpp) {
    if (il2Cpp.version >= 27 && il2Cpp.version < 29) {
        uint64_t total = 0;
        for (const auto& imageDef : metadata.imageDefs) {
            total += imageDef.customAttributeCount;
        }
        customAttributeGenerators.assign(static_cast<size_t>(total), 0);
        for (const auto& imageDef : metadata.imageDefs) {
            std::string imageDefName = metadata.getStringFromIndex(imageDef.nameIndex);
            auto it = il2Cpp.codeGenModules.find(imageDefName);
            if (it == il2Cpp.codeGenModules.end()) {
                continue;
            }
            const auto& codeGenModule = it->second;
            if (imageDef.customAttributeCount > 0) {
                auto pointers = il2Cpp.mapVATRArray<uint64_t>(
                    il2Cpp.mapVATR(codeGenModule.customAttributeCacheGenerator),
                    static_cast<int64_t>(imageDef.customAttributeCount));
                std::copy(pointers.begin(), pointers.end(),
                          customAttributeGenerators.begin() + imageDef.customAttributeStart);
            }
        }
    } else if (il2Cpp.version < 27) {
        customAttributeGenerators = il2Cpp.customAttributeGenerators;
    }
}

std::string Il2CppExecutor::join(const std::vector<std::string>& parts, const char* sep) {
    std::string out;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i != 0) {
            out += sep;
        }
        out += parts[i];
    }
    return out;
}

std::string Il2CppExecutor::getTypeName(const Il2CppType* il2CppType, bool addNamespace, bool isNested) {
    switch (il2CppType->type) {
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_ARRAY): {
            auto arrayType = il2Cpp->mapVATRClass<Il2CppArrayType>(il2CppType->arrayPtr());
            const Il2CppType* elementType = il2Cpp->getIl2CppType(arrayType.etype);
            std::string commas = arrayType.rank > 1 ? std::string(arrayType.rank - 1, ',') : "";
            return getTypeName(elementType, addNamespace, false) + "[" + commas + "]";
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_SZARRAY): {
            const Il2CppType* elementType = il2Cpp->getIl2CppType(il2CppType->typePtr());
            return getTypeName(elementType, addNamespace, false) + "[]";
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_PTR): {
            const Il2CppType* oriType = il2Cpp->getIl2CppType(il2CppType->typePtr());
            return getTypeName(oriType, addNamespace, false) + "*";
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VAR):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_MVAR): {
            const Il2CppGenericParameter* param = getGenericParameteFromIl2CppType(il2CppType);
            return metadata->getStringFromIndex(param->nameIndex);
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CLASS):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST): {
            std::string str;
            const Il2CppTypeDefinition* typeDef = nullptr;
            const Il2CppGenericClass* genericClass = nullptr;
            Il2CppGenericClass genericClassLocal;
            if (il2CppType->type == static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST)) {
                genericClassLocal = il2Cpp->mapVATRClass<Il2CppGenericClass>(il2CppType->genericClassPtr());
                genericClass = &genericClassLocal;
                typeDef = getGenericClassTypeDefinition(genericClass);
            } else {
                typeDef = getTypeDefinitionFromIl2CppType(il2CppType);
            }
            if (typeDef == nullptr) {
                return str;
            }
            if (typeDef->declaringTypeIndex != -1) {
                str += getTypeName(&il2Cpp->types[static_cast<size_t>(typeDef->declaringTypeIndex)], addNamespace, true);
                str += '.';
            } else if (addNamespace) {
                std::string ns = metadata->getStringFromIndex(typeDef->namespaceIndex);
                if (!ns.empty()) {
                    str += ns + ".";
                }
            }
            std::string typeName = metadata->getStringFromIndex(typeDef->nameIndex);
            size_t index = typeName.find('`');
            if (index != std::string::npos) {
                str += typeName.substr(0, index);
            } else {
                str += typeName;
            }
            if (isNested) {
                return str;
            }
            if (genericClass != nullptr) {
                auto genericInst = il2Cpp->mapVATRClass<Il2CppGenericInst>(genericClass->context.class_inst);
                str += getGenericInstParams(&genericInst);
            } else if (typeDef->genericContainerIndex >= 0) {
                const auto& genericContainer = metadata->genericContainers[static_cast<size_t>(typeDef->genericContainerIndex)];
                str += getGenericContainerParams(&genericContainer);
            }
            return str;
        }
        default: {
            auto it = typeString().find(il2CppType->type);
            if (it == typeString().end()) {
                throw std::runtime_error("getTypeName: unsupported type");
            }
            return it->second;
        }
    }
}

std::string Il2CppExecutor::getTypeDefName(const Il2CppTypeDefinition* typeDef, bool addNamespace, bool genericParameter) {
    std::string prefix;
    if (typeDef->declaringTypeIndex != -1) {
        prefix = getTypeName(&il2Cpp->types[static_cast<size_t>(typeDef->declaringTypeIndex)], addNamespace, true) + ".";
    } else if (addNamespace) {
        std::string ns = metadata->getStringFromIndex(typeDef->namespaceIndex);
        if (!ns.empty()) {
            prefix = ns + ".";
        }
    }
    std::string typeName = metadata->getStringFromIndex(typeDef->nameIndex);
    if (typeDef->genericContainerIndex >= 0) {
        size_t index = typeName.find('`');
        if (index != std::string::npos) {
            typeName = typeName.substr(0, index);
        }
        if (genericParameter) {
            const auto& genericContainer = metadata->genericContainers[static_cast<size_t>(typeDef->genericContainerIndex)];
            typeName += getGenericContainerParams(&genericContainer);
        }
    }
    return prefix + typeName;
}

std::string Il2CppExecutor::getGenericInstParams(const Il2CppGenericInst* genericInst) {
    std::vector<std::string> names;
    auto pointers = il2Cpp->mapVATRArray<uint64_t>(genericInst->type_argv, genericInst->type_argc);
    for (uint64_t p : pointers) {
        const Il2CppType* t = il2Cpp->getIl2CppType(p);
        names.push_back(getTypeName(t, false, false));
    }
    return "<" + join(names, ", ") + ">";
}

std::string Il2CppExecutor::getGenericContainerParams(const Il2CppGenericContainer* genericContainer) {
    std::vector<std::string> names;
    for (int i = 0; i < genericContainer->type_argc; i++) {
        int genericParameterIndex = genericContainer->genericParameterStart + i;
        const auto& genericParameter = metadata->genericParameters[static_cast<size_t>(genericParameterIndex)];
        names.push_back(metadata->getStringFromIndex(genericParameter.nameIndex));
    }
    return "<" + join(names, ", ") + ">";
}

std::pair<std::string, std::string> Il2CppExecutor::getMethodSpecName(const Il2CppMethodSpec* methodSpec,
                                                                      bool addNamespace) {
    const auto& methodDef = metadata->methodDefs[static_cast<size_t>(methodSpec->methodDefinitionIndex)];
    const auto& typeDef = metadata->typeDefs[static_cast<size_t>(methodDef.declaringType)];
    std::string typeName = getTypeDefName(&typeDef, addNamespace, false);
    if (methodSpec->classIndexIndex != -1) {
        const auto& classInst = il2Cpp->genericInsts[static_cast<size_t>(methodSpec->classIndexIndex)];
        typeName += getGenericInstParams(&classInst);
    }
    std::string methodName = metadata->getStringFromIndex(methodDef.nameIndex);
    if (methodSpec->methodIndexIndex != -1) {
        const auto& methodInst = il2Cpp->genericInsts[static_cast<size_t>(methodSpec->methodIndexIndex)];
        methodName += getGenericInstParams(&methodInst);
    }
    return {typeName, methodName};
}

Il2CppGenericContext Il2CppExecutor::getMethodSpecGenericContext(const Il2CppMethodSpec* methodSpec) {
    Il2CppGenericContext ctx;
    if (methodSpec->classIndexIndex != -1) {
        ctx.class_inst = il2Cpp->genericInstPointers[static_cast<size_t>(methodSpec->classIndexIndex)];
    }
    if (methodSpec->methodIndexIndex != -1) {
        ctx.method_inst = il2Cpp->genericInstPointers[static_cast<size_t>(methodSpec->methodIndexIndex)];
    }
    return ctx;
}

std::vector<Il2CppRGCTXDefinition> Il2CppExecutor::getRGCTXDefinition(const std::string& imageName,
                                                                      const Il2CppTypeDefinition& typeDef) {
    if (il2Cpp->version >= 24.2) {
        auto it = il2Cpp->rgctxsDictionary.find(imageName);
        if (it != il2Cpp->rgctxsDictionary.end()) {
            auto it2 = it->second.find(typeDef.token);
            if (it2 != it->second.end()) {
                return it2->second;
            }
        }
        return {};
    }
    if (typeDef.rgctxCount > 0) {
        return std::vector<Il2CppRGCTXDefinition>(
            metadata->rgctxEntries.begin() + typeDef.rgctxStartIndex,
            metadata->rgctxEntries.begin() + typeDef.rgctxStartIndex + typeDef.rgctxCount);
    }
    return {};
}

std::vector<Il2CppRGCTXDefinition> Il2CppExecutor::getRGCTXDefinition(const std::string& imageName,
                                                                      const Il2CppMethodDefinition& methodDef) {
    if (il2Cpp->version >= 24.2) {
        auto it = il2Cpp->rgctxsDictionary.find(imageName);
        if (it != il2Cpp->rgctxsDictionary.end()) {
            auto it2 = it->second.find(methodDef.token);
            if (it2 != it->second.end()) {
                return it2->second;
            }
        }
        return {};
    }
    if (methodDef.rgctxCount > 0) {
        return std::vector<Il2CppRGCTXDefinition>(
            metadata->rgctxEntries.begin() + methodDef.rgctxStartIndex,
            metadata->rgctxEntries.begin() + methodDef.rgctxStartIndex + methodDef.rgctxCount);
    }
    return {};
}

const Il2CppTypeDefinition* Il2CppExecutor::getGenericClassTypeDefinition(const Il2CppGenericClass* genericClass) {
    if (il2Cpp->version >= 27) {
        const Il2CppType* t = il2Cpp->getIl2CppType(genericClass->type);
        if (t == nullptr) {
            return nullptr;
        }
        return getTypeDefinitionFromIl2CppType(t);
    }
    if (genericClass->typeDefinitionIndex == 4294967295 || genericClass->typeDefinitionIndex == -1) {
        return nullptr;
    }
    return &metadata->typeDefs[static_cast<size_t>(genericClass->typeDefinitionIndex)];
}

const Il2CppTypeDefinition* Il2CppExecutor::getTypeDefinitionFromIl2CppType(const Il2CppType* il2CppType) {
    if (il2Cpp->version >= 27 && il2Cpp->isDumped) {
        uint64_t offset = il2CppType->typeHandle() - metadata->imageBase - metadata->header.typeDefinitionsOffset;
        size_t index = static_cast<size_t>(offset / metadata->sizeOfClass<Il2CppTypeDefinition>());
        return &metadata->typeDefs.at(index);
    }
    return &metadata->typeDefs.at(static_cast<size_t>(il2CppType->klassIndex()));
}

const Il2CppGenericParameter* Il2CppExecutor::getGenericParameteFromIl2CppType(const Il2CppType* il2CppType) {
    if (il2Cpp->version >= 27 && il2Cpp->isDumped) {
        uint64_t offset = il2CppType->genericParameterHandle() - metadata->imageBase - metadata->header.genericParametersOffset;
        size_t index = static_cast<size_t>(offset / metadata->sizeOfClass<Il2CppGenericParameter>());
        return &metadata->genericParameters.at(index);
    }
    return &metadata->genericParameters.at(static_cast<size_t>(il2CppType->genericParameterIndex()));
}

SectionHelper Il2CppExecutor::getSectionHelper() {
    int methodCount = 0;
    for (const auto& methodDef : metadata->methodDefs) {
        if (methodDef.methodIndex >= 0) {
            methodCount++;
        }
    }
    return il2Cpp->getSectionHelper(methodCount, static_cast<int>(metadata->typeDefs.size()),
                                    static_cast<int>(metadata->imageDefs.size()));
}

bool Il2CppExecutor::tryGetDefaultValue(int typeIndex, int dataIndex, BlobValue& value) {
    uint32_t pointer = metadata->getDefaultValueFromIndex(dataIndex);
    const Il2CppType& defaultValueType = il2Cpp->types.at(static_cast<size_t>(typeIndex));
    metadata->setPosition(pointer);
    if (getConstantValueFromBlob(*metadata, defaultValueType.type, value)) {
        return true;
    }
    value = BlobValue{};
    value.kind = BlobValue::Kind::U32;
    value.u32 = pointer;
    return false;
}

bool Il2CppExecutor::getConstantValueFromBlob(BinaryStream& reader, int type, BlobValue& value) {
    value = BlobValue{};
    value.il2CppTypeEnum = type;
    switch (type) {
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_BOOLEAN):
            value.kind = BlobValue::Kind::Bool;
            value.b = reader.readU8() != 0;
            return true;
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U1):
            value.kind = BlobValue::Kind::U8;
            value.u8 = reader.readU8();
            return true;
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I1):
            value.kind = BlobValue::Kind::I8;
            value.i8 = reader.readI8();
            return true;
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CHAR): {
            value.kind = BlobValue::Kind::Char;
            auto bytes = reader.readBytes(2);
            value.ch = static_cast<char16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
            return true;
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U2):
            value.kind = BlobValue::Kind::U16;
            value.u16 = reader.readU16();
            return true;
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I2):
            value.kind = BlobValue::Kind::I16;
            value.i16 = reader.readI16();
            return true;
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U4):
            value.kind = BlobValue::Kind::U32;
            value.u32 = il2Cpp->version >= 29 ? reader.readCompressedU32() : reader.readU32();
            return true;
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I4):
            value.kind = BlobValue::Kind::I32;
            value.i32 = il2Cpp->version >= 29 ? reader.readCompressedI32() : reader.readI32();
            return true;
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U8):
            value.kind = BlobValue::Kind::U64;
            value.u64 = reader.readU64();
            return true;
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I8):
            value.kind = BlobValue::Kind::I64;
            value.i64 = reader.readI64();
            return true;
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R4):
            value.kind = BlobValue::Kind::F32;
            value.f32 = reader.readF32();
            return true;
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R8):
            value.kind = BlobValue::Kind::F64;
            value.f64 = reader.readF64();
            return true;
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_STRING): {
            int length;
            if (il2Cpp->version >= 29) {
                length = reader.readCompressedI32();
                if (length == -1) {
                    value.kind = BlobValue::Kind::None;
                } else {
                    auto bytes = reader.readBytes(static_cast<size_t>(length));
                    value.kind = BlobValue::Kind::String;
                    value.str.assign(bytes.begin(), bytes.end());
                }
            } else {
                length = reader.readI32();
                value.kind = BlobValue::Kind::String;
                value.str = readUtf8StringN(reader, length);
            }
            return true;
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_SZARRAY): {
            int32_t arrayLen = reader.readCompressedI32();
            if (arrayLen == -1) {
                value.kind = BlobValue::Kind::None;
                return true;
            }
            value.kind = BlobValue::Kind::Array;
            auto& array = value.array;
            array.resize(static_cast<size_t>(arrayLen));
            uint32_t enumTypeIndex = 0;
            int arrayElementType = readEncodedTypeEnum(reader, enumTypeIndex);
            uint8_t arrayElementsAreDifferent = reader.readU8();
            for (int i = 0; i < arrayLen; i++) {
                int elementType = arrayElementType;
                if (arrayElementsAreDifferent == 1) {
                    elementType = readEncodedTypeEnum(reader, enumTypeIndex);
                }
                BlobValue data;
                if (!getConstantValueFromBlob(reader, elementType, data)) {
                    return false;
                }
                data.il2CppTypeEnum = elementType;
                data.enumTypeIndex = enumTypeIndex;
                array[static_cast<size_t>(i)] = std::move(data);
            }
            return true;
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_IL2CPP_TYPE_INDEX): {
            int32_t typeIndex = reader.readCompressedI32();
            if (typeIndex == -1) {
                value.kind = BlobValue::Kind::None;
            } else {
                value.kind = BlobValue::Kind::TypeIndex;
                value.typeIndex = static_cast<uint32_t>(typeIndex);
            }
            return true;
        }
        default:
            return false;
    }
}

int Il2CppExecutor::readEncodedTypeEnum(BinaryStream& reader, uint32_t& enumTypeIndex) {
    enumTypeIndex = 0;
    int type = reader.readU8();
    if (type == static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_ENUM)) {
        int32_t enumTypeIndex32 = reader.readCompressedI32();
        if (enumTypeIndex32 < 0 || static_cast<size_t>(enumTypeIndex32) >= il2Cpp->types.size()) {
            throw std::out_of_range("readEncodedTypeEnum: enum type index out of range");
        }
        enumTypeIndex = static_cast<uint32_t>(enumTypeIndex32);
        const Il2CppType& enumType = il2Cpp->types[static_cast<size_t>(enumTypeIndex32)];
        const Il2CppTypeDefinition* typeDef = getTypeDefinitionFromIl2CppType(&enumType);
        if (typeDef == nullptr) {
            throw std::runtime_error("readEncodedTypeEnum: enum type definition not found");
        }
        type = il2Cpp->types[static_cast<size_t>(typeDef->enumElementTypeIndex(il2Cpp->version))].type;
    }
    return type;
}

}
