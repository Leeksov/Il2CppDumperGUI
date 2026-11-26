#pragma once


#include "outputs/assembly_writer.h"
#include "utils/il2cpp_executor.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace il2cppdumper {

class DummyAssemblyGenerator {
public:
    struct Output {
        std::string fileName;
        std::vector<uint8_t> bytes;
    };

    DummyAssemblyGenerator(Il2CppExecutor& executor, bool addToken);
    ~DummyAssemblyGenerator();
    DummyAssemblyGenerator(DummyAssemblyGenerator&&) noexcept;

    std::vector<Output> assemblies;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
