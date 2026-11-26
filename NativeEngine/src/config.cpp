#include "config.h"

#include <fstream>

#include <nlohmann/json.hpp>

namespace il2cppdumper {

namespace {

bool getBool(const nlohmann::json& j, const char* key, bool def) {
    auto it = j.find(key);
    if (it != j.end() && it->is_boolean()) {
        return it->get<bool>();
    }
    return def;
}

double getDouble(const nlohmann::json& j, const char* key, double def) {
    auto it = j.find(key);
    if (it != j.end() && it->is_number()) {
        return it->get<double>();
    }
    return def;
}

}

Config loadConfig(const std::string& path) {
    Config cfg;
    std::ifstream in(path);
    if (!in) {
        return cfg;
    }
    try {
        nlohmann::json j;
        in >> j;
        cfg.DumpMethod = getBool(j, "DumpMethod", cfg.DumpMethod);
        cfg.DumpField = getBool(j, "DumpField", cfg.DumpField);
        cfg.DumpProperty = getBool(j, "DumpProperty", cfg.DumpProperty);
        cfg.DumpAttribute = getBool(j, "DumpAttribute", cfg.DumpAttribute);
        cfg.DumpFieldOffset = getBool(j, "DumpFieldOffset", cfg.DumpFieldOffset);
        cfg.DumpMethodOffset = getBool(j, "DumpMethodOffset", cfg.DumpMethodOffset);
        cfg.DumpTypeDefIndex = getBool(j, "DumpTypeDefIndex", cfg.DumpTypeDefIndex);
        cfg.GenerateDummyDll = getBool(j, "GenerateDummyDll", cfg.GenerateDummyDll);
        cfg.GenerateStruct = getBool(j, "GenerateStruct", cfg.GenerateStruct);
        cfg.DummyDllAddToken = getBool(j, "DummyDllAddToken", cfg.DummyDllAddToken);
        cfg.RequireAnyKey = getBool(j, "RequireAnyKey", cfg.RequireAnyKey);
        cfg.ForceIl2CppVersion = getBool(j, "ForceIl2CppVersion", cfg.ForceIl2CppVersion);
        cfg.ForceVersion = getDouble(j, "ForceVersion", cfg.ForceVersion);
        cfg.ForceDump = getBool(j, "ForceDump", cfg.ForceDump);
        cfg.NoRedirectedPointer = getBool(j, "NoRedirectedPointer", cfg.NoRedirectedPointer);
    } catch (...) {
    }
    return cfg;
}

}
