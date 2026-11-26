#pragma once

#include "utils/blob_value.h"
#include "il2cpp/il2cpp.h"
#include "il2cpp/metadata.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace il2cppdumper {

class Il2CppExecutor {
public:
    Metadata* metadata;
    Il2Cpp* il2Cpp;
    std::vector<uint64_t> customAttributeGenerators;

    explicit Il2CppExecutor(Metadata& metadata, Il2Cpp& il2Cpp);

    std::string getTypeName(const Il2CppType* il2CppType, bool addNamespace, bool isNested);
    std::string getTypeDefName(const Il2CppTypeDefinition* typeDef, bool addNamespace, bool genericParameter);
    std::string getGenericInstParams(const Il2CppGenericInst* genericInst);
    std::string getGenericContainerParams(const Il2CppGenericContainer* genericContainer);
    std::pair<std::string, std::string> getMethodSpecName(const Il2CppMethodSpec* methodSpec, bool addNamespace);
    Il2CppGenericContext getMethodSpecGenericContext(const Il2CppMethodSpec* methodSpec);

    std::vector<Il2CppRGCTXDefinition> getRGCTXDefinition(const std::string& imageName,
                                                          const Il2CppTypeDefinition& typeDef);
    std::vector<Il2CppRGCTXDefinition> getRGCTXDefinition(const std::string& imageName,
                                                          const Il2CppMethodDefinition& methodDef);

    const Il2CppTypeDefinition* getGenericClassTypeDefinition(const Il2CppGenericClass* genericClass);
    const Il2CppTypeDefinition* getTypeDefinitionFromIl2CppType(const Il2CppType* il2CppType);
    const Il2CppGenericParameter* getGenericParameteFromIl2CppType(const Il2CppType* il2CppType);

    SectionHelper getSectionHelper();

    bool tryGetDefaultValue(int typeIndex, int dataIndex, BlobValue& value);
    bool getConstantValueFromBlob(BinaryStream& reader, int type, BlobValue& value);
    int readEncodedTypeEnum(BinaryStream& reader, uint32_t& enumTypeIndex);

private:
    static std::string join(const std::vector<std::string>& parts, const char* sep);
};

}
