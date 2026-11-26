#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace il2cppdumper {

struct ScriptMethod {
    uint64_t Address = 0;
    std::string Name;
    std::string Signature;
    std::string TypeSignature;
};

struct ScriptString {
    uint64_t Address = 0;
    std::string Value;
};

struct ScriptMetadata {
    uint64_t Address = 0;
    std::string Name;
    std::string Signature;
};

struct ScriptMetadataMethod {
    uint64_t Address = 0;
    std::string Name;
    uint64_t MethodAddress = 0;
};

struct ScriptJson {
    std::vector<ScriptMethod> methodList;
    std::vector<ScriptString> stringList;
    std::vector<ScriptMetadata> metadataList;
    std::vector<ScriptMetadataMethod> metadataMethodList;
    std::vector<uint64_t> Addresses;
};

}
