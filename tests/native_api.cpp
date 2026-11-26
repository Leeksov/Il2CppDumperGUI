#include "../NativeEngine/include/il2cpp_native.h"
#include <cassert>
#include <string>
#include <vector>

static void onLog(const char* message, void* context) {
    static_cast<std::vector<std::string>*>(context)->emplace_back(message);
}

int main() {
    std::vector<std::string> messages;
    int result = il2cpp_dump_native("/missing/executable", "/missing/metadata",
                                    ".build/smoke-output", "NativeEngine/config.json",
                                    nullptr, nullptr, onLog, &messages);
    assert(result != 0);
    assert(!messages.empty());
    assert(messages.back().find("Cannot open file") != std::string::npos);
}
