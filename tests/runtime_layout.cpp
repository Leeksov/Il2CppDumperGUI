#include "../NativeEngine/src/il2cpp/il2cpp_classes.h"
#include <cassert>
#include <cstring>
#include <vector>

static void word(std::vector<uint8_t>& bytes, size_t index, uint64_t value) {
    std::memcpy(bytes.data() + index * 8, &value, sizeof(value));
}

int main() {
    std::vector<uint8_t> registration(14 * 8);
    word(registration, 4, 123); // v108 types count: two table pairs were removed
    word(registration, 6, 456); // field offsets count
    word(registration, 12, 7); // always-initialized metadata usages
    il2cppdumper::BinaryStream metadata(registration);
    metadata.version = 108;
    auto record = metadata.readClassAt<il2cppdumper::Il2CppMetadataRegistration>(0);
    assert(record.typesCount == 123);
    assert(record.fieldOffsetsCount == 456);
    assert(record.alwaysInitMetadataUsagesCount == 7);

    std::vector<uint8_t> module(11 * 8);
    word(module, 0, 0x1111);
    word(module, 5, 9); // reverse P/Invoke count follows adjustor thunks
    word(module, 7, 0x2222); // debugger metadata
    il2cppdumper::BinaryStream code(module);
    code.version = 108;
    auto entry = code.readClassAt<il2cppdumper::Il2CppCodeGenModule>(0);
    assert(entry.moduleName == 0x1111);
    assert(entry.reversePInvokeWrapperCount == 9);
    assert(entry.debuggerMetadata == 0x2222);
}
