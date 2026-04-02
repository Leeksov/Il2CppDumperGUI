#include "il2cpp_executor.h"
using namespace Il2CppConstants;

std::string ToEscapedString(const std::string& s) {
    std::string re;
    re.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\'': re += "\\'"; break;
            case '"': re += "\\\""; break;
            case '\\': re += "\\\\"; break;
            case '\0': re += "\\0"; break;
            case '\a': re += "\\a"; break;
            case '\b': re += "\\b"; break;
            case '\f': re += "\\f"; break;
            case '\n': re += "\\n"; break;
            case '\r': re += "\\r"; break;
            case '\t': re += "\\t"; break;
            case '\v': re += "\\v"; break;
            default: re += c; break;
        }
    }
    return re;
}

const std::map<int, std::string> Il2CppExecutor::TypeString = {
    {1,"void"},{2,"bool"},{3,"char"},{4,"sbyte"},{5,"byte"},
    {6,"short"},{7,"ushort"},{8,"int"},{9,"uint"},{10,"long"},
    {11,"ulong"},{12,"float"},{13,"double"},{14,"string"},
    {22,"TypedReference"},{24,"IntPtr"},{25,"UIntPtr"},{28,"object"}
};

Il2CppExecutor::Il2CppExecutor(Metadata* meta, Il2Cpp* il2cpp)
    : metadata(meta), il2Cpp(il2cpp)
{
    if (il2Cpp->Version >= 27 && il2Cpp->Version < 29) {
        uint64_t total = 0;
        for (auto& img : metadata->imageDefs) total += img.customAttributeCount;
        customAttributeGenerators.resize(total, 0);
        for (auto& imageDef : metadata->imageDefs) {
            auto imageDefName = metadata->GetStringFromIndex(imageDef.nameIndex);
            auto it = il2Cpp->codeGenModules.find(imageDefName);
            if (it != il2Cpp->codeGenModules.end() && imageDef.customAttributeCount > 0) {
                auto pointers = il2Cpp->MapVATRReadUIntPtrArray(
                    il2Cpp->MapVATR(it->second.customAttributeCacheGenerator),
                    imageDef.customAttributeCount);
                // Actually read from mapped address
                il2Cpp->SetPosition(il2Cpp->MapVATR(it->second.customAttributeCacheGenerator));
                for (uint32_t i = 0; i < imageDef.customAttributeCount; i++) {
                    customAttributeGenerators[imageDef.customAttributeStart + i] = il2Cpp->ReadUIntPtr();
                }
            }
        }
    } else if (il2Cpp->Version < 27) {
        customAttributeGenerators = il2Cpp->customAttributeGenerators;
    }
}

std::string Il2CppExecutor::GetTypeName(const Il2CppType& il2CppType, bool addNamespace, bool is_nested) {
    switch (il2CppType.type) {
        case Il2CppTypeEnum::IL2CPP_TYPE_ARRAY: {
            auto arrayType = il2Cpp->MapVATRRead<Il2CppArrayType>(il2CppType.data_dummy, &BinaryStream::ReadArrayType);
            auto elementType = il2Cpp->GetIl2CppType(arrayType.etype);
            if (!elementType) return "object[]";
            return GetTypeName(*elementType, addNamespace, false) + "[" + std::string(arrayType.rank > 1 ? arrayType.rank - 1 : 0, ',') + "]";
        }
        case Il2CppTypeEnum::IL2CPP_TYPE_SZARRAY: {
            auto elementType = il2Cpp->GetIl2CppType(il2CppType.data_dummy);
            if (!elementType) return "object[]";
            return GetTypeName(*elementType, addNamespace, false) + "[]";
        }
        case Il2CppTypeEnum::IL2CPP_TYPE_PTR: {
            auto oriType = il2Cpp->GetIl2CppType(il2CppType.data_dummy);
            if (!oriType) return "void*";
            return GetTypeName(*oriType, addNamespace, false) + "*";
        }
        case Il2CppTypeEnum::IL2CPP_TYPE_VAR:
        case Il2CppTypeEnum::IL2CPP_TYPE_MVAR: {
            auto param = GetGenericParameteFromIl2CppType(il2CppType);
            if (!param) return "T";
            return metadata->GetStringFromIndex(param->nameIndex);
        }
        case Il2CppTypeEnum::IL2CPP_TYPE_CLASS:
        case Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE:
        case Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST: {
            std::string str;
            Il2CppTypeDefinition* typeDef = nullptr;
            Il2CppGenericClass genericClass;
            bool hasGenericClass = false;

            if (il2CppType.type == Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST) {
                il2Cpp->SetPosition(il2Cpp->MapVATR(il2CppType.data_dummy));
                genericClass = il2Cpp->ReadGenericClass();
                hasGenericClass = true;
                typeDef = GetGenericClassTypeDefinition(genericClass);
            } else {
                typeDef = GetTypeDefinitionFromIl2CppType(il2CppType);
            }
            if (!typeDef) return "object";

            if (typeDef->declaringTypeIndex != -1) {
                str += GetTypeName(il2Cpp->types[typeDef->declaringTypeIndex], addNamespace, true);
                str += '.';
            } else if (addNamespace) {
                auto ns = metadata->GetStringFromIndex(typeDef->namespaceIndex);
                if (!ns.empty()) str += ns + ".";
            }

            auto typeName = metadata->GetStringFromIndex(typeDef->nameIndex);
            auto idx = typeName.find('`');
            if (idx != std::string::npos) str += typeName.substr(0, idx);
            else str += typeName;

            if (is_nested) return str;

            if (hasGenericClass) {
                il2Cpp->SetPosition(il2Cpp->MapVATR(genericClass.class_inst));
                auto genericInst = il2Cpp->ReadGenericInst();
                str += GetGenericInstParams(genericInst);
            } else if (typeDef->genericContainerIndex >= 0) {
                str += GetGenericContainerParams(metadata->genericContainers[typeDef->genericContainerIndex]);
            }
            return str;
        }
        default: {
            auto it = TypeString.find((int)il2CppType.type);
            if (it != TypeString.end()) return it->second;
            return "unknown";
        }
    }
}

std::string Il2CppExecutor::GetTypeDefName(const Il2CppTypeDefinition& typeDef, bool addNamespace, bool genericParameter) {
    std::string prefix;
    if (typeDef.declaringTypeIndex != -1) {
        prefix = GetTypeName(il2Cpp->types[typeDef.declaringTypeIndex], addNamespace, true) + ".";
    } else if (addNamespace) {
        auto ns = metadata->GetStringFromIndex(typeDef.namespaceIndex);
        if (!ns.empty()) prefix = ns + ".";
    }
    auto typeName = metadata->GetStringFromIndex(typeDef.nameIndex);
    if (typeDef.genericContainerIndex >= 0) {
        auto idx = typeName.find('`');
        if (idx != std::string::npos) typeName = typeName.substr(0, idx);
        if (genericParameter) {
            typeName += GetGenericContainerParams(metadata->genericContainers[typeDef.genericContainerIndex]);
        }
    }
    return prefix + typeName;
}

std::string Il2CppExecutor::GetGenericInstParams(const Il2CppGenericInst& genericInst) {
    std::vector<std::string> names;
    auto pointers = il2Cpp->MapVATRReadUIntPtrArray(genericInst.type_argv, genericInst.type_argc);
    for (int i = 0; i < genericInst.type_argc; i++) {
        auto t = il2Cpp->GetIl2CppType(pointers[i]);
        if (t) names.push_back(GetTypeName(*t, false, false));
        else names.push_back("?");
    }
    std::string result = "<";
    for (size_t i = 0; i < names.size(); i++) {
        if (i > 0) result += ", ";
        result += names[i];
    }
    return result + ">";
}

std::string Il2CppExecutor::GetGenericContainerParams(const Il2CppGenericContainer& gc) {
    std::vector<std::string> names;
    for (int i = 0; i < gc.type_argc; i++) {
        auto& gp = metadata->genericParameters[gc.genericParameterStart + i];
        names.push_back(metadata->GetStringFromIndex(gp.nameIndex));
    }
    std::string result = "<";
    for (size_t i = 0; i < names.size(); i++) {
        if (i > 0) result += ", ";
        result += names[i];
    }
    return result + ">";
}

Il2CppTypeDefinition* Il2CppExecutor::GetGenericClassTypeDefinition(const Il2CppGenericClass& gc) {
    if (il2Cpp->Version >= 27) {
        auto t = il2Cpp->GetIl2CppType(gc.type);
        if (!t) return nullptr;
        return GetTypeDefinitionFromIl2CppType(*t);
    }
    if (gc.typeDefinitionIndex == 4294967295LL || gc.typeDefinitionIndex == -1) return nullptr;
    return &metadata->typeDefs[gc.typeDefinitionIndex];
}

Il2CppTypeDefinition* Il2CppExecutor::GetTypeDefinitionFromIl2CppType(const Il2CppType& t) {
    if (il2Cpp->Version >= 27 && il2Cpp->IsDumped) {
        auto offset = t.data_dummy - metadata->ImageBase - metadata->header.typeDefinitionsOffset;
        auto index = offset / (uint64_t)metadata->SizeOfTypeDef();
        if (index < metadata->typeDefs.size()) return &metadata->typeDefs[index];
        return nullptr;
    }
    auto idx = t.klassIndex();
    if (idx >= 0 && idx < (int64_t)metadata->typeDefs.size()) return &metadata->typeDefs[idx];
    return nullptr;
}

Il2CppGenericParameter* Il2CppExecutor::GetGenericParameteFromIl2CppType(const Il2CppType& t) {
    if (il2Cpp->Version >= 27 && il2Cpp->IsDumped) {
        auto offset = t.data_dummy - metadata->ImageBase - metadata->header.genericParametersOffset;
        auto index = offset / (uint64_t)metadata->SizeOfGenericParameter();
        if (index < metadata->genericParameters.size()) return &metadata->genericParameters[index];
        return nullptr;
    }
    auto idx = t.genericParameterIndex();
    if (idx >= 0 && idx < (int64_t)metadata->genericParameters.size()) return &metadata->genericParameters[idx];
    return nullptr;
}

std::pair<std::string, std::string> Il2CppExecutor::GetMethodSpecName(const Il2CppMethodSpec& ms, bool addNamespace) {
    auto& methodDef = metadata->methodDefs[ms.methodDefinitionIndex];
    auto& typeDef = metadata->typeDefs[methodDef.declaringType];
    auto typeName = GetTypeDefName(typeDef, addNamespace, false);
    if (ms.classIndexIndex != -1) {
        typeName += GetGenericInstParams(il2Cpp->genericInsts[ms.classIndexIndex]);
    }
    auto methodName = metadata->GetStringFromIndex(methodDef.nameIndex);
    if (ms.methodIndexIndex != -1) {
        methodName += GetGenericInstParams(il2Cpp->genericInsts[ms.methodIndexIndex]);
    }
    return { typeName, methodName };
}

bool Il2CppExecutor::TryGetDefaultValue(int typeIndex, int dataIndex, std::string& valueStr) {
    auto pointer = metadata->GetDefaultValueFromIndex(dataIndex);
    auto& defaultValueType = il2Cpp->types[typeIndex];
    metadata->SetPosition(pointer);

    switch (defaultValueType.type) {
        case Il2CppTypeEnum::IL2CPP_TYPE_BOOLEAN:
            valueStr = metadata->ReadBoolean() ? "true" : "false"; return true;
        case Il2CppTypeEnum::IL2CPP_TYPE_U1:
            valueStr = std::to_string(metadata->ReadByte()); return true;
        case Il2CppTypeEnum::IL2CPP_TYPE_I1:
            valueStr = std::to_string(metadata->ReadSByte()); return true;
        case Il2CppTypeEnum::IL2CPP_TYPE_CHAR: {
            auto bytes = metadata->ReadBytes(2);
            int v = bytes[0] | (bytes[1] << 8);
            char buf[16]; snprintf(buf, sizeof(buf), "'\\x%x'", v);
            valueStr = buf; return true;
        }
        case Il2CppTypeEnum::IL2CPP_TYPE_U2:
            valueStr = std::to_string(metadata->ReadUInt16()); return true;
        case Il2CppTypeEnum::IL2CPP_TYPE_I2:
            valueStr = std::to_string(metadata->ReadInt16()); return true;
        case Il2CppTypeEnum::IL2CPP_TYPE_U4:
            valueStr = std::to_string(il2Cpp->Version >= 29 ? metadata->ReadCompressedUInt32() : metadata->ReadUInt32()); return true;
        case Il2CppTypeEnum::IL2CPP_TYPE_I4:
            valueStr = std::to_string(il2Cpp->Version >= 29 ? metadata->ReadCompressedInt32() : metadata->ReadInt32()); return true;
        case Il2CppTypeEnum::IL2CPP_TYPE_U8:
            valueStr = std::to_string(metadata->ReadUInt64()); return true;
        case Il2CppTypeEnum::IL2CPP_TYPE_I8:
            valueStr = std::to_string(metadata->ReadInt64()); return true;
        case Il2CppTypeEnum::IL2CPP_TYPE_R4: {
            char buf[64]; snprintf(buf, sizeof(buf), "%g", metadata->ReadSingle());
            valueStr = buf; return true;
        }
        case Il2CppTypeEnum::IL2CPP_TYPE_R8: {
            char buf[64]; snprintf(buf, sizeof(buf), "%g", metadata->ReadDouble());
            valueStr = buf; return true;
        }
        case Il2CppTypeEnum::IL2CPP_TYPE_STRING: {
            if (il2Cpp->Version >= 29) {
                auto len = metadata->ReadCompressedInt32();
                if (len == -1) { valueStr = "null"; return true; }
                auto bytes = metadata->ReadBytes(len);
                valueStr = "\"" + ToEscapedString(std::string(bytes.begin(), bytes.end())) + "\"";
            } else {
                auto len = metadata->ReadInt32();
                auto bytes = metadata->ReadBytes(len);
                valueStr = "\"" + ToEscapedString(std::string(bytes.begin(), bytes.end())) + "\"";
            }
            return true;
        }
        default:
            valueStr = "";
            return false;
    }
}

// ============================================================
// Il2CppDecompiler
// ============================================================
Il2CppDecompiler::Il2CppDecompiler(Il2CppExecutor* executor)
    : executor_(executor), metadata_(executor->metadata), il2Cpp_(executor->il2Cpp) {}

std::string Il2CppDecompiler::GetModifiers(int methodDefIndex) {
    auto it = methodModifiers_.find(methodDefIndex);
    if (it != methodModifiers_.end()) return it->second;

    auto& methodDef = metadata_->methodDefs[methodDefIndex];
    std::string str;
    auto access = methodDef.flags & METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK;
    switch (access) {
        case METHOD_ATTRIBUTE_PRIVATE: str += "private "; break;
        case METHOD_ATTRIBUTE_PUBLIC: str += "public "; break;
        case METHOD_ATTRIBUTE_FAMILY: str += "protected "; break;
        case METHOD_ATTRIBUTE_ASSEM: case METHOD_ATTRIBUTE_FAM_AND_ASSEM: str += "internal "; break;
        case METHOD_ATTRIBUTE_FAM_OR_ASSEM: str += "protected internal "; break;
    }
    if ((methodDef.flags & METHOD_ATTRIBUTE_STATIC) != 0) str += "static ";
    if ((methodDef.flags & METHOD_ATTRIBUTE_ABSTRACT) != 0) {
        str += "abstract ";
        if ((methodDef.flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) str += "override ";
    } else if ((methodDef.flags & METHOD_ATTRIBUTE_FINAL) != 0) {
        if ((methodDef.flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) str += "sealed override ";
    } else if ((methodDef.flags & METHOD_ATTRIBUTE_VIRTUAL) != 0) {
        if ((methodDef.flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_NEW_SLOT) str += "virtual ";
        else str += "override ";
    }
    if ((methodDef.flags & METHOD_ATTRIBUTE_PINVOKE_IMPL) != 0) str += "extern ";
    methodModifiers_[methodDefIndex] = str;
    return str;
}

std::string Il2CppDecompiler::GetCustomAttribute(int imageIndex, int customAttributeIndex, uint32_t token, const std::string& padding) {
    if (il2Cpp_->Version < 21) return "";
    auto attributeIndex = metadata_->GetCustomAttributeIndex(imageIndex, customAttributeIndex, token);
    if (attributeIndex >= 0) {
        if (il2Cpp_->Version < 29) {
            if (attributeIndex >= (int)metadata_->attributeTypeRanges.size()) return "";
            auto& atr = metadata_->attributeTypeRanges[attributeIndex];
            std::string sb;
            for (int i = 0; i < atr.count; i++) {
                auto typeIdx = metadata_->attributeTypes[atr.start + i];
                if (typeIdx >= 0 && typeIdx < (int)il2Cpp_->types.size()) {
                    sb += padding + "[" + executor_->GetTypeName(il2Cpp_->types[typeIdx], false, false) + "]\n";
                }
            }
            return sb;
        }
    }
    return "";
}

std::string Il2CppDecompiler::Decompile(const Il2CppDumperConfig& config) {
    std::ostringstream out;

    // Dump image list
    for (int imageIndex = 0; imageIndex < (int)metadata_->imageDefs.size(); imageIndex++) {
        auto& imageDef = metadata_->imageDefs[imageIndex];
        out << "// Image " << imageIndex << ": " << metadata_->GetStringFromIndex(imageDef.nameIndex) << " - " << imageDef.typeStart << "\n";
    }

    // Dump types
    for (int imageIndex = 0; imageIndex < (int)metadata_->imageDefs.size(); imageIndex++) {
        auto& imageDef = metadata_->imageDefs[imageIndex];
        try {
            auto imageName = metadata_->GetStringFromIndex(imageDef.nameIndex);
            auto typeEnd = imageDef.typeStart + (int)imageDef.typeCount;
            for (int typeDefIndex = imageDef.typeStart; typeDefIndex < typeEnd; typeDefIndex++) {
                auto& typeDef = metadata_->typeDefs[typeDefIndex];

                // Extends
                std::vector<std::string> extends;
                if (typeDef.parentIndex >= 0 && typeDef.parentIndex < (int)il2Cpp_->types.size()) {
                    auto parentName = executor_->GetTypeName(il2Cpp_->types[typeDef.parentIndex], false, false);
                    if (!typeDef.IsValueType() && !typeDef.IsEnum() && parentName != "object") {
                        extends.push_back(parentName);
                    }
                }
                if (typeDef.interfaces_count > 0) {
                    for (int i = 0; i < typeDef.interfaces_count; i++) {
                        auto ifIdx = metadata_->interfaceIndices[typeDef.interfacesStart + i];
                        if (ifIdx >= 0 && ifIdx < (int)il2Cpp_->types.size()) {
                            extends.push_back(executor_->GetTypeName(il2Cpp_->types[ifIdx], false, false));
                        }
                    }
                }

                out << "\n// Namespace: " << metadata_->GetStringFromIndex(typeDef.namespaceIndex) << "\n";

                if (config.DumpAttribute) {
                    out << GetCustomAttribute(imageIndex, typeDef.customAttributeIndex, typeDef.token);
                }
                if (config.DumpAttribute && (typeDef.flags & TYPE_ATTRIBUTE_SERIALIZABLE) != 0)
                    out << "[Serializable]\n";

                auto visibility = typeDef.flags & TYPE_ATTRIBUTE_VISIBILITY_MASK;
                switch (visibility) {
                    case TYPE_ATTRIBUTE_PUBLIC: case TYPE_ATTRIBUTE_NESTED_PUBLIC: out << "public "; break;
                    case TYPE_ATTRIBUTE_NOT_PUBLIC: case TYPE_ATTRIBUTE_NESTED_FAM_AND_ASSEM: case TYPE_ATTRIBUTE_NESTED_ASSEMBLY: out << "internal "; break;
                    case TYPE_ATTRIBUTE_NESTED_PRIVATE: out << "private "; break;
                    case TYPE_ATTRIBUTE_NESTED_FAMILY: out << "protected "; break;
                    case TYPE_ATTRIBUTE_NESTED_FAM_OR_ASSEM: out << "protected internal "; break;
                }

                if ((typeDef.flags & TYPE_ATTRIBUTE_ABSTRACT) != 0 && (typeDef.flags & TYPE_ATTRIBUTE_SEALED) != 0) out << "static ";
                else if ((typeDef.flags & TYPE_ATTRIBUTE_INTERFACE) == 0 && (typeDef.flags & TYPE_ATTRIBUTE_ABSTRACT) != 0) out << "abstract ";
                else if (!typeDef.IsValueType() && !typeDef.IsEnum() && (typeDef.flags & TYPE_ATTRIBUTE_SEALED) != 0) out << "sealed ";

                if ((typeDef.flags & TYPE_ATTRIBUTE_INTERFACE) != 0) out << "interface ";
                else if (typeDef.IsEnum()) out << "enum ";
                else if (typeDef.IsValueType()) out << "struct ";
                else out << "class ";

                out << executor_->GetTypeDefName(typeDef, false, true);
                if (!extends.empty()) {
                    out << " : ";
                    for (size_t i = 0; i < extends.size(); i++) {
                        if (i > 0) out << ", ";
                        out << extends[i];
                    }
                }
                if (config.DumpTypeDefIndex) out << " // TypeDefIndex: " << typeDefIndex << "\n{";
                else out << "\n{";

                // Fields
                if (config.DumpField && typeDef.field_count > 0) {
                    out << "\n\t// Fields\n";
                    auto fieldEnd = typeDef.fieldStart + typeDef.field_count;
                    for (int i = typeDef.fieldStart; i < fieldEnd; i++) {
                        auto& fieldDef = metadata_->fieldDefs[i];
                        auto& fieldType = il2Cpp_->types[fieldDef.typeIndex];
                        bool isStatic = false, isConst = false;

                        if (config.DumpAttribute) {
                            out << GetCustomAttribute(imageIndex, fieldDef.customAttributeIndex, fieldDef.token, "\t");
                        }
                        out << "\t";
                        auto access = fieldType.attrs & FIELD_ATTRIBUTE_FIELD_ACCESS_MASK;
                        switch (access) {
                            case FIELD_ATTRIBUTE_PRIVATE: out << "private "; break;
                            case FIELD_ATTRIBUTE_PUBLIC: out << "public "; break;
                            case FIELD_ATTRIBUTE_FAMILY: out << "protected "; break;
                            case FIELD_ATTRIBUTE_ASSEMBLY: case FIELD_ATTRIBUTE_FAM_AND_ASSEM: out << "internal "; break;
                            case FIELD_ATTRIBUTE_FAM_OR_ASSEM: out << "protected internal "; break;
                        }
                        if ((fieldType.attrs & FIELD_ATTRIBUTE_LITERAL) != 0) { isConst = true; out << "const "; }
                        else {
                            if ((fieldType.attrs & FIELD_ATTRIBUTE_STATIC) != 0) { isStatic = true; out << "static "; }
                            if ((fieldType.attrs & FIELD_ATTRIBUTE_INIT_ONLY) != 0) out << "readonly ";
                        }
                        out << executor_->GetTypeName(fieldType, false, false) << " " << metadata_->GetStringFromIndex(fieldDef.nameIndex);

                        Il2CppFieldDefaultValue fdv;
                        if (metadata_->GetFieldDefaultValueFromIndex(i, fdv) && fdv.dataIndex != -1) {
                            std::string valStr;
                            if (executor_->TryGetDefaultValue(fdv.typeIndex, fdv.dataIndex, valStr)) {
                                out << " = " << valStr;
                            }
                        }

                        if (config.DumpFieldOffset && !isConst) {
                            char buf[32];
                            snprintf(buf, sizeof(buf), "; // 0x%X\n", il2Cpp_->GetFieldOffsetFromIndex(typeDefIndex, i - typeDef.fieldStart, i, typeDef.IsValueType(), isStatic));
                            out << buf;
                        } else {
                            out << ";\n";
                        }
                    }
                }

                // Properties
                if (config.DumpProperty && typeDef.property_count > 0) {
                    out << "\n\t// Properties\n";
                    auto propEnd = typeDef.propertyStart + typeDef.property_count;
                    for (int i = typeDef.propertyStart; i < propEnd; i++) {
                        auto& propDef = metadata_->propertyDefs[i];
                        out << "\t";
                        if (propDef.get >= 0) {
                            auto& methodDef = metadata_->methodDefs[typeDef.methodStart + propDef.get];
                            out << GetModifiers(typeDef.methodStart + propDef.get);
                            out << executor_->GetTypeName(il2Cpp_->types[methodDef.returnType], false, false);
                        } else if (propDef.set >= 0) {
                            auto& methodDef = metadata_->methodDefs[typeDef.methodStart + propDef.set];
                            out << GetModifiers(typeDef.methodStart + propDef.set);
                            auto& paramDef = metadata_->parameterDefs[methodDef.parameterStart];
                            out << executor_->GetTypeName(il2Cpp_->types[paramDef.typeIndex], false, false);
                        }
                        out << " " << metadata_->GetStringFromIndex(propDef.nameIndex) << " { ";
                        if (propDef.get >= 0) out << "get; ";
                        if (propDef.set >= 0) out << "set; ";
                        out << "}\n";
                    }
                }

                // Methods
                if (config.DumpMethod && typeDef.method_count > 0) {
                    out << "\n\t// Methods\n";
                    auto methodEnd = typeDef.methodStart + typeDef.method_count;
                    for (int i = typeDef.methodStart; i < methodEnd; i++) {
                        out << "\n";
                        auto& methodDef = metadata_->methodDefs[i];
                        bool isAbstract = (methodDef.flags & METHOD_ATTRIBUTE_ABSTRACT) != 0;

                        if (config.DumpAttribute) {
                            out << GetCustomAttribute(imageIndex, methodDef.customAttributeIndex, methodDef.token, "\t");
                        }

                        if (config.DumpMethodOffset) {
                            auto methodPointer = il2Cpp_->GetMethodPointer(imageName, methodDef);
                            if (!isAbstract && methodPointer > 0) {
                                char buf[128];
                                snprintf(buf, sizeof(buf), "\t// RVA: 0x%llX Offset: 0x%llX VA: 0x%llX",
                                    (unsigned long long)il2Cpp_->GetRVA(methodPointer),
                                    (unsigned long long)il2Cpp_->MapVATR(methodPointer),
                                    (unsigned long long)methodPointer);
                                out << buf;
                            } else {
                                out << "\t// RVA: -1 Offset: -1";
                            }
                            if (methodDef.slot != 0xFFFF) out << " Slot: " << methodDef.slot;
                            out << "\n";
                        }

                        out << "\t" << GetModifiers(i);
                        auto& methodReturnType = il2Cpp_->types[methodDef.returnType];
                        auto methodName = metadata_->GetStringFromIndex(methodDef.nameIndex);
                        if (methodDef.genericContainerIndex >= 0) {
                            methodName += executor_->GetGenericContainerParams(metadata_->genericContainers[methodDef.genericContainerIndex]);
                        }
                        if (methodReturnType.byref == 1) out << "ref ";
                        out << executor_->GetTypeName(methodReturnType, false, false) << " " << methodName << "(";

                        // Parameters
                        for (int j = 0; j < methodDef.parameterCount; j++) {
                            if (j > 0) out << ", ";
                            auto& paramDef = metadata_->parameterDefs[methodDef.parameterStart + j];
                            auto& paramType = il2Cpp_->types[paramDef.typeIndex];
                            if (paramType.byref == 1) {
                                if ((paramType.attrs & PARAM_ATTRIBUTE_OUT) != 0 && (paramType.attrs & PARAM_ATTRIBUTE_IN) == 0) out << "out ";
                                else if ((paramType.attrs & PARAM_ATTRIBUTE_OUT) == 0 && (paramType.attrs & PARAM_ATTRIBUTE_IN) != 0) out << "in ";
                                else out << "ref ";
                            } else {
                                if ((paramType.attrs & PARAM_ATTRIBUTE_IN) != 0) out << "[In] ";
                                if ((paramType.attrs & PARAM_ATTRIBUTE_OUT) != 0) out << "[Out] ";
                            }
                            out << executor_->GetTypeName(paramType, false, false) << " " << metadata_->GetStringFromIndex(paramDef.nameIndex);

                            Il2CppParameterDefaultValue pdv;
                            if (metadata_->GetParameterDefaultValueFromIndex(methodDef.parameterStart + j, pdv) && pdv.dataIndex != -1) {
                                std::string valStr;
                                if (executor_->TryGetDefaultValue(pdv.typeIndex, pdv.dataIndex, valStr)) {
                                    out << " = " << valStr;
                                }
                            }
                        }

                        if (isAbstract) out << ");\n";
                        else out << ") { }\n";

                        // Generic method specs
                        auto specIt = il2Cpp_->methodDefinitionMethodSpecs.find(i);
                        if (specIt != il2Cpp_->methodDefinitionMethodSpecs.end()) {
                            out << "\t/* GenericInstMethod :\n";
                            for (auto& ms : specIt->second) {
                                out << "\t|\n";
                                // Find pointer for this spec
                                auto ptrIt = il2Cpp_->methodSpecGenericMethodPointers.find(
                                    (int)(&ms - &specIt->second[0]) /* not exact, simplified */);
                                auto [tName, mName] = executor_->GetMethodSpecName(ms);
                                out << "\t|-" << tName << "." << mName << "\n";
                            }
                            out << "\t*/\n";
                        }
                    }
                }

                out << "}\n";
            }
        } catch (std::exception& e) {
            out << "/*" << e.what() << "*/\n}\n";
        }
    }

    return out.str();
}
