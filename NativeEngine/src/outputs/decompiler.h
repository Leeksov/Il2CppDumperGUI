#pragma once

#include "config.h"
#include "il2cpp/il2cpp.h"
#include "utils/il2cpp_executor.h"
#include "il2cpp/metadata.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace il2cppdumper {

class Il2CppDecompiler {
public:
    explicit Il2CppDecompiler(Il2CppExecutor& executor);

    void decompile(const Config& config, const std::string& outputDir);

private:
    std::string getCustomAttribute(int imageIndex, int customAttributeIndex, uint32_t token,
                                   const std::string& padding = "");
    std::string getModifiers(const Il2CppMethodDefinition* methodDef);
    std::string defaultValueString(const BlobValue& value);

    Il2CppExecutor& executor;
    Metadata* metadata;
    Il2Cpp* il2Cpp;
    std::unordered_map<int, std::string> methodModifiers;
};

}
