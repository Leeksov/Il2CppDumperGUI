#pragma once

#include "il2cpp/il2cpp.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace il2cppdumper {

struct DataSection {
    uint32_t index = 0;
    uint32_t offset = 0;
    std::vector<uint8_t> data;
};

class WebAssembly : public BinaryStream {
public:
    explicit WebAssembly(std::vector<uint8_t> data);

    std::unique_ptr<Il2Cpp> createMemory();

private:
    std::vector<DataSection> dataSections;
};

class WebAssemblyMemory : public Il2Cpp {
public:
    WebAssemblyMemory(std::vector<uint8_t> data, uint32_t bssStart);

    uint64_t mapVATR(uint64_t addr) override { return addr; }
    uint64_t mapRTVA(uint64_t addr) override { return addr; }
    bool search() override { return false; }
    bool plusSearch(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool symbolSearch() override { return false; }
    SectionHelper getSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) override;
    bool checkDump() override { return false; }

private:
    uint32_t bssStart = 0;
};

}
