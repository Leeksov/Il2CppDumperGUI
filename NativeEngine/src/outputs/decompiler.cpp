#include "outputs/decompiler.h"

#include "utils/custom_attribute_data_reader.h"
#include "outputs/il2cpp_constants.h"
#include "utils/string_utils.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace il2cppdumper {

using namespace Il2CppConstants;

namespace {

std::string hexU64(uint64_t v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%llX", static_cast<unsigned long long>(v));
    return buf;
}

std::string hexI32(int32_t v) {
    return hexU64(static_cast<uint32_t>(v));
}

std::string hexI32Lower(int32_t v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%x", static_cast<uint32_t>(v));
    return buf;
}

std::string blobToString(const BlobValue& value) {
    switch (value.kind) {
        case BlobValue::Kind::Bool:
            return value.b ? "True" : "False";
        case BlobValue::Kind::I8:
            return std::to_string(value.i8);
        case BlobValue::Kind::U8:
            return std::to_string(value.u8);
        case BlobValue::Kind::I16:
            return std::to_string(value.i16);
        case BlobValue::Kind::U16:
            return std::to_string(value.u16);
        case BlobValue::Kind::I32:
            return std::to_string(value.i32);
        case BlobValue::Kind::U32:
            return std::to_string(value.u32);
        case BlobValue::Kind::I64:
            return std::to_string(value.i64);
        case BlobValue::Kind::U64:
            return std::to_string(value.u64);
        case BlobValue::Kind::F32:
            return netNumberFormat(value.f32, 9);
        case BlobValue::Kind::F64:
            return netNumberFormat(value.f64, 17);
        default:
            return "";
    }
}

}

Il2CppDecompiler::Il2CppDecompiler(Il2CppExecutor& executor)
    : executor(executor), metadata(executor.metadata), il2Cpp(executor.il2Cpp) {}

void Il2CppDecompiler::decompile(const Config& config, const std::string& outputDir) {
    std::ofstream writer(outputDir + "dump.cs", std::ios::binary | std::ios::trunc);
    for (size_t imageIndex = 0; imageIndex < metadata->imageDefs.size(); imageIndex++) {
        const auto& imageDef = metadata->imageDefs[imageIndex];
        writer << "// Image " << imageIndex << ": " << metadata->getStringFromIndex(imageDef.nameIndex) << " - "
               << imageDef.typeStart << "\n";
    }
    for (size_t imageIdx = 0; imageIdx < metadata->imageDefs.size(); imageIdx++) {
        const auto& imageDef = metadata->imageDefs[imageIdx];
        try {
            std::string imageName = metadata->getStringFromIndex(imageDef.nameIndex);
            int typeEnd = imageDef.typeStart + static_cast<int>(imageDef.typeCount);
            for (int typeDefIndex = imageDef.typeStart; typeDefIndex < typeEnd; typeDefIndex++) {
                const auto& typeDef = metadata->typeDefs[static_cast<size_t>(typeDefIndex)];
                std::vector<std::string> extends;
                if (typeDef.parentIndex >= 0) {
                    const Il2CppType& parent = il2Cpp->types[static_cast<size_t>(typeDef.parentIndex)];
                    std::string parentName = executor.getTypeName(&parent, false, false);
                    if (!typeDef.isValueType() && !typeDef.isEnum() && parentName != "object") {
                        extends.push_back(parentName);
                    }
                }
                if (typeDef.interfaces_count > 0) {
                    for (int i = 0; i < typeDef.interfaces_count; i++) {
                        const Il2CppType& interfaceType =
                            il2Cpp->types[static_cast<size_t>(metadata->interfaceIndices[static_cast<size_t>(typeDef.interfacesStart + i)])];
                        extends.push_back(executor.getTypeName(&interfaceType, false, false));
                    }
                }
                writer << "\n// Namespace: " << metadata->getStringFromIndex(typeDef.namespaceIndex) << "\n";
                if (config.DumpAttribute) {
                    writer << getCustomAttribute(static_cast<int>(imageIdx), typeDef.customAttributeIndex, typeDef.token);
                }
                if (config.DumpAttribute && (typeDef.flags & TYPE_ATTRIBUTE_SERIALIZABLE) != 0) {
                    writer << "[Serializable]\n";
                }
                uint32_t visibility = typeDef.flags & TYPE_ATTRIBUTE_VISIBILITY_MASK;
                switch (visibility) {
                    case TYPE_ATTRIBUTE_PUBLIC:
                    case TYPE_ATTRIBUTE_NESTED_PUBLIC:
                        writer << "public ";
                        break;
                    case TYPE_ATTRIBUTE_NOT_PUBLIC:
                    case TYPE_ATTRIBUTE_NESTED_FAM_AND_ASSEM:
                    case TYPE_ATTRIBUTE_NESTED_ASSEMBLY:
                        writer << "internal ";
                        break;
                    case TYPE_ATTRIBUTE_NESTED_PRIVATE:
                        writer << "private ";
                        break;
                    case TYPE_ATTRIBUTE_NESTED_FAMILY:
                        writer << "protected ";
                        break;
                    case TYPE_ATTRIBUTE_NESTED_FAM_OR_ASSEM:
                        writer << "protected internal ";
                        break;
                }
                if ((typeDef.flags & TYPE_ATTRIBUTE_ABSTRACT) != 0 && (typeDef.flags & TYPE_ATTRIBUTE_SEALED) != 0) {
                    writer << "static ";
                } else if ((typeDef.flags & TYPE_ATTRIBUTE_INTERFACE) == 0 && (typeDef.flags & TYPE_ATTRIBUTE_ABSTRACT) != 0) {
                    writer << "abstract ";
                } else if (!typeDef.isValueType() && !typeDef.isEnum() && (typeDef.flags & TYPE_ATTRIBUTE_SEALED) != 0) {
                    writer << "sealed ";
                }
                if ((typeDef.flags & TYPE_ATTRIBUTE_INTERFACE) != 0) {
                    writer << "interface ";
                } else if (typeDef.isEnum()) {
                    writer << "enum ";
                } else if (typeDef.isValueType()) {
                    writer << "struct ";
                } else {
                    writer << "class ";
                }
                std::string typeName = executor.getTypeDefName(&typeDef, false, true);
                writer << typeName;
                if (!extends.empty()) {
                    writer << " : ";
                    for (size_t i = 0; i < extends.size(); i++) {
                        if (i != 0) {
                            writer << ", ";
                        }
                        writer << extends[i];
                    }
                }
                if (config.DumpTypeDefIndex) {
                    writer << " // TypeDefIndex: " << typeDefIndex << "\n{";
                } else {
                    writer << "\n{";
                }
                if (config.DumpField && typeDef.field_count > 0) {
                    writer << "\n\t// Fields\n";
                    int fieldEnd = typeDef.fieldStart + typeDef.field_count;
                    for (int i = typeDef.fieldStart; i < fieldEnd; ++i) {
                        const auto& fieldDef = metadata->fieldDefs[static_cast<size_t>(i)];
                        const Il2CppType& fieldType = il2Cpp->types[static_cast<size_t>(fieldDef.typeIndex)];
                        bool isStatic = false;
                        bool isConst = false;
                        if (config.DumpAttribute) {
                            writer << getCustomAttribute(static_cast<int>(imageIdx), fieldDef.customAttributeIndex,
                                                         fieldDef.token, "\t");
                        }
                        writer << "\t";
                        uint32_t access = fieldType.attrs & FIELD_ATTRIBUTE_FIELD_ACCESS_MASK;
                        switch (access) {
                            case FIELD_ATTRIBUTE_PRIVATE:
                                writer << "private ";
                                break;
                            case FIELD_ATTRIBUTE_PUBLIC:
                                writer << "public ";
                                break;
                            case FIELD_ATTRIBUTE_FAMILY:
                                writer << "protected ";
                                break;
                            case FIELD_ATTRIBUTE_ASSEMBLY:
                            case FIELD_ATTRIBUTE_FAM_AND_ASSEM:
                                writer << "internal ";
                                break;
                            case FIELD_ATTRIBUTE_FAM_OR_ASSEM:
                                writer << "protected internal ";
                                break;
                        }
                        if ((fieldType.attrs & FIELD_ATTRIBUTE_LITERAL) != 0) {
                            isConst = true;
                            writer << "const ";
                        } else {
                            if ((fieldType.attrs & FIELD_ATTRIBUTE_STATIC) != 0) {
                                isStatic = true;
                                writer << "static ";
                            }
                            if ((fieldType.attrs & FIELD_ATTRIBUTE_INIT_ONLY) != 0) {
                                writer << "readonly ";
                            }
                        }
                        writer << executor.getTypeName(&fieldType, false, false) << " "
                               << metadata->getStringFromIndex(fieldDef.nameIndex);
                        Il2CppFieldDefaultValue fieldDefaultValue;
                        if (metadata->getFieldDefaultValueFromIndex(i, fieldDefaultValue) &&
                            fieldDefaultValue.dataIndex != -1) {
                            BlobValue value;
                            if (executor.tryGetDefaultValue(fieldDefaultValue.typeIndex, fieldDefaultValue.dataIndex,
                                                            value)) {
                                writer << " = ";
                                if (value.kind == BlobValue::Kind::String) {
                                    writer << "\"" << toEscapedString(value.str) << "\"";
                                } else if (value.kind == BlobValue::Kind::Char) {
                                    writer << "'\\x" << hexI32Lower(static_cast<int32_t>(value.ch)) << "'";
                                } else if (value.kind != BlobValue::Kind::None) {
                                    writer << blobToString(value);
                                } else {
                                    writer << "null";
                                }
                            } else {
                                writer << " /*Metadata offset 0x" << hexU64(value.u32) << "*/";
                            }
                        }
                        if (config.DumpFieldOffset && !isConst) {
                            writer << "; // 0x"
                                   << hexI32(il2Cpp->getFieldOffsetFromIndex(typeDefIndex, i - typeDef.fieldStart, i,
                                                                             typeDef.isValueType(), isStatic))
                                   << "\n";
                        } else {
                            writer << ";\n";
                        }
                    }
                }
                if (config.DumpProperty && typeDef.property_count > 0) {
                    writer << "\n\t// Properties\n";
                    int propertyEnd = typeDef.propertyStart + typeDef.property_count;
                    for (int i = typeDef.propertyStart; i < propertyEnd; ++i) {
                        const auto& propertyDef = metadata->propertyDefs[static_cast<size_t>(i)];
                        if (config.DumpAttribute) {
                            writer << getCustomAttribute(static_cast<int>(imageIdx), propertyDef.customAttributeIndex,
                                                         propertyDef.token, "\t");
                        }
                        writer << "\t";
                        if (propertyDef.get >= 0) {
                            const auto& methodDef =
                                metadata->methodDefs[static_cast<size_t>(metadata->getMethodIndexFromTypeDefinition(typeDefIndex, propertyDef.get))];
                            writer << getModifiers(&methodDef);
                            const Il2CppType& propertyType = il2Cpp->types[static_cast<size_t>(methodDef.returnType)];
                            writer << executor.getTypeName(&propertyType, false, false) << " "
                                   << metadata->getStringFromIndex(propertyDef.nameIndex) << " { ";
                        } else if (propertyDef.set >= 0) {
                            const auto& methodDef =
                                metadata->methodDefs[static_cast<size_t>(metadata->getMethodIndexFromTypeDefinition(typeDefIndex, propertyDef.set))];
                            writer << getModifiers(&methodDef);
                            const auto& parameterDef =
                                metadata->parameterDefs[static_cast<size_t>(methodDef.parameterStart)];
                            const Il2CppType& propertyType = il2Cpp->types[static_cast<size_t>(parameterDef.typeIndex)];
                            writer << executor.getTypeName(&propertyType, false, false) << " "
                                   << metadata->getStringFromIndex(propertyDef.nameIndex) << " { ";
                        }
                        if (propertyDef.get >= 0) {
                            writer << "get; ";
                        }
                        if (propertyDef.set >= 0) {
                            writer << "set; ";
                        }
                        writer << "}\n";
                    }
                }
                if (config.DumpMethod && typeDef.method_count > 0) {
                    writer << "\n\t// Methods\n";
                    for (int methodIndexInType = 0; methodIndexInType < typeDef.method_count; ++methodIndexInType) {
                        int i = metadata->getMethodIndexFromTypeDefinition(typeDefIndex, methodIndexInType);
                        writer << "\n";
                        const auto& methodDef = metadata->methodDefs[static_cast<size_t>(i)];
                        bool isAbstract = (methodDef.flags & METHOD_ATTRIBUTE_ABSTRACT) != 0;
                        if (config.DumpAttribute) {
                            writer << getCustomAttribute(static_cast<int>(imageIdx), methodDef.customAttributeIndex,
                                                         methodDef.token, "\t");
                        }
                        if (config.DumpMethodOffset) {
                            uint64_t methodPointer = il2Cpp->getMethodPointer(imageName, methodDef);
                            if (!isAbstract && methodPointer > 0) {
                                uint64_t fixedMethodPointer = il2Cpp->getRVA(methodPointer);
                                writer << "\t// RVA: 0x" << hexU64(fixedMethodPointer) << " Offset: 0x"
                                       << hexU64(il2Cpp->mapVATR(methodPointer)) << " VA: 0x" << hexU64(methodPointer);
                            } else {
                                writer << "\t// RVA: -1 Offset: -1";
                            }
                            if (methodDef.slot != UINT16_MAX) {
                                writer << " Slot: " << methodDef.slot;
                            }
                            writer << "\n";
                        }
                        writer << "\t";
                        writer << getModifiers(&methodDef);
                        const Il2CppType& methodReturnType = il2Cpp->types[static_cast<size_t>(methodDef.returnType)];
                        std::string methodName = metadata->getStringFromIndex(methodDef.nameIndex);
                        if (methodDef.genericContainerIndex >= 0) {
                            const auto& genericContainer =
                                metadata->genericContainers[static_cast<size_t>(methodDef.genericContainerIndex)];
                            methodName += executor.getGenericContainerParams(&genericContainer);
                        }
                        if (methodReturnType.byref == 1) {
                            writer << "ref ";
                        }
                        writer << executor.getTypeName(&methodReturnType, false, false) << " " << methodName << "(";
                        std::vector<std::string> parameterStrs;
                        for (int j = 0; j < methodDef.parameterCount; ++j) {
                            std::string parameterStr;
                            const auto& parameterDef =
                                metadata->parameterDefs[static_cast<size_t>(methodDef.parameterStart + j)];
                            std::string parameterName = metadata->getStringFromIndex(parameterDef.nameIndex);
                            const Il2CppType& parameterType = il2Cpp->types[static_cast<size_t>(parameterDef.typeIndex)];
                            std::string parameterTypeName = executor.getTypeName(&parameterType, false, false);
                            if (parameterType.byref == 1) {
                                if ((parameterType.attrs & PARAM_ATTRIBUTE_OUT) != 0 &&
                                    (parameterType.attrs & PARAM_ATTRIBUTE_IN) == 0) {
                                    parameterStr += "out ";
                                } else if ((parameterType.attrs & PARAM_ATTRIBUTE_OUT) == 0 &&
                                           (parameterType.attrs & PARAM_ATTRIBUTE_IN) != 0) {
                                    parameterStr += "in ";
                                } else {
                                    parameterStr += "ref ";
                                }
                            } else {
                                if ((parameterType.attrs & PARAM_ATTRIBUTE_IN) != 0) {
                                    parameterStr += "[In] ";
                                }
                                if ((parameterType.attrs & PARAM_ATTRIBUTE_OUT) != 0) {
                                    parameterStr += "[Out] ";
                                }
                            }
                            parameterStr += parameterTypeName + " " + parameterName;
                            Il2CppParameterDefaultValue parameterDefault;
                            if (metadata->getParameterDefaultValueFromIndex(methodDef.parameterStart + j,
                                                                            parameterDefault) &&
                                parameterDefault.dataIndex != -1) {
                                BlobValue value;
                                if (executor.tryGetDefaultValue(parameterDefault.typeIndex, parameterDefault.dataIndex,
                                                                value)) {
                                    parameterStr += " = ";
                                    if (value.kind == BlobValue::Kind::String) {
                                        parameterStr += "\"" + toEscapedString(value.str) + "\"";
                                    } else if (value.kind == BlobValue::Kind::Char) {
                                        parameterStr += "'\\x" + hexI32Lower(static_cast<int32_t>(value.ch)) + "'";
                                    } else if (value.kind != BlobValue::Kind::None) {
                                        parameterStr += blobToString(value);
                                    } else {
                                        parameterStr += "null";
                                    }
                                } else {
                                    parameterStr += " /*Metadata offset 0x" + hexU64(value.u32) + "*/";
                                }
                            }
                            parameterStrs.push_back(parameterStr);
                        }
                        for (size_t j = 0; j < parameterStrs.size(); j++) {
                            if (j != 0) {
                                writer << ", ";
                            }
                            writer << parameterStrs[j];
                        }
                        if (isAbstract) {
                            writer << ");\n";
                        } else {
                            writer << ") { }\n";
                        }

                        auto it = il2Cpp->methodDefinitionMethodSpecs.find(i);
                        if (it != il2Cpp->methodDefinitionMethodSpecs.end()) {
                            writer << "\t/* GenericInstMethod :\n";
                            std::vector<uint64_t> groupKeys;
                            std::vector<std::vector<const Il2CppMethodSpec*>> groups;
                            for (const auto& spec : it->second) {
                                uint64_t key = il2Cpp->methodSpecGenericMethodPointers.at(spec);
                                auto found = std::find(groupKeys.begin(), groupKeys.end(), key);
                                if (found == groupKeys.end()) {
                                    groupKeys.push_back(key);
                                    groups.push_back({});
                                    found = groupKeys.end() - 1;
                                }
                                groups[static_cast<size_t>(found - groupKeys.begin())].push_back(&spec);
                            }
                            for (size_t g = 0; g < groups.size(); g++) {
                                writer << "\t|\n";
                                uint64_t genericMethodPointer = groupKeys[g];
                                if (genericMethodPointer > 0) {
                                    uint64_t fixedPointer = il2Cpp->getRVA(genericMethodPointer);
                                    writer << "\t|-RVA: 0x" << hexU64(fixedPointer) << " Offset: 0x"
                                           << hexU64(il2Cpp->mapVATR(genericMethodPointer)) << " VA: 0x"
                                           << hexU64(genericMethodPointer) << "\n";
                                } else {
                                    writer << "\t|-RVA: -1 Offset: -1\n";
                                }
                                for (const auto* methodSpec : groups[g]) {
                                    auto names = executor.getMethodSpecName(methodSpec, false);
                                    writer << "\t|-" << names.first << "." << names.second << "\n";
                                }
                            }
                            writer << "\t*/\n";
                        }
                    }
                }
                writer << "}\n";
            }
        } catch (const std::exception& e) {
            std::printf("ERROR: Some errors in dumping\n");
            writer << "/*" << e.what() << "*/" << "\n}\n";
        }
    }
    writer.close();
}

std::string Il2CppDecompiler::getCustomAttribute(int imageIndex, int customAttributeIndex, uint32_t token,
                                                 const std::string& padding) {
    if (il2Cpp->version < 21) {
        return "";
    }
    int attributeIndex = metadata->getCustomAttributeIndex(imageIndex, customAttributeIndex, token);
    if (attributeIndex >= 0) {
        if (il2Cpp->version < 29) {
            uint64_t methodPointer = executor.customAttributeGenerators[static_cast<size_t>(attributeIndex)];
            uint64_t fixedMethodPointer = il2Cpp->getRVA(methodPointer);
            const auto& attributeTypeRange = metadata->attributeTypeRanges[static_cast<size_t>(attributeIndex)];
            std::string sb;
            for (int i = 0; i < attributeTypeRange.count; i++) {
                int typeIndex = metadata->attributeTypes[static_cast<size_t>(attributeTypeRange.start + i)];
                const Il2CppType& type = il2Cpp->types[static_cast<size_t>(typeIndex)];
                sb += padding + "[" + executor.getTypeName(&type, false, false) + "] // RVA: 0x" +
                      hexU64(fixedMethodPointer) + " Offset: 0x" + hexU64(il2Cpp->mapVATR(methodPointer)) + " VA: 0x" +
                      hexU64(methodPointer) + "\n";
            }
            return sb;
        }
        if (attributeIndex + 1 >= static_cast<int>(metadata->attributeDataRanges.size())) {
            return "";
        }
        const auto& startRange = metadata->attributeDataRanges[static_cast<size_t>(attributeIndex)];
        const auto& endRange = metadata->attributeDataRanges[static_cast<size_t>(attributeIndex + 1)];
        metadata->setPosition(static_cast<size_t>(metadata->header.attributeDataOffset) + startRange.startOffset);
        auto buff = metadata->readBytes(endRange.startOffset - startRange.startOffset);
        CustomAttributeDataReader reader(executor, std::move(buff));
        if (reader.count() == 0) {
            return "";
        }
        std::string sb;
        for (uint32_t i = 0; i < reader.count(); i++) {
            sb += padding;
            sb += reader.getStringCustomAttributeData();
            sb += '\n';
        }
        return sb;
    }
    return "";
}

std::string Il2CppDecompiler::getModifiers(const Il2CppMethodDefinition* methodDef) {
    int key = static_cast<int>(methodDef - metadata->methodDefs.data());
    auto cached = methodModifiers.find(key);
    if (cached != methodModifiers.end()) {
        return cached->second;
    }
    std::string str;
    uint32_t access = methodDef->flags & METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK;
    switch (access) {
        case METHOD_ATTRIBUTE_PRIVATE:
            str += "private ";
            break;
        case METHOD_ATTRIBUTE_PUBLIC:
            str += "public ";
            break;
        case METHOD_ATTRIBUTE_FAMILY:
            str += "protected ";
            break;
        case METHOD_ATTRIBUTE_ASSEM:
        case METHOD_ATTRIBUTE_FAM_AND_ASSEM:
            str += "internal ";
            break;
        case METHOD_ATTRIBUTE_FAM_OR_ASSEM:
            str += "protected internal ";
            break;
    }
    if ((methodDef->flags & METHOD_ATTRIBUTE_STATIC) != 0) {
        str += "static ";
    }
    if ((methodDef->flags & METHOD_ATTRIBUTE_ABSTRACT) != 0) {
        str += "abstract ";
        if ((methodDef->flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            str += "override ";
        }
    } else if ((methodDef->flags & METHOD_ATTRIBUTE_FINAL) != 0) {
        if ((methodDef->flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            str += "sealed override ";
        }
    } else if ((methodDef->flags & METHOD_ATTRIBUTE_VIRTUAL) != 0) {
        if ((methodDef->flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_NEW_SLOT) {
            str += "virtual ";
        } else {
            str += "override ";
        }
    }
    if ((methodDef->flags & METHOD_ATTRIBUTE_PINVOKE_IMPL) != 0) {
        str += "extern ";
    }
    methodModifiers[key] = str;
    return str;
}

}
