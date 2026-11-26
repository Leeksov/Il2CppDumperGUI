#include "formats/webassembly.h"

#include "utils/section_helper.h"

#include <stdexcept>

namespace il2cppdumper {

WebAssembly::WebAssembly(std::vector<uint8_t> data) : BinaryStream(std::move(data)) {
    is32Bit = true;
    uint32_t magic = readU32();
    int32_t wasmVersion = readI32();
    (void)magic;
    (void)wasmVersion;
    while (position() < length()) {
        uint32_t id = readULEB128();
        uint32_t len = readULEB128();
        if (id == 11) {
            uint32_t count = readULEB128();
            dataSections.resize(count);
            for (uint32_t i = 0; i < count; i++) {
                dataSections[i].index = readULEB128();
                uint8_t opCode = readU8();
                if (opCode != 0x41) {
                    throw std::runtime_error("WebAssembly: unsupported data section opcode");
                }
                dataSections[i].offset = readULEB128();
                opCode = readU8();
                if (opCode != 0xB) {
                    throw std::runtime_error("WebAssembly: unsupported data section opcode");
                }
                dataSections[i].data = readBytes(readULEB128());
            }
            break;
        }
        seek(len);
    }
    if (dataSections.empty()) {
        throw std::runtime_error("WebAssembly: no data sections found");
    }
}

std::unique_ptr<Il2Cpp> WebAssembly::createMemory() {
    const auto& last = dataSections.back();
    uint32_t bssStart = last.offset + static_cast<uint32_t>(last.data.size());
    std::vector<uint8_t> stream(length(), 0);
    for (const auto& dataSection : dataSections) {
        if (dataSection.offset < stream.size()) {
            size_t n = dataSection.data.size();
            if (static_cast<uint64_t>(dataSection.offset) + n > stream.size()) {
                n = stream.size() - dataSection.offset;
            }
            std::copy(dataSection.data.begin(), dataSection.data.begin() + static_cast<std::ptrdiff_t>(n),
                      stream.begin() + dataSection.offset);
        }
    }
    return std::make_unique<WebAssemblyMemory>(std::move(stream), bssStart);
}

WebAssemblyMemory::WebAssemblyMemory(std::vector<uint8_t> data, uint32_t bssStart)
    : Il2Cpp(std::move(data)), bssStart(bssStart) {
    is32Bit = true;
}

bool WebAssemblyMemory::plusSearch(int methodCount, int typeDefinitionsCount, int imageCount) {
    auto sectionHelper = getSectionHelper(methodCount, typeDefinitionsCount, imageCount);
    uint64_t codeRegistration = sectionHelper.findCodeRegistration();
    uint64_t metadataRegistration = sectionHelper.findMetadataRegistration();
    return autoPlusInit(codeRegistration, metadataRegistration);
}

SectionHelper WebAssemblyMemory::getSectionHelper(int methodCount, int typeDefinitionsCount, int imageCount) {
    SearchSection exec;
    exec.offset = 0;
    exec.offsetEnd = static_cast<uint64_t>(methodCount);
    exec.address = 0;
    exec.addressEnd = static_cast<uint64_t>(methodCount);
    SearchSection data;
    data.offset = 1024;
    data.offsetEnd = length();
    data.address = 1024;
    data.addressEnd = length();
    SearchSection bss;
    bss.offset = bssStart;
    bss.offsetEnd = 0x7FFFFFFFFFFFFFFFull;
    bss.address = bssStart;
    bss.addressEnd = 0x7FFFFFFFFFFFFFFFull;
    SectionHelper sectionHelper(this, methodCount, typeDefinitionsCount, metadataUsagesCount, imageCount);
    sectionHelper.setSection(SearchSectionType::Exec, std::vector<SearchSection>{exec});
    sectionHelper.setSection(SearchSectionType::Data, std::vector<SearchSection>{data});
    sectionHelper.setSection(SearchSectionType::Bss, std::vector<SearchSection>{bss});
    return sectionHelper;
}

}
