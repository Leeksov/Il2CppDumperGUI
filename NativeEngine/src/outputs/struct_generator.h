#pragma once

#include "utils/il2cpp_executor.h"
#include "outputs/script_json.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace il2cppdumper {

struct StructFieldInfo {
    std::string fieldTypeName;
    std::string fieldName;
    bool isValueType = false;
    bool isCustomType = false;
};

struct StructVTableMethodInfo {
    std::string methodName;
};

struct StructRGCTXInfo {
    int type = 0;
    std::string typeName;
    std::string className;
    std::string methodName;
};

struct StructInfo {
    std::string typeName;
    bool isValueType = false;
    std::string parent;
    std::vector<StructFieldInfo> fields;
    std::vector<StructFieldInfo> staticFields;
    std::vector<StructVTableMethodInfo> vtableMethod;
    std::vector<StructRGCTXInfo> rgctxs;
};

class StructGenerator {
public:
    explicit StructGenerator(Il2CppExecutor& executor);

    void writeScript(const std::string& outputDir);

private:
    void addMetadataUsageTypeInfo(ScriptJson& json, uint32_t index, uint64_t address);
    void addMetadataUsageIl2CppType(ScriptJson& json, uint32_t index, uint64_t address);
    void addMetadataUsageMethodDef(ScriptJson& json, uint32_t index, uint64_t address);
    void addMetadataUsageFieldInfo(ScriptJson& json, uint32_t index, uint64_t address);
    void addMetadataUsageStringLiteral(ScriptJson& json, uint32_t index, uint64_t address);
    void addMetadataUsageMethodRef(ScriptJson& json, uint32_t index, uint64_t address);

    static std::string fixName(const std::string& str);
    std::string parseType(const Il2CppType* il2CppType, const Il2CppGenericContext* context = nullptr);
    static std::string getMethodTypeSignature(const std::vector<int>& types);

    void addStruct(int typeDefIndex);
    void addGenericClassStruct(uint64_t pointer);
    void addParents(const Il2CppTypeDefinition* typeDef, StructInfo& structInfo);
    void addFields(const Il2CppTypeDefinition* typeDef, StructInfo& structInfo, const Il2CppGenericContext* context);
    void addVTableMethod(StructInfo& structInfo, const Il2CppTypeDefinition* typeDef);
    void addRGCTX(StructInfo& structInfo, int typeDefIndex);
    std::vector<StructRGCTXInfo> generateRGCTX(const std::string& imageName, const Il2CppMethodDefinition* methodDef);
    void parseArrayClassStruct(const Il2CppType* il2CppType, const Il2CppGenericContext* context);
    const Il2CppTypeDefinition* getTypeDefinition(const Il2CppType* il2CppType);
    void createStructNameDic(const Il2CppTypeDefinition* typeDef);
    std::string getUniqueName(const std::string& name);
    std::string recursionStructInfo(const StructInfo* info);
    std::string getIl2CppStructName(const Il2CppType* il2CppType, const Il2CppGenericContext* context = nullptr);
    bool isValueType(const Il2CppType* il2CppType, const Il2CppGenericContext* context);
    bool isCustomType(const Il2CppType* il2CppType, const Il2CppGenericContext* context);
    void generateMethodInfo(const std::string& methodInfoName, const std::string& structTypeName,
                            const std::vector<StructRGCTXInfo>& rgctxs);

    Il2CppExecutor& executor;
    Metadata* metadata;
    Il2Cpp* il2Cpp;
    std::unordered_map<int, std::string> typeDefImageNames;
    std::unordered_set<std::string> structNameHashSet;
    std::vector<StructInfo> structInfoList;
    std::unordered_map<std::string, StructInfo*> structInfoWithStructName;
    std::unordered_set<const StructInfo*> structCache;
    std::unordered_map<int, std::string> structNameDic;
    std::unordered_map<uint64_t, std::string> genericClassStructNameDic;
    std::unordered_map<std::string, const Il2CppType*> nameGenericClassDic;
    std::vector<uint64_t> genericClassList;
    std::string arrayClassHeader;
    std::string methodInfoHeader;
    static std::unordered_set<uint64_t> methodInfoCache;
};

}
