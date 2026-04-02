#pragma once
#include "il2cpp_il2cpp.h"

// String escape helper
std::string ToEscapedString(const std::string& s);

class Il2CppExecutor {
public:
    Metadata* metadata;
    Il2Cpp* il2Cpp;
    std::vector<uint64_t> customAttributeGenerators;

    Il2CppExecutor(Metadata* metadata, Il2Cpp* il2Cpp);

    std::string GetTypeName(const Il2CppType& il2CppType, bool addNamespace, bool is_nested);
    std::string GetTypeDefName(const Il2CppTypeDefinition& typeDef, bool addNamespace, bool genericParameter);
    std::string GetGenericInstParams(const Il2CppGenericInst& genericInst);
    std::string GetGenericContainerParams(const Il2CppGenericContainer& genericContainer);

    Il2CppTypeDefinition* GetGenericClassTypeDefinition(const Il2CppGenericClass& genericClass);
    Il2CppTypeDefinition* GetTypeDefinitionFromIl2CppType(const Il2CppType& il2CppType);
    Il2CppGenericParameter* GetGenericParameteFromIl2CppType(const Il2CppType& il2CppType);

    bool TryGetDefaultValue(int typeIndex, int dataIndex, std::string& valueStr);

    std::pair<std::string, std::string> GetMethodSpecName(const Il2CppMethodSpec& methodSpec, bool addNamespace = false);

    int SizeOfTypeDef() const { return metadata->SizeOfTypeDef(); }
    int SizeOfGenericParameter() const { return metadata->SizeOfGenericParameter(); }

private:
    static const std::map<int, std::string> TypeString;
};

class Il2CppDecompiler {
public:
    Il2CppDecompiler(Il2CppExecutor* executor);
    std::string Decompile(const Il2CppDumperConfig& config);

private:
    Il2CppExecutor* executor_;
    Metadata* metadata_;
    Il2Cpp* il2Cpp_;
    std::map<int, std::string> methodModifiers_;

    std::string GetCustomAttribute(int imageIndex, int customAttributeIndex, uint32_t token, const std::string& padding = "");
    std::string GetModifiers(int methodDefIndex);
};
