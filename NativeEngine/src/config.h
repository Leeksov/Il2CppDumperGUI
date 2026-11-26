#pragma once

#include <string>

namespace il2cppdumper {

struct Config {
    bool DumpMethod = true;
    bool DumpField = true;
    bool DumpProperty = false;
    bool DumpAttribute = false;
    bool DumpFieldOffset = true;
    bool DumpMethodOffset = true;
    bool DumpTypeDefIndex = true;
    bool GenerateDummyDll = false;
    bool GenerateStruct = true;
    bool DummyDllAddToken = true;
    bool RequireAnyKey = true;
    bool ForceIl2CppVersion = false;
    double ForceVersion = 24.3;
    bool ForceDump = false;
    bool NoRedirectedPointer = false;
};

Config loadConfig(const std::string& path);

}
