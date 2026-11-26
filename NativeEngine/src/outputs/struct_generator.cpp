#include "outputs/struct_generator.h"

#include "outputs/header_constants.h"
#include "outputs/il2cpp_constants.h"
#include "formats/pe.h"
#include "utils/section_helper.h"
#include "utils/string_utils.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <stdexcept>

namespace il2cppdumper {

using namespace Il2CppConstants;
using ordered_json = nlohmann::ordered_json;

std::unordered_set<uint64_t> StructGenerator::methodInfoCache;

namespace {

const std::unordered_set<std::string>& keywords() {
    static const std::unordered_set<std::string> k = {
        "klass",     "monitor",  "register", "_cs",  "auto",   "friend", "template", "flat",   "default",
        "_ds",       "interrupt", "unsigned", "signed", "asm",   "if",     "case",     "break",  "continue",
        "do",        "new",      "_",        "short", "union",  "class",  "namespace"};
    return k;
}

const std::unordered_set<std::string>& specialKeywords() {
    static const std::unordered_set<std::string> k = {"inline", "near", "far"};
    return k;
}

std::string sanitizeUtf8(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    size_t i = 0;
    while (i < in.size()) {
        unsigned char c = static_cast<unsigned char>(in[i]);
        size_t len = 0;
        if (c < 0x80) {
            len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
        }
        if (len == 0 || i + len > in.size()) {
            out += "\xEF\xBF\xBD";
            i += 1;
            continue;
        }
        bool valid = true;
        for (size_t j = 1; j < len; j++) {
            if ((static_cast<unsigned char>(in[i + j]) & 0xC0) != 0x80) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            out += "\xEF\xBF\xBD";
            i += 1;
            continue;
        }
        out.append(in, i, len);
        i += len;
    }
    return out;
}

std::string stjEscapeString(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    size_t i = 0;
    while (i < in.size()) {
        unsigned char c = static_cast<unsigned char>(in[i]);
        switch (c) {
            case '"':
                out += "\\u0022";
                i++;
                continue;
            case '\\':
                out += "\\\\";
                i++;
                continue;
            case '\b':
                out += "\\b";
                i++;
                continue;
            case '\f':
                out += "\\f";
                i++;
                continue;
            case '\n':
                out += "\\n";
                i++;
                continue;
            case '\r':
                out += "\\r";
                i++;
                continue;
            case '\t':
                out += "\\t";
                i++;
                continue;
            case '<':
                out += "\\u003C";
                i++;
                continue;
            case '>':
                out += "\\u003E";
                i++;
                continue;
            case '&':
                out += "\\u0026";
                i++;
                continue;
            case '\'':
                out += "\\u0027";
                i++;
                continue;
            case '+':
                out += "\\u002B";
                i++;
                continue;
            case '`':
                out += "\\u0060";
                i++;
                continue;
            default:
                break;
        }
        if (c < 0x20 || c == 0x7F) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04X", c);
            out += buf;
            i++;
            continue;
        }
        if (c < 0x80) {
            out += static_cast<char>(c);
            i++;
            continue;
        }
        size_t len = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
        uint32_t cp = 0;
        if (len == 2) {
            cp = c & 0x1F;
        } else if (len == 3) {
            cp = c & 0x0F;
        } else {
            cp = c & 0x07;
        }
        bool ok = i + len <= in.size();
        for (size_t j = 1; ok && j < len; j++) {
            unsigned char cc = static_cast<unsigned char>(in[i + j]);
            if ((cc & 0xC0) != 0x80) {
                ok = false;
                break;
            }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) {
            out += "\\uFFFD";
            i++;
            continue;
        }
        if (cp <= 0xFFFF) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04X", cp);
            out += buf;
        } else {
            uint32_t v = cp - 0x10000;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "\\u%04X\\u%04X", 0xD800 + (v >> 10), 0xDC00 + (v & 0x3FF));
            out += buf;
        }
        i += len;
    }
    return out;
}

void stjAppendValue(std::string& out, const ordered_json& j, int indent) {
    std::string pad(static_cast<size_t>(indent) * 2, ' ');
    if (j.is_string()) {
        out += "\"" + stjEscapeString(j.get_ref<const std::string&>()) + "\"";
    } else if (j.is_number_unsigned()) {
        out += std::to_string(j.get<uint64_t>());
    } else if (j.is_number_integer()) {
        out += std::to_string(j.get<int64_t>());
    } else if (j.is_boolean()) {
        out += j.get<bool>() ? "true" : "false";
    } else if (j.is_null()) {
        out += "null";
    } else if (j.is_array()) {
        if (j.empty()) {
            out += "[]";
            return;
        }
        out += "[\n";
        for (size_t i = 0; i < j.size(); i++) {
            out += std::string(static_cast<size_t>(indent + 1) * 2, ' ');
            stjAppendValue(out, j[i], indent + 1);
            if (i + 1 < j.size()) {
                out += ",";
            }
            out += "\n";
        }
        out += pad + "]";
    } else if (j.is_object()) {
        if (j.empty()) {
            out += "{}";
            return;
        }
        out += "{\n";
        size_t i = 0;
        for (auto it = j.begin(); it != j.end(); ++it, ++i) {
            out += std::string(static_cast<size_t>(indent + 1) * 2, ' ');
            out += "\"" + stjEscapeString(it.key()) + "\": ";
            stjAppendValue(out, it.value(), indent + 1);
            if (i + 1 < j.size()) {
                out += ",";
            }
            out += "\n";
        }
        out += pad + "}";
    } else {
        out += j.dump();
    }
}

std::string stjDump(const ordered_json& j) {
    std::string out;
    stjAppendValue(out, j, 0);
    return out;
}

}

StructGenerator::StructGenerator(Il2CppExecutor& executor)
    : executor(executor), metadata(executor.metadata), il2Cpp(executor.il2Cpp) {}

void StructGenerator::writeScript(const std::string& outputDir) {
    ScriptJson json;
    for (size_t imageIndex = 0; imageIndex < metadata->imageDefs.size(); imageIndex++) {
        const auto& imageDef = metadata->imageDefs[imageIndex];
        std::string imageName = metadata->getStringFromIndex(imageDef.nameIndex);
        int typeEnd = imageDef.typeStart + static_cast<int>(imageDef.typeCount);
        for (int typeIndex = imageDef.typeStart; typeIndex < typeEnd; typeIndex++) {
            const auto& typeDef = metadata->typeDefs[static_cast<size_t>(typeIndex)];
            typeDefImageNames[typeIndex] = imageName;
            createStructNameDic(&typeDef);
        }
    }
    for (const auto& il2CppType : il2Cpp->types) {
        if (il2CppType.type != static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST)) {
            continue;
        }
        auto genericClass = il2Cpp->mapVATRClass<Il2CppGenericClass>(il2CppType.genericClassPtr());
        const Il2CppTypeDefinition* typeDef = executor.getGenericClassTypeDefinition(&genericClass);
        if (typeDef == nullptr) {
            continue;
        }
        int typeDefIndex = static_cast<int>(typeDef - metadata->typeDefs.data());
        std::string typeBaseName = structNameDic[typeDefIndex];
        std::string typeToReplaceName = fixName(executor.getTypeDefName(typeDef, true, true));
        std::string typeReplaceName = fixName(executor.getTypeName(&il2CppType, true, false));
        std::string typeStructName = typeBaseName;
        size_t pos = typeStructName.find(typeToReplaceName);
        if (pos != std::string::npos) {
            typeStructName.replace(pos, typeToReplaceName.size(), typeReplaceName);
        }
        nameGenericClassDic[typeStructName] = &il2CppType;
        genericClassStructNameDic[il2CppType.genericClassPtr()] = typeStructName;
    }
    for (const auto& imageDef : metadata->imageDefs) {
        std::string imageName = metadata->getStringFromIndex(imageDef.nameIndex);
        int typeEnd = imageDef.typeStart + static_cast<int>(imageDef.typeCount);
        for (int typeIndex = imageDef.typeStart; typeIndex < typeEnd; typeIndex++) {
            const auto& typeDef = metadata->typeDefs[static_cast<size_t>(typeIndex)];
            addStruct(typeIndex);
            std::string typeName = executor.getTypeDefName(&typeDef, true, true);
            int methodEnd = typeDef.methodStart + typeDef.method_count;
            for (int i = typeDef.methodStart; i < methodEnd; ++i) {
                const auto& methodDef = metadata->methodDefs[static_cast<size_t>(i)];
                std::string methodName = metadata->getStringFromIndex(methodDef.nameIndex);
                uint64_t methodPointer = il2Cpp->getMethodPointer(imageName, methodDef);
                if (methodPointer > 0) {
                    std::vector<int> methodTypeSignature;
                    ScriptMethod scriptMethod;
                    json.methodList.push_back(scriptMethod);
                    json.methodList.back().Address = il2Cpp->getRVA(methodPointer);
                    std::string methodFullName = typeName + "$$" + methodName;
                    json.methodList.back().Name = methodFullName;

                    const Il2CppType& methodReturnType = il2Cpp->types[static_cast<size_t>(methodDef.returnType)];
                    std::string returnType = parseType(&methodReturnType);
                    if (methodReturnType.byref == 1) {
                        returnType += "*";
                    }
                    methodTypeSignature.push_back(methodReturnType.byref == 1 ? static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_PTR)
                                                                              : methodReturnType.type);
                    std::string signature = returnType + " " + fixName(methodFullName) + " (";
                    std::vector<std::string> parameterStrs;
                    if ((methodDef.flags & METHOD_ATTRIBUTE_STATIC) == 0) {
                        const Il2CppType& thisType = il2Cpp->types[static_cast<size_t>(typeDef.byvalTypeIndex)];
                        methodTypeSignature.push_back(thisType.type);
                        parameterStrs.push_back(parseType(&thisType) + " __this");
                    } else if (il2Cpp->version <= 24) {
                        methodTypeSignature.push_back(static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_PTR));
                        parameterStrs.push_back("Il2CppObject* __this");
                    }
                    for (int j = 0; j < methodDef.parameterCount; j++) {
                        const auto& parameterDef =
                            metadata->parameterDefs[static_cast<size_t>(methodDef.parameterStart + j)];
                        std::string parameterName = metadata->getStringFromIndex(parameterDef.nameIndex);
                        const Il2CppType& parameterType = il2Cpp->types[static_cast<size_t>(parameterDef.typeIndex)];
                        std::string parameterCType = parseType(&parameterType);
                        if (parameterType.byref == 1) {
                            parameterCType += "*";
                        }
                        methodTypeSignature.push_back(parameterType.byref == 1 ? static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_PTR)
                                                                               : parameterType.type);
                        parameterStrs.push_back(parameterCType + " " + fixName(parameterName));
                    }
                    methodTypeSignature.push_back(static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_PTR));
                    parameterStrs.push_back("const MethodInfo* method");
                    for (size_t p = 0; p < parameterStrs.size(); p++) {
                        if (p != 0) {
                            signature += ", ";
                        }
                        signature += parameterStrs[p];
                    }
                    signature += ");";
                    json.methodList.back().Signature = signature;
                    json.methodList.back().TypeSignature = getMethodTypeSignature(methodTypeSignature);
                }
                auto specIt = il2Cpp->methodDefinitionMethodSpecs.find(i);
                if (specIt != il2Cpp->methodDefinitionMethodSpecs.end()) {
                    for (const auto& methodSpec : specIt->second) {
                        uint64_t genericMethodPointer = il2Cpp->methodSpecGenericMethodPointers.at(methodSpec);
                        if (genericMethodPointer > 0) {
                            std::vector<int> methodTypeSignature;
                            ScriptMethod scriptMethod;
                            json.methodList.push_back(scriptMethod);
                            json.methodList.back().Address = il2Cpp->getRVA(genericMethodPointer);
                            char buf[32];
                            std::snprintf(buf, sizeof(buf), "MethodInfo_%llX",
                                          static_cast<unsigned long long>(json.methodList.back().Address));
                            std::string methodInfoName = buf;
                            std::string structTypeName = structNameDic[typeIndex];
                            auto rgctxs = generateRGCTX(imageName, &methodDef);
                            if (methodInfoCache.insert(genericMethodPointer).second) {
                                generateMethodInfo(methodInfoName, structTypeName, rgctxs);
                            }
                            auto specNames = executor.getMethodSpecName(&methodSpec, true);
                            std::string methodFullName = specNames.first + "$$" + specNames.second;
                            json.methodList.back().Name = methodFullName;

                            Il2CppGenericContext genericContext = executor.getMethodSpecGenericContext(&methodSpec);
                            const Il2CppType& methodReturnType = il2Cpp->types[static_cast<size_t>(methodDef.returnType)];
                            std::string returnType = parseType(&methodReturnType, &genericContext);
                            if (methodReturnType.byref == 1) {
                                returnType += "*";
                            }
                            methodTypeSignature.push_back(methodReturnType.byref == 1
                                                              ? static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_PTR)
                                                              : methodReturnType.type);
                            std::string signature = returnType + " " + fixName(methodFullName) + " (";
                            std::vector<std::string> parameterStrs;
                            if ((methodDef.flags & METHOD_ATTRIBUTE_STATIC) == 0) {
                                std::string thisType;
                                if (methodSpec.classIndexIndex != -1) {
                                    std::string typeBaseName = structNameDic[typeIndex];
                                    std::string typeToReplaceName = fixName(typeName);
                                    std::string typeReplaceName = fixName(specNames.first);
                                    std::string typeStructName = typeBaseName;
                                    size_t pos = typeStructName.find(typeToReplaceName);
                                    if (pos != std::string::npos) {
                                        typeStructName.replace(pos, typeToReplaceName.size(), typeReplaceName);
                                    }
                                    auto it = nameGenericClassDic.find(typeStructName);
                                    if (it != nameGenericClassDic.end()) {
                                        thisType = parseType(it->second);
                                        methodTypeSignature.push_back(it->second->type);
                                    } else {
                                        thisType = parseType(&il2Cpp->types[static_cast<size_t>(typeDef.byvalTypeIndex)]);
                                        methodTypeSignature.push_back(
                                            il2Cpp->types[static_cast<size_t>(typeDef.byvalTypeIndex)].type);
                                    }
                                } else {
                                    thisType = parseType(&il2Cpp->types[static_cast<size_t>(typeDef.byvalTypeIndex)]);
                                    methodTypeSignature.push_back(
                                        il2Cpp->types[static_cast<size_t>(typeDef.byvalTypeIndex)].type);
                                }
                                parameterStrs.push_back(thisType + " __this");
                            } else if (il2Cpp->version <= 24) {
                                methodTypeSignature.push_back(static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_PTR));
                                parameterStrs.push_back("Il2CppObject* __this");
                            }
                            for (int j = 0; j < methodDef.parameterCount; j++) {
                                const auto& parameterDef =
                                    metadata->parameterDefs[static_cast<size_t>(methodDef.parameterStart + j)];
                                std::string parameterName = metadata->getStringFromIndex(parameterDef.nameIndex);
                                const Il2CppType& parameterType =
                                    il2Cpp->types[static_cast<size_t>(parameterDef.typeIndex)];
                                std::string parameterCType = parseType(&parameterType, &genericContext);
                                if (parameterType.byref == 1) {
                                    parameterCType += "*";
                                }
                                methodTypeSignature.push_back(parameterType.byref == 1
                                                                  ? static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_PTR)
                                                                  : parameterType.type);
                                parameterStrs.push_back(parameterCType + " " + fixName(parameterName));
                            }
                            methodTypeSignature.push_back(static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_PTR));
                            parameterStrs.push_back("const " + methodInfoName + "* method");
                            for (size_t p = 0; p < parameterStrs.size(); p++) {
                                if (p != 0) {
                                    signature += ", ";
                                }
                                signature += parameterStrs[p];
                            }
                            signature += ");";
                            json.methodList.back().Signature = signature;
                            json.methodList.back().TypeSignature = getMethodTypeSignature(methodTypeSignature);
                        }
                    }
                }
            }
        }
    }
    std::vector<uint64_t> orderedPointers;
    if (il2Cpp->version >= 24.2) {
        for (const auto& pair : il2Cpp->codeGenModuleMethodPointers) {
            orderedPointers.insert(orderedPointers.end(), pair.second.begin(), pair.second.end());
        }
    } else {
        orderedPointers = il2Cpp->methodPointers;
    }
    orderedPointers.insert(orderedPointers.end(), il2Cpp->genericMethodPointers.begin(),
                           il2Cpp->genericMethodPointers.end());
    orderedPointers.insert(orderedPointers.end(), il2Cpp->invokerPointers.begin(), il2Cpp->invokerPointers.end());
    if (il2Cpp->version < 29) {
        orderedPointers.insert(orderedPointers.end(), executor.customAttributeGenerators.begin(),
                               executor.customAttributeGenerators.end());
    }
    if (il2Cpp->version >= 22) {
        if (!il2Cpp->reversePInvokeWrappers.empty()) {
            orderedPointers.insert(orderedPointers.end(), il2Cpp->reversePInvokeWrappers.begin(),
                                   il2Cpp->reversePInvokeWrappers.end());
        }
        if (!il2Cpp->unresolvedVirtualCallPointers.empty()) {
            orderedPointers.insert(orderedPointers.end(), il2Cpp->unresolvedVirtualCallPointers.begin(),
                                   il2Cpp->unresolvedVirtualCallPointers.end());
        }
    }
    std::sort(orderedPointers.begin(), orderedPointers.end());
    orderedPointers.erase(std::unique(orderedPointers.begin(), orderedPointers.end()), orderedPointers.end());
    orderedPointers.erase(std::remove(orderedPointers.begin(), orderedPointers.end(), 0), orderedPointers.end());
    for (uint64_t p : orderedPointers) {
        json.Addresses.push_back(il2Cpp->getRVA(p));
    }
    if (il2Cpp->version >= 27) {
        auto sectionHelper = executor.getSectionHelper();
        for (const auto& sec : sectionHelper.data()) {
            il2Cpp->setPosition(sec.offset);
            size_t end = std::min<uint64_t>(sec.offsetEnd, il2Cpp->length()) - il2Cpp->pointerSize();
            while (il2Cpp->position() < end) {
                size_t addr = il2Cpp->position();
                uint64_t metadataValue = il2Cpp->readUPtr();
                size_t position = il2Cpp->position();
                if (metadataValue < UINT32_MAX) {
                    uint32_t encodedToken = static_cast<uint32_t>(metadataValue);
                    uint32_t usage = Metadata::getEncodedIndexType(encodedToken);
                    if (usage > 0 && usage <= 6) {
                        uint32_t decodedIndex = metadata->getDecodedMethodIndex(encodedToken);
                        if (metadataValue == (static_cast<uint64_t>((usage << 29) | (decodedIndex << 1)) + 1)) {
                            uint64_t va = il2Cpp->mapRTVA(addr);
                            if (va > 0) {
                                switch (static_cast<Il2CppMetadataUsage>(usage)) {
                                    case Il2CppMetadataUsage::kIl2CppMetadataUsageTypeInfo:
                                        if (decodedIndex < il2Cpp->types.size()) {
                                            addMetadataUsageTypeInfo(json, decodedIndex, va);
                                        }
                                        break;
                                    case Il2CppMetadataUsage::kIl2CppMetadataUsageIl2CppType:
                                        if (decodedIndex < il2Cpp->types.size()) {
                                            addMetadataUsageIl2CppType(json, decodedIndex, va);
                                        }
                                        break;
                                    case Il2CppMetadataUsage::kIl2CppMetadataUsageMethodDef:
                                        if (decodedIndex < metadata->methodDefs.size()) {
                                            addMetadataUsageMethodDef(json, decodedIndex, va);
                                        }
                                        break;
                                    case Il2CppMetadataUsage::kIl2CppMetadataUsageFieldInfo:
                                        if (decodedIndex < metadata->fieldRefs.size()) {
                                            addMetadataUsageFieldInfo(json, decodedIndex, va);
                                        }
                                        break;
                                    case Il2CppMetadataUsage::kIl2CppMetadataUsageStringLiteral:
                                        if (decodedIndex < metadata->stringLiterals.size()) {
                                            addMetadataUsageStringLiteral(json, decodedIndex, va);
                                        }
                                        break;
                                    case Il2CppMetadataUsage::kIl2CppMetadataUsageMethodRef:
                                        if (decodedIndex < il2Cpp->methodSpecs.size()) {
                                            addMetadataUsageMethodRef(json, decodedIndex, va);
                                        }
                                        break;
                                    default:
                                        break;
                                }
                                if (il2Cpp->position() != position) {
                                    il2Cpp->setPosition(position);
                                }
                            }
                        }
                    }
                }
                il2Cpp->setPosition(addr + il2Cpp->pointerSize());
            }
        }
    } else if (il2Cpp->version > 16 && il2Cpp->version < 27) {
        for (const auto& entry : metadata->metadataUsageDic[static_cast<size_t>(Il2CppMetadataUsage::kIl2CppMetadataUsageTypeInfo)]) {
            addMetadataUsageTypeInfo(json, entry.second, il2Cpp->metadataUsages[entry.first]);
        }
        for (const auto& entry : metadata->metadataUsageDic[static_cast<size_t>(Il2CppMetadataUsage::kIl2CppMetadataUsageIl2CppType)]) {
            addMetadataUsageIl2CppType(json, entry.second, il2Cpp->metadataUsages[entry.first]);
        }
        for (const auto& entry : metadata->metadataUsageDic[static_cast<size_t>(Il2CppMetadataUsage::kIl2CppMetadataUsageMethodDef)]) {
            addMetadataUsageMethodDef(json, entry.second, il2Cpp->metadataUsages[entry.first]);
        }
        for (const auto& entry : metadata->metadataUsageDic[static_cast<size_t>(Il2CppMetadataUsage::kIl2CppMetadataUsageFieldInfo)]) {
            addMetadataUsageFieldInfo(json, entry.second, il2Cpp->metadataUsages[entry.first]);
        }
        for (const auto& entry : metadata->metadataUsageDic[static_cast<size_t>(Il2CppMetadataUsage::kIl2CppMetadataUsageStringLiteral)]) {
            addMetadataUsageStringLiteral(json, entry.second, il2Cpp->metadataUsages[entry.first]);
        }
        for (const auto& entry : metadata->metadataUsageDic[static_cast<size_t>(Il2CppMetadataUsage::kIl2CppMetadataUsageMethodRef)]) {
            addMetadataUsageMethodRef(json, entry.second, il2Cpp->metadataUsages[entry.first]);
        }
    }
    ordered_json stringLiteralsJson = ordered_json::array();
    for (const auto& s : json.stringList) {
        char addrBuf[32];
        std::snprintf(addrBuf, sizeof(addrBuf), "0x%llX", static_cast<unsigned long long>(s.Address));
        ordered_json item;
        item["value"] = sanitizeUtf8(s.Value);
        item["address"] = addrBuf;
        stringLiteralsJson.push_back(item);
    }
    {
        std::ofstream f(outputDir + "stringliteral.json", std::ios::binary | std::ios::trunc);
        f << stjDump(stringLiteralsJson);
    }
    ordered_json scriptJson;
    scriptJson["ScriptMethod"] = ordered_json::array();
    for (const auto& m : json.methodList) {
        ordered_json item;
        item["Address"] = m.Address;
        item["Name"] = sanitizeUtf8(m.Name);
        item["Signature"] = sanitizeUtf8(m.Signature);
        item["TypeSignature"] = m.TypeSignature;
        scriptJson["ScriptMethod"].push_back(item);
    }
    scriptJson["ScriptString"] = ordered_json::array();
    for (const auto& s : json.stringList) {
        ordered_json item;
        item["Address"] = s.Address;
        item["Value"] = sanitizeUtf8(s.Value);
        scriptJson["ScriptString"].push_back(item);
    }
    scriptJson["ScriptMetadata"] = ordered_json::array();
    for (const auto& m : json.metadataList) {
        ordered_json item;
        item["Address"] = m.Address;
        item["Name"] = sanitizeUtf8(m.Name);
        item["Signature"] = m.Signature.empty() ? ordered_json(nullptr) : ordered_json(sanitizeUtf8(m.Signature));
        scriptJson["ScriptMetadata"].push_back(item);
    }
    scriptJson["ScriptMetadataMethod"] = ordered_json::array();
    for (const auto& m : json.metadataMethodList) {
        ordered_json item;
        item["Address"] = m.Address;
        item["Name"] = sanitizeUtf8(m.Name);
        item["MethodAddress"] = m.MethodAddress;
        scriptJson["ScriptMetadataMethod"].push_back(item);
    }
    scriptJson["Addresses"] = json.Addresses;
    {
        std::ofstream f(outputDir + "script.json", std::ios::binary | std::ios::trunc);
        f << stjDump(scriptJson);
    }
    for (size_t i = 0; i < genericClassList.size(); i++) {
        addGenericClassStruct(genericClassList[i]);
    }
    std::string headerStruct;
    for (const auto& info : structInfoList) {
        structInfoWithStructName[info.typeName + "_o"] = const_cast<StructInfo*>(&info);
    }
    for (const auto& info : structInfoList) {
        headerStruct += recursionStructInfo(&info);
    }
    std::string sb = HeaderConstants::GenericHeader;
    double v = il2Cpp->version;
    bool supported = true;
    if (v >= 35) {
        sb += HeaderConstants::HeaderV35Plus;
    } else if (v == 22) {
        sb += HeaderConstants::HeaderV22;
    } else if (v == 23 || v == 24) {
        sb += HeaderConstants::HeaderV240;
    } else if (v == 24.1) {
        sb += HeaderConstants::HeaderV241;
    } else if (v == 24.2 || v == 24.3 || v == 24.4 || v == 24.5) {
        sb += HeaderConstants::HeaderV242;
    } else if (v == 27 || v == 27.1 || v == 27.2) {
        sb += HeaderConstants::HeaderV27;
    } else if (v == 29 || v == 29.1 || v == 31) {
        sb += HeaderConstants::HeaderV29;
    } else {
        std::printf("WARNING: This il2cpp version [%g] does not support generating .h files\n", v);
        supported = false;
    }
    if (supported) {
        sb += headerStruct;
        sb += arrayClassHeader;
        sb += methodInfoHeader;
        std::ofstream f(outputDir + "il2cpp.h", std::ios::binary | std::ios::trunc);
        f << sb;
    }
}

void StructGenerator::addMetadataUsageTypeInfo(ScriptJson& json, uint32_t index, uint64_t address) {
    const Il2CppType& type = il2Cpp->types[index];
    std::string typeName = executor.getTypeName(&type, true, false);
    ScriptMetadata scriptMetadata;
    json.metadataList.push_back(scriptMetadata);
    json.metadataList.back().Address = il2Cpp->getRVA(address);
    json.metadataList.back().Name = typeName + "_TypeInfo";
    std::string signature = getIl2CppStructName(&type);
    if (signature.size() >= 6 && signature.compare(signature.size() - 6, 6, "_array") == 0) {
        json.metadataList.back().Signature = "Il2CppClass*";
    } else {
        json.metadataList.back().Signature = fixName(signature) + "_c*";
    }
}

void StructGenerator::addMetadataUsageIl2CppType(ScriptJson& json, uint32_t index, uint64_t address) {
    const Il2CppType& type = il2Cpp->types[index];
    std::string typeName = executor.getTypeName(&type, true, false);
    ScriptMetadata scriptMetadata;
    json.metadataList.push_back(scriptMetadata);
    json.metadataList.back().Address = il2Cpp->getRVA(address);
    json.metadataList.back().Name = typeName + "_var";
    json.metadataList.back().Signature = "Il2CppType*";
}

void StructGenerator::addMetadataUsageMethodDef(ScriptJson& json, uint32_t index, uint64_t address) {
    const auto& methodDef = metadata->methodDefs[index];
    const auto& typeDef = metadata->typeDefs[static_cast<size_t>(methodDef.declaringType)];
    std::string typeName = executor.getTypeDefName(&typeDef, true, true);
    std::string methodName = typeName + "." + metadata->getStringFromIndex(methodDef.nameIndex) + "()";
    ScriptMetadataMethod scriptMetadataMethod;
    json.metadataMethodList.push_back(scriptMetadataMethod);
    json.metadataMethodList.back().Address = il2Cpp->getRVA(address);
    json.metadataMethodList.back().Name = "Method$" + methodName;
    std::string imageName = typeDefImageNames[static_cast<int>(&typeDef - metadata->typeDefs.data())];
    uint64_t methodPointer = il2Cpp->getMethodPointer(imageName, methodDef);
    if (methodPointer > 0) {
        json.metadataMethodList.back().MethodAddress = il2Cpp->getRVA(methodPointer);
    }
}

void StructGenerator::addMetadataUsageFieldInfo(ScriptJson& json, uint32_t index, uint64_t address) {
    const auto& fieldRef = metadata->fieldRefs[index];
    const Il2CppType& type = il2Cpp->types[static_cast<size_t>(fieldRef.typeIndex)];
    const Il2CppTypeDefinition* typeDef = getTypeDefinition(&type);
    const auto& fieldDef = metadata->fieldDefs[static_cast<size_t>(typeDef->fieldStart + fieldRef.fieldIndex)];
    std::string fieldName = executor.getTypeName(&type, true, false) + "." + metadata->getStringFromIndex(fieldDef.nameIndex);
    ScriptMetadata scriptMetadata;
    json.metadataList.push_back(scriptMetadata);
    json.metadataList.back().Address = il2Cpp->getRVA(address);
    json.metadataList.back().Name = "Field$" + fieldName;
}

void StructGenerator::addMetadataUsageStringLiteral(ScriptJson& json, uint32_t index, uint64_t address) {
    ScriptString scriptString;
    json.stringList.push_back(scriptString);
    json.stringList.back().Address = il2Cpp->getRVA(address);
    json.stringList.back().Value = metadata->getStringLiteralFromIndex(index);
}

void StructGenerator::addMetadataUsageMethodRef(ScriptJson& json, uint32_t index, uint64_t address) {
    const auto& methodSpec = il2Cpp->methodSpecs[index];
    ScriptMetadataMethod scriptMetadataMethod;
    json.metadataMethodList.push_back(scriptMetadataMethod);
    json.metadataMethodList.back().Address = il2Cpp->getRVA(address);
    auto specNames = executor.getMethodSpecName(&methodSpec, true);
    json.metadataMethodList.back().Name = "Method$" + specNames.first + "." + specNames.second + "()";
    auto it = il2Cpp->methodSpecGenericMethodPointers.find(methodSpec);
    if (it != il2Cpp->methodSpecGenericMethodPointers.end()) {
        uint64_t genericMethodPointer = it->second;
        if (genericMethodPointer > 0) {
            json.metadataMethodList.back().MethodAddress = il2Cpp->getRVA(genericMethodPointer);
        }
    }
}

std::string StructGenerator::fixName(const std::string& str) {
    std::string s = str;
    if (keywords().count(s) != 0) {
        s = "_" + s;
    } else if (specialKeywords().count(s) != 0) {
        s = "_" + s + "_";
    }
    if (!s.empty() && s[0] >= '0' && s[0] <= '9') {
        return "_" + s;
    }
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
            out += static_cast<char>(c);
        } else {
            out += '_';
        }
    }
    return out;
}

std::string StructGenerator::parseType(const Il2CppType* il2CppType, const Il2CppGenericContext* context) {
    switch (il2CppType->type) {
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VOID):
            return "void";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_BOOLEAN):
            return "bool";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CHAR):
            return "uint16_t";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I1):
            return "int8_t";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U1):
            return "uint8_t";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I2):
            return "int16_t";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U2):
            return "uint16_t";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I4):
            return "int32_t";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U4):
            return "uint32_t";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I8):
            return "int64_t";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U8):
            return "uint64_t";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R4):
            return "float";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R8):
            return "double";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_STRING):
            return "System_String_o*";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_PTR): {
            const Il2CppType* oriType = il2Cpp->getIl2CppType(il2CppType->typePtr());
            return parseType(oriType) + "*";
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE): {
            const Il2CppTypeDefinition* typeDef = executor.getTypeDefinitionFromIl2CppType(il2CppType);
            if (typeDef->isEnum()) {
                return parseType(&il2Cpp->types[static_cast<size_t>(typeDef->enumElementTypeIndex(il2Cpp->version))]);
            }
            return structNameDic[static_cast<int>(typeDef - metadata->typeDefs.data())] + "_o";
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CLASS): {
            const Il2CppTypeDefinition* typeDef = executor.getTypeDefinitionFromIl2CppType(il2CppType);
            return structNameDic[static_cast<int>(typeDef - metadata->typeDefs.data())] + "_o*";
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VAR): {
            if (context != nullptr) {
                const Il2CppGenericParameter* genericParameter = executor.getGenericParameteFromIl2CppType(il2CppType);
                auto genericInst = il2Cpp->mapVATRClass<Il2CppGenericInst>(context->class_inst);
                auto pointers = il2Cpp->mapVATRArray<uint64_t>(genericInst.type_argv, genericInst.type_argc);
                const Il2CppType* type = il2Cpp->getIl2CppType(pointers[static_cast<size_t>(genericParameter->num)]);
                return parseType(type);
            }
            return "Il2CppObject*";
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_ARRAY): {
            auto arrayType = il2Cpp->mapVATRClass<Il2CppArrayType>(il2CppType->arrayPtr());
            const Il2CppType* elementType = il2Cpp->getIl2CppType(arrayType.etype);
            std::string elementStructName = getIl2CppStructName(elementType, context);
            std::string typeStructName = elementStructName + "_array";
            if (structNameHashSet.insert(typeStructName).second) {
                parseArrayClassStruct(elementType, context);
            }
            return typeStructName + "*";
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST): {
            auto genericClass = il2Cpp->mapVATRClass<Il2CppGenericClass>(il2CppType->genericClassPtr());
            const Il2CppTypeDefinition* typeDef = executor.getGenericClassTypeDefinition(&genericClass);
            std::string typeStructName = genericClassStructNameDic[il2CppType->genericClassPtr()];
            if (structNameHashSet.insert(typeStructName).second) {
                genericClassList.push_back(il2CppType->genericClassPtr());
            }
            if (typeDef->isValueType()) {
                if (typeDef->isEnum()) {
                    return parseType(&il2Cpp->types[static_cast<size_t>(typeDef->enumElementTypeIndex(il2Cpp->version))]);
                }
                return typeStructName + "_o";
            }
            return typeStructName + "_o*";
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_TYPEDBYREF):
            return "Il2CppObject*";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I):
            return "intptr_t";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U):
            return "uintptr_t";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_OBJECT):
            return "Il2CppObject*";
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_SZARRAY): {
            const Il2CppType* elementType = il2Cpp->getIl2CppType(il2CppType->typePtr());
            std::string elementStructName = getIl2CppStructName(elementType, context);
            std::string typeStructName = elementStructName + "_array";
            if (structNameHashSet.insert(typeStructName).second) {
                parseArrayClassStruct(elementType, context);
            }
            return typeStructName + "*";
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_MVAR): {
            if (context != nullptr) {
                const Il2CppGenericParameter* genericParameter = executor.getGenericParameteFromIl2CppType(il2CppType);
                if (context->method_inst == 0 && context->class_inst != 0) {
                    auto genericInst = il2Cpp->mapVATRClass<Il2CppGenericInst>(context->class_inst);
                    auto pointers = il2Cpp->mapVATRArray<uint64_t>(genericInst.type_argv, genericInst.type_argc);
                    const Il2CppType* type = il2Cpp->getIl2CppType(pointers[static_cast<size_t>(genericParameter->num)]);
                    return parseType(type);
                }
                auto genericInst = il2Cpp->mapVATRClass<Il2CppGenericInst>(context->method_inst);
                auto pointers = il2Cpp->mapVATRArray<uint64_t>(genericInst.type_argv, genericInst.type_argc);
                const Il2CppType* type = il2Cpp->getIl2CppType(pointers[static_cast<size_t>(genericParameter->num)]);
                return parseType(type);
            }
            return "Il2CppObject*";
        }
        default:
            throw std::runtime_error("parseType: unsupported type");
    }
}

std::string StructGenerator::getMethodTypeSignature(const std::vector<int>& types) {
    std::string signature;
    for (int type : types) {
        switch (type) {
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VOID):
                signature += "v";
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_BOOLEAN):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CHAR):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I1):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U1):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I2):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U2):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I4):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U4):
                signature += "i";
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I8):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U8):
                signature += "j";
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R4):
                signature += "f";
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R8):
                signature += "d";
                break;
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_STRING):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_PTR):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CLASS):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VAR):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_ARRAY):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_TYPEDBYREF):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_OBJECT):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_SZARRAY):
            case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_MVAR):
                signature += "i";
                break;
            default:
                throw std::runtime_error("getMethodTypeSignature: unsupported type");
        }
    }
    return signature;
}

void StructGenerator::addStruct(int typeDefIndex) {
    const auto& typeDef = metadata->typeDefs[static_cast<size_t>(typeDefIndex)];
    StructInfo structInfo;
    structInfoList.push_back(std::move(structInfo));
    StructInfo& info = structInfoList.back();
    info.typeName = structNameDic[typeDefIndex];
    info.isValueType = typeDef.isValueType();
    addParents(&typeDef, info);
    addFields(&typeDef, info, nullptr);
    addVTableMethod(info, &typeDef);
    addRGCTX(info, typeDefIndex);
}

void StructGenerator::addGenericClassStruct(uint64_t pointer) {
    auto genericClass = il2Cpp->mapVATRClass<Il2CppGenericClass>(pointer);
    const Il2CppTypeDefinition* typeDef = executor.getGenericClassTypeDefinition(&genericClass);
    StructInfo structInfo;
    structInfoList.push_back(std::move(structInfo));
    StructInfo& info = structInfoList.back();
    info.typeName = genericClassStructNameDic[pointer];
    info.isValueType = typeDef->isValueType();
    addParents(typeDef, info);
    addFields(typeDef, info, &genericClass.context);
    addVTableMethod(info, typeDef);
}

void StructGenerator::addParents(const Il2CppTypeDefinition* typeDef, StructInfo& structInfo) {
    if (!typeDef->isValueType() && !typeDef->isEnum()) {
        if (typeDef->parentIndex >= 0) {
            const Il2CppType& parent = il2Cpp->types[static_cast<size_t>(typeDef->parentIndex)];
            if (parent.type != static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_OBJECT)) {
                structInfo.parent = getIl2CppStructName(&parent);
            }
        }
    }
}

void StructGenerator::addFields(const Il2CppTypeDefinition* typeDef, StructInfo& structInfo,
                                const Il2CppGenericContext* context) {
    if (typeDef->field_count > 0) {
        int fieldEnd = typeDef->fieldStart + typeDef->field_count;
        std::unordered_set<std::string> cache;
        for (int i = typeDef->fieldStart; i < fieldEnd; ++i) {
            const auto& fieldDef = metadata->fieldDefs[static_cast<size_t>(i)];
            const Il2CppType& fieldType = il2Cpp->types[static_cast<size_t>(fieldDef.typeIndex)];
            if ((fieldType.attrs & FIELD_ATTRIBUTE_LITERAL) != 0) {
                continue;
            }
            StructFieldInfo structFieldInfo;
            structFieldInfo.fieldTypeName = parseType(&fieldType, context);
            std::string fieldName = fixName(metadata->getStringFromIndex(fieldDef.nameIndex));
            if (!cache.insert(fieldName).second) {
                fieldName = "_" + std::to_string(i - typeDef->fieldStart) + "_" + fieldName;
            }
            structFieldInfo.fieldName = fieldName;
            structFieldInfo.isValueType = isValueType(&fieldType, context);
            structFieldInfo.isCustomType = isCustomType(&fieldType, context);
            if ((fieldType.attrs & FIELD_ATTRIBUTE_STATIC) != 0) {
                structInfo.staticFields.push_back(std::move(structFieldInfo));
            } else {
                structInfo.fields.push_back(std::move(structFieldInfo));
            }
        }
    }
}

void StructGenerator::addVTableMethod(StructInfo& structInfo, const Il2CppTypeDefinition* typeDef) {
    std::map<int, const Il2CppMethodDefinition*> dic;
    for (int i = 0; i < typeDef->vtable_count; i++) {
        int vTableIndex = typeDef->vtableStart + i;
        uint32_t encodedMethodIndex = metadata->vtableMethods[static_cast<size_t>(vTableIndex)];
        uint32_t usage = Metadata::getEncodedIndexType(encodedMethodIndex);
        uint32_t index = metadata->getDecodedMethodIndex(encodedMethodIndex);
        const Il2CppMethodDefinition* methodDef;
        if (usage == 6) {
            const auto& methodSpec = il2Cpp->methodSpecs[index];
            methodDef = &metadata->methodDefs[static_cast<size_t>(methodSpec.methodDefinitionIndex)];
        } else {
            methodDef = &metadata->methodDefs[index];
        }
        if (methodDef->slot != UINT16_MAX) {
            dic[methodDef->slot] = methodDef;
        }
    }
    if (!dic.empty()) {
        structInfo.vtableMethod.resize(static_cast<size_t>(dic.rbegin()->first) + 1);
        for (const auto& entry : dic) {
            structInfo.vtableMethod[static_cast<size_t>(entry.first)].methodName =
                fixName(metadata->getStringFromIndex(entry.second->nameIndex));
        }
    }
}

void StructGenerator::addRGCTX(StructInfo& structInfo, int typeDefIndex) {
    std::string imageName = typeDefImageNames[typeDefIndex];
    const auto& typeDef = metadata->typeDefs[static_cast<size_t>(typeDefIndex)];
    auto collection = executor.getRGCTXDefinition(imageName, typeDef);
    for (const auto& definitionData : collection) {
        StructRGCTXInfo structRGCTXInfo;
        structInfo.rgctxs.push_back(structRGCTXInfo);
        StructRGCTXInfo& info = structInfo.rgctxs.back();
        info.type = definitionData.type();
        Il2CppRGCTXDefinitionData rgctxDefData;
        if (il2Cpp->version >= 27.2) {
            rgctxDefData = il2Cpp->mapVATRClass<Il2CppRGCTXDefinitionData>(definitionData._data);
        } else {
            rgctxDefData = definitionData.data;
        }
        switch (info.type) {
            case static_cast<int>(Il2CppRGCTXDataType::IL2CPP_RGCTX_DATA_TYPE): {
                const Il2CppType& t = il2Cpp->types[static_cast<size_t>(rgctxDefData.typeIndex())];
                info.typeName = fixName(executor.getTypeName(&t, true, false));
                break;
            }
            case static_cast<int>(Il2CppRGCTXDataType::IL2CPP_RGCTX_DATA_CLASS): {
                const Il2CppType& t = il2Cpp->types[static_cast<size_t>(rgctxDefData.typeIndex())];
                info.className = fixName(executor.getTypeName(&t, true, false));
                break;
            }
            case static_cast<int>(Il2CppRGCTXDataType::IL2CPP_RGCTX_DATA_METHOD): {
                const auto& methodSpec = il2Cpp->methodSpecs[static_cast<size_t>(rgctxDefData.methodIndex())];
                auto names = executor.getMethodSpecName(&methodSpec, true);
                info.methodName = fixName(names.first + "." + names.second);
                break;
            }
        }
    }
}

std::vector<StructRGCTXInfo> StructGenerator::generateRGCTX(const std::string& imageName,
                                                            const Il2CppMethodDefinition* methodDef) {
    std::vector<StructRGCTXInfo> rgctxs;
    auto collection = executor.getRGCTXDefinition(imageName, *methodDef);
    for (const auto& definitionData : collection) {
        StructRGCTXInfo structRGCTXInfo;
        rgctxs.push_back(structRGCTXInfo);
        StructRGCTXInfo& info = rgctxs.back();
        info.type = definitionData.type();
        Il2CppRGCTXDefinitionData rgctxDefData;
        if (il2Cpp->version >= 27.2) {
            rgctxDefData = il2Cpp->mapVATRClass<Il2CppRGCTXDefinitionData>(definitionData._data);
        } else {
            rgctxDefData = definitionData.data;
        }
        switch (info.type) {
            case static_cast<int>(Il2CppRGCTXDataType::IL2CPP_RGCTX_DATA_TYPE): {
                const Il2CppType& t = il2Cpp->types[static_cast<size_t>(rgctxDefData.typeIndex())];
                info.typeName = fixName(executor.getTypeName(&t, true, false));
                break;
            }
            case static_cast<int>(Il2CppRGCTXDataType::IL2CPP_RGCTX_DATA_CLASS): {
                const Il2CppType& t = il2Cpp->types[static_cast<size_t>(rgctxDefData.typeIndex())];
                info.className = fixName(executor.getTypeName(&t, true, false));
                break;
            }
            case static_cast<int>(Il2CppRGCTXDataType::IL2CPP_RGCTX_DATA_METHOD): {
                const auto& methodSpec = il2Cpp->methodSpecs[static_cast<size_t>(rgctxDefData.methodIndex())];
                auto names = executor.getMethodSpecName(&methodSpec, true);
                info.methodName = fixName(names.first + "." + names.second);
                break;
            }
        }
    }
    return rgctxs;
}

void StructGenerator::parseArrayClassStruct(const Il2CppType* il2CppType, const Il2CppGenericContext* context) {
    std::string structName = getIl2CppStructName(il2CppType, context);
    arrayClassHeader += "struct " + structName + "_array {\n";
    arrayClassHeader += "\tIl2CppObject obj;\n";
    arrayClassHeader += "\tIl2CppArrayBounds *bounds;\n";
    arrayClassHeader += "\til2cpp_array_size_t max_length;\n";
    arrayClassHeader += "\t" + parseType(il2CppType, context) + " m_Items[65535];\n";
    arrayClassHeader += "};\n";
}

const Il2CppTypeDefinition* StructGenerator::getTypeDefinition(const Il2CppType* il2CppType) {
    switch (il2CppType->type) {
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VOID):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_BOOLEAN):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CHAR):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I1):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U1):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I2):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U2):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I4):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U4):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I8):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U8):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R4):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R8):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_STRING):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_TYPEDBYREF):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CLASS):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_OBJECT):
            return executor.getTypeDefinitionFromIl2CppType(il2CppType);
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST): {
            auto genericClass = il2Cpp->mapVATRClass<Il2CppGenericClass>(il2CppType->genericClassPtr());
            return executor.getGenericClassTypeDefinition(&genericClass);
        }
        default:
            throw std::runtime_error("getTypeDefinition: unsupported type");
    }
}

void StructGenerator::createStructNameDic(const Il2CppTypeDefinition* typeDef) {
    std::string typeName = executor.getTypeDefName(typeDef, true, true);
    std::string typeStructName = fixName(typeName);
    std::string uniqueName = getUniqueName(typeStructName);
    structNameDic[static_cast<int>(typeDef - metadata->typeDefs.data())] = uniqueName;
}

std::string StructGenerator::getUniqueName(const std::string& name) {
    std::string fixName_ = name;
    int i = 1;
    while (!structNameHashSet.insert(fixName_).second) {
        fixName_ = name + "_" + std::to_string(i++);
    }
    return fixName_;
}

std::string StructGenerator::recursionStructInfo(const StructInfo* info) {
    if (!structCache.insert(info).second) {
        return "";
    }
    std::string sb;
    std::string pre;

    if (!info->parent.empty()) {
        std::string parentStructName = info->parent + "_o";
        auto parentIt = structInfoWithStructName.find(parentStructName);
        if (parentIt != structInfoWithStructName.end()) {
            pre += recursionStructInfo(parentIt->second);
        }
        sb += "struct " + info->typeName + "_Fields : " + info->parent + "_Fields {\n";
    } else {
        if (dynamic_cast<PE*>(il2Cpp) != nullptr && !info->isValueType) {
            if (il2Cpp->is32Bit) {
                sb += "struct __declspec(align(4)) " + info->typeName + "_Fields {\n";
            } else {
                sb += "struct __declspec(align(8)) " + info->typeName + "_Fields {\n";
            }
        } else {
            sb += "struct " + info->typeName + "_Fields {\n";
        }
    }
    for (const auto& field : info->fields) {
        if (field.isValueType) {
            auto fieldIt = structInfoWithStructName.find(field.fieldTypeName);
            if (fieldIt != structInfoWithStructName.end()) {
                pre += recursionStructInfo(fieldIt->second);
            }
        }
        if (field.isCustomType) {
            sb += "\tstruct " + field.fieldTypeName + " " + field.fieldName + ";\n";
        } else {
            sb += "\t" + field.fieldTypeName + " " + field.fieldName + ";\n";
        }
    }
    sb += "};\n";

    if (!info->rgctxs.empty()) {
        sb += "struct " + info->typeName + "_RGCTXs {\n";
        for (size_t i = 0; i < info->rgctxs.size(); i++) {
            const auto& rgctx = info->rgctxs[i];
            switch (rgctx.type) {
                case static_cast<int>(Il2CppRGCTXDataType::IL2CPP_RGCTX_DATA_TYPE):
                    sb += "\tIl2CppType* _" + std::to_string(i) + "_" + rgctx.typeName + ";\n";
                    break;
                case static_cast<int>(Il2CppRGCTXDataType::IL2CPP_RGCTX_DATA_CLASS):
                    sb += "\tIl2CppClass* _" + std::to_string(i) + "_" + rgctx.className + ";\n";
                    break;
                case static_cast<int>(Il2CppRGCTXDataType::IL2CPP_RGCTX_DATA_METHOD):
                    sb += "\tMethodInfo* _" + std::to_string(i) + "_" + rgctx.methodName + ";\n";
                    break;
            }
        }
        sb += "};\n";
    }

    if (!info->vtableMethod.empty()) {
        sb += "struct " + info->typeName + "_VTable {\n";
        for (size_t i = 0; i < info->vtableMethod.size(); i++) {
            sb += "\tVirtualInvokeData _" + std::to_string(i) + "_";
            if (!info->vtableMethod[i].methodName.empty()) {
                sb += info->vtableMethod[i].methodName;
            } else {
                sb += "unknown";
            }
            sb += ";\n";
        }
        sb += "};\n";
    }

    sb += "struct " + info->typeName + "_c {\n";
    sb += "\tIl2CppClass_1 _1;\n";
    if (!info->staticFields.empty()) {
        sb += "\tstruct " + info->typeName + "_StaticFields* static_fields;\n";
    } else {
        sb += "\tvoid* static_fields;\n";
    }
    if (!info->rgctxs.empty()) {
        sb += "\t" + info->typeName + "_RGCTXs* rgctx_data;\n";
    } else {
        sb += "\tIl2CppRGCTXData* rgctx_data;\n";
    }
    sb += "\tIl2CppClass_2 _2;\n";
    if (!info->vtableMethod.empty()) {
        sb += "\t" + info->typeName + "_VTable vtable;\n";
    } else {
        sb += "\tVirtualInvokeData vtable[32];\n";
    }
    sb += "};\n";

    sb += "struct " + info->typeName + "_o {\n";
    if (!info->isValueType) {
        sb += "\t" + info->typeName + "_c *klass;\n";
        sb += "\tvoid *monitor;\n";
    }
    sb += "\t" + info->typeName + "_Fields fields;\n";
    sb += "};\n";

    if (!info->staticFields.empty()) {
        sb += "struct " + info->typeName + "_StaticFields {\n";
        for (const auto& field : info->staticFields) {
            if (field.isValueType) {
                auto fieldIt = structInfoWithStructName.find(field.fieldTypeName);
                if (fieldIt != structInfoWithStructName.end()) {
                    pre += recursionStructInfo(fieldIt->second);
                }
            }
            if (field.isCustomType) {
                sb += "\tstruct " + field.fieldTypeName + " " + field.fieldName + ";\n";
            } else {
                sb += "\t" + field.fieldTypeName + " " + field.fieldName + ";\n";
            }
        }
        sb += "};\n";
    }

    return pre + sb;
}

std::string StructGenerator::getIl2CppStructName(const Il2CppType* il2CppType, const Il2CppGenericContext* context) {
    switch (il2CppType->type) {
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VOID):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_BOOLEAN):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CHAR):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I1):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U1):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I2):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U2):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I4):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U4):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I8):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U8):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R4):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_R8):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_STRING):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_TYPEDBYREF):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_I):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_U):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CLASS):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_OBJECT): {
            const Il2CppTypeDefinition* typeDef = executor.getTypeDefinitionFromIl2CppType(il2CppType);
            return structNameDic[static_cast<int>(typeDef - metadata->typeDefs.data())];
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_PTR): {
            const Il2CppType* oriType = il2Cpp->getIl2CppType(il2CppType->typePtr());
            return getIl2CppStructName(oriType);
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_ARRAY): {
            auto arrayType = il2Cpp->mapVATRClass<Il2CppArrayType>(il2CppType->arrayPtr());
            const Il2CppType* elementType = il2Cpp->getIl2CppType(arrayType.etype);
            std::string elementStructName = getIl2CppStructName(elementType, context);
            std::string typeStructName = elementStructName + "_array";
            if (structNameHashSet.insert(typeStructName).second) {
                parseArrayClassStruct(elementType, context);
            }
            return typeStructName;
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_SZARRAY): {
            const Il2CppType* elementType = il2Cpp->getIl2CppType(il2CppType->typePtr());
            std::string elementStructName = getIl2CppStructName(elementType, context);
            std::string typeStructName = elementStructName + "_array";
            if (structNameHashSet.insert(typeStructName).second) {
                parseArrayClassStruct(elementType, context);
            }
            return typeStructName;
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST): {
            std::string typeStructName = genericClassStructNameDic[il2CppType->genericClassPtr()];
            if (structNameHashSet.insert(typeStructName).second) {
                genericClassList.push_back(il2CppType->genericClassPtr());
            }
            return typeStructName;
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VAR): {
            if (context != nullptr) {
                const Il2CppGenericParameter* genericParameter = executor.getGenericParameteFromIl2CppType(il2CppType);
                auto genericInst = il2Cpp->mapVATRClass<Il2CppGenericInst>(context->class_inst);
                auto pointers = il2Cpp->mapVATRArray<uint64_t>(genericInst.type_argv, genericInst.type_argc);
                const Il2CppType* type = il2Cpp->getIl2CppType(pointers[static_cast<size_t>(genericParameter->num)]);
                return getIl2CppStructName(type);
            }
            return "System_Object";
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_MVAR): {
            if (context != nullptr) {
                const Il2CppGenericParameter* genericParameter = executor.getGenericParameteFromIl2CppType(il2CppType);
                auto genericInst = il2Cpp->mapVATRClass<Il2CppGenericInst>(context->method_inst);
                auto pointers = il2Cpp->mapVATRArray<uint64_t>(genericInst.type_argv, genericInst.type_argc);
                const Il2CppType* type = il2Cpp->getIl2CppType(pointers[static_cast<size_t>(genericParameter->num)]);
                return getIl2CppStructName(type);
            }
            return "System_Object";
        }
        default:
            throw std::runtime_error("getIl2CppStructName: unsupported type");
    }
}

bool StructGenerator::isValueType(const Il2CppType* il2CppType, const Il2CppGenericContext* context) {
    switch (il2CppType->type) {
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE): {
            const Il2CppTypeDefinition* typeDef = executor.getTypeDefinitionFromIl2CppType(il2CppType);
            return !typeDef->isEnum();
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST): {
            auto genericClass = il2Cpp->mapVATRClass<Il2CppGenericClass>(il2CppType->genericClassPtr());
            const Il2CppTypeDefinition* typeDef = executor.getGenericClassTypeDefinition(&genericClass);
            return typeDef->isValueType() && !typeDef->isEnum();
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VAR): {
            if (context != nullptr) {
                const Il2CppGenericParameter* genericParameter = executor.getGenericParameteFromIl2CppType(il2CppType);
                auto genericInst = il2Cpp->mapVATRClass<Il2CppGenericInst>(context->class_inst);
                auto pointers = il2Cpp->mapVATRArray<uint64_t>(genericInst.type_argv, genericInst.type_argc);
                const Il2CppType* type = il2Cpp->getIl2CppType(pointers[static_cast<size_t>(genericParameter->num)]);
                return isValueType(type, nullptr);
            }
            return false;
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_MVAR): {
            if (context != nullptr) {
                const Il2CppGenericParameter* genericParameter = executor.getGenericParameteFromIl2CppType(il2CppType);
                auto genericInst = il2Cpp->mapVATRClass<Il2CppGenericInst>(context->method_inst);
                auto pointers = il2Cpp->mapVATRArray<uint64_t>(genericInst.type_argv, genericInst.type_argc);
                const Il2CppType* type = il2Cpp->getIl2CppType(pointers[static_cast<size_t>(genericParameter->num)]);
                return isValueType(type, nullptr);
            }
            return false;
        }
        default:
            return false;
    }
}

bool StructGenerator::isCustomType(const Il2CppType* il2CppType, const Il2CppGenericContext* context) {
    switch (il2CppType->type) {
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_PTR): {
            const Il2CppType* oriType = il2Cpp->getIl2CppType(il2CppType->typePtr());
            return isCustomType(oriType, context);
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_STRING):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_CLASS):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_ARRAY):
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_SZARRAY):
            return true;
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VALUETYPE): {
            const Il2CppTypeDefinition* typeDef = executor.getTypeDefinitionFromIl2CppType(il2CppType);
            if (typeDef->isEnum()) {
                return isCustomType(&il2Cpp->types[static_cast<size_t>(typeDef->enumElementTypeIndex(il2Cpp->version))], context);
            }
            return true;
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_GENERICINST): {
            auto genericClass = il2Cpp->mapVATRClass<Il2CppGenericClass>(il2CppType->genericClassPtr());
            const Il2CppTypeDefinition* typeDef = executor.getGenericClassTypeDefinition(&genericClass);
            if (typeDef->isEnum()) {
                return isCustomType(&il2Cpp->types[static_cast<size_t>(typeDef->enumElementTypeIndex(il2Cpp->version))], context);
            }
            return true;
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_VAR): {
            if (context != nullptr) {
                const Il2CppGenericParameter* genericParameter = executor.getGenericParameteFromIl2CppType(il2CppType);
                auto genericInst = il2Cpp->mapVATRClass<Il2CppGenericInst>(context->class_inst);
                auto pointers = il2Cpp->mapVATRArray<uint64_t>(genericInst.type_argv, genericInst.type_argc);
                const Il2CppType* type = il2Cpp->getIl2CppType(pointers[static_cast<size_t>(genericParameter->num)]);
                return isCustomType(type, nullptr);
            }
            return false;
        }
        case static_cast<int>(Il2CppTypeEnum::IL2CPP_TYPE_MVAR): {
            if (context != nullptr) {
                const Il2CppGenericParameter* genericParameter = executor.getGenericParameteFromIl2CppType(il2CppType);
                auto genericInst = il2Cpp->mapVATRClass<Il2CppGenericInst>(context->method_inst);
                auto pointers = il2Cpp->mapVATRArray<uint64_t>(genericInst.type_argv, genericInst.type_argc);
                const Il2CppType* type = il2Cpp->getIl2CppType(pointers[static_cast<size_t>(genericParameter->num)]);
                return isCustomType(type, nullptr);
            }
            return false;
        }
        default:
            return false;
    }
}

void StructGenerator::generateMethodInfo(const std::string& methodInfoName, const std::string& structTypeName,
                                         const std::vector<StructRGCTXInfo>& rgctxs) {
    if (!rgctxs.empty()) {
        methodInfoHeader += "struct " + methodInfoName + "_RGCTXs {\n";
        for (size_t i = 0; i < rgctxs.size(); i++) {
            const auto& rgctx = rgctxs[i];
            switch (rgctx.type) {
                case static_cast<int>(Il2CppRGCTXDataType::IL2CPP_RGCTX_DATA_TYPE):
                    methodInfoHeader += "\tIl2CppType* _" + std::to_string(i) + "_" + rgctx.typeName + ";\n";
                    break;
                case static_cast<int>(Il2CppRGCTXDataType::IL2CPP_RGCTX_DATA_CLASS):
                    methodInfoHeader += "\tIl2CppClass* _" + std::to_string(i) + "_" + rgctx.className + ";\n";
                    break;
                case static_cast<int>(Il2CppRGCTXDataType::IL2CPP_RGCTX_DATA_METHOD):
                    methodInfoHeader += "\tMethodInfo* _" + std::to_string(i) + "_" + rgctx.methodName + ";\n";
                    break;
            }
        }
        methodInfoHeader += "};\n";
    }

    methodInfoHeader += "struct " + methodInfoName + " {\n";
    methodInfoHeader += "\tIl2CppMethodPointer methodPointer;\n";
    if (il2Cpp->version >= 29) {
        methodInfoHeader += "\tIl2CppMethodPointer virtualMethodPointer;\n";
        methodInfoHeader += "\tInvokerMethod invoker_method;\n";
    } else {
        methodInfoHeader += "\tvoid* invoker_method;\n";
    }
    methodInfoHeader += "\tconst char* name;\n";
    if (il2Cpp->version <= 24) {
        methodInfoHeader += "\t" + structTypeName + "_c *declaring_type;\n";
    } else {
        methodInfoHeader += "\t" + structTypeName + "_c *klass;\n";
    }
    methodInfoHeader += "\tconst Il2CppType *return_type;\n";
    if (il2Cpp->version >= 29) {
        methodInfoHeader += "\tconst Il2CppType** parameters;\n";
    } else {
        methodInfoHeader += "\tconst void* parameters;\n";
    }
    if (!rgctxs.empty()) {
        methodInfoHeader += "\tconst " + methodInfoName + "_RGCTXs* rgctx_data;\n";
    } else {
        methodInfoHeader += "\tconst Il2CppRGCTXData* rgctx_data;\n";
    }
    methodInfoHeader += "\tunion\n";
    methodInfoHeader += "\t{\n";
    methodInfoHeader += "\t\tconst void* genericMethod;\n";
    if (il2Cpp->version >= 27) {
        methodInfoHeader += "\t\tconst void* genericContainerHandle;\n";
    } else {
        methodInfoHeader += "\t\tconst void* genericContainer;\n";
    }
    methodInfoHeader += "\t};\n";
    if (il2Cpp->version <= 24) {
        methodInfoHeader += "\tint32_t customAttributeIndex;\n";
    }
    methodInfoHeader += "\tuint32_t token;\n";
    methodInfoHeader += "\tuint16_t flags;\n";
    methodInfoHeader += "\tuint16_t iflags;\n";
    methodInfoHeader += "\tuint16_t slot;\n";
    methodInfoHeader += "\tuint8_t parameters_count;\n";
    methodInfoHeader += "\tuint8_t bitflags;\n";
    methodInfoHeader += "};\n";
}

}
