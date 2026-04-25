#include "il2cpp_bridge.h"
#include "il2cpp_executor.h"
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <sys/wait.h>

static std::string lastError;

const char* il2cpp_get_last_error(void) {
    return lastError.c_str();
}

static std::vector<uint8_t> readFile(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) throw std::runtime_error(std::string("Cannot open file: ") + path);
    auto size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(size);
    f.read((char*)data.data(), size);
    return data;
}

static uint64_t parseHex(const char* str) {
    if (!str || str[0] == '\0') return 0;
    return strtoull(str, nullptr, 16);
}

// Core implementation
static int il2cpp_dump_impl(
    const char* il2cppPath,
    const char* metadataPath,
    const char* outputDir,
    const Il2CppDumperConfig& config,
    uint64_t manualCodeReg,
    uint64_t manualMetaReg,
    const char* postProcessToolPath,
    Il2CppLogCallback logCallback,
    void* logContext)
{
    lastError.clear();

    auto logFunc = [logCallback, logContext](const std::string& msg) {
        if (logCallback) logCallback(msg.c_str(), logContext);
    };

    try {
        logFunc("Initializing metadata...");
        auto metadataBytes = readFile(metadataPath);
        Metadata metadata(metadataBytes, logFunc);
        logFunc("Metadata Version: " + std::to_string(metadata.Version));

        logFunc("Initializing il2cpp file...");
        auto il2cppBytes = readFile(il2cppPath);
        if (il2cppBytes.size() < 4) {
            throw std::runtime_error("il2cpp file too small");
        }

        uint32_t magic = 0;
        memcpy(&magic, il2cppBytes.data(), 4);

        std::unique_ptr<Il2Cpp> il2Cpp;

        switch (magic) {
            case 0x905A4D: // PE
                il2Cpp = std::make_unique<PE>(il2cppBytes, logFunc);
                break;
            case 0x464c457f: { // ELF
                if (il2cppBytes.size() > 4 && il2cppBytes[4] == 2)
                    il2Cpp = std::make_unique<Elf64Impl>(il2cppBytes, logFunc);
                else
                    il2Cpp = std::make_unique<Elf32>(il2cppBytes, logFunc);
                break;
            }
            case 0xCAFEBABE:
            case 0xBEBAFECA: { // FAT Mach-O
                auto fatInfo = MachoFatInfo::Parse(il2cppBytes);
                int index = -1;
                for (int i = 0; i < (int)fatInfo.fats.size(); i++) {
                    if (fatInfo.fats[i].magic == 0xFEEDFACF) { index = i; break; }
                }
                if (index == -1 && !fatInfo.fats.empty()) index = 0;
                if (index == -1) throw std::runtime_error("No valid Mach-O in fat binary");
                auto machoData = MachoFatInfo::GetMacho(il2cppBytes, fatInfo.fats[index]);
                uint32_t innerMagic = 0;
                memcpy(&innerMagic, machoData.data(), 4);
                if (innerMagic == 0xFEEDFACF)
                    il2Cpp = std::make_unique<Macho64>(machoData, logFunc);
                else
                    il2Cpp = std::make_unique<Macho>(machoData, logFunc);
                break;
            }
            case 0xFEEDFACF:
                il2Cpp = std::make_unique<Macho64>(il2cppBytes, logFunc);
                break;
            case 0xFEEDFACE:
                il2Cpp = std::make_unique<Macho>(il2cppBytes, logFunc);
                break;
            default:
                throw std::runtime_error("ERROR: il2cpp file not supported. Magic: 0x" +
                    ([](uint32_t v) { char b[16]; snprintf(b, sizeof(b), "%X", v); return std::string(b); })(magic));
        }

        double version = config.ForceIl2CppVersion ? config.ForceVersion : metadata.Version;
        il2Cpp->SetProperties(version, metadata.metadataUsagesCount);
        logFunc("Il2Cpp Version: " + std::to_string(il2Cpp->Version));

        // Check dump / ELF ImageBase
        if (config.ForceDump || il2Cpp->CheckDump()) {
            auto elfBase = dynamic_cast<ElfBase*>(il2Cpp.get());
            if (elfBase) {
                il2Cpp->IsDumped = true;
                if (!config.NoRedirectedPointer) {
                    elfBase->Reload();
                }
            }
        }

        // Search
        logFunc("Searching...");
        bool flag = false;
        try {
            int methodCount = 0;
            for (auto& m : metadata.methodDefs) {
                if (m.methodIndex >= 0) methodCount++;
            }
            flag = il2Cpp->PlusSearch(methodCount, (int)metadata.typeDefs.size(), (int)metadata.imageDefs.size());
            if (!flag) flag = il2Cpp->Search();
            if (!flag) flag = il2Cpp->SymbolSearch();

            // Manual fallback
            if (!flag && manualCodeReg != 0 && manualMetaReg != 0) {
                logFunc("Using manual CodeRegistration/MetadataRegistration...");
                char buf[64];
                snprintf(buf, sizeof(buf), "CodeRegistration : %llx", (unsigned long long)manualCodeReg);
                logFunc(buf);
                snprintf(buf, sizeof(buf), "MetadataRegistration : %llx", (unsigned long long)manualMetaReg);
                logFunc(buf);
                il2Cpp->Init(manualCodeReg, manualMetaReg);
                flag = true;
            }

            if (!flag) {
                throw std::runtime_error("Cannot find CodeRegistration and MetadataRegistration. Try manual mode.");
            }

            if (il2Cpp->Version >= 27 && il2Cpp->IsDumped) {
                auto& typeDef = metadata.typeDefs[0];
                auto& il2CppType = il2Cpp->types[typeDef.byvalTypeIndex];
                metadata.ImageBase = il2CppType.data_dummy - metadata.header.typeDefinitionsOffset;
            }
        } catch (std::exception& ex) {
            lastError = ex.what();
            logFunc(std::string("Error during search: ") + ex.what());
            return 2;
        }

        // Dump
        logFunc("Dumping...");
        auto executor = std::make_unique<Il2CppExecutor>(&metadata, il2Cpp.get());
        auto decompiler = std::make_unique<Il2CppDecompiler>(executor.get());
        auto result = decompiler->Decompile(config);

        std::string outPath = std::string(outputDir);
        if (!outPath.empty() && outPath.back() != '/') outPath += '/';
        std::string dumpPath = outPath + "dump.cs";

        std::ofstream outFile(dumpPath, std::ios::binary);
        if (!outFile.is_open()) {
            throw std::runtime_error("Cannot write to: " + dumpPath);
        }
        outFile.write(result.data(), result.size());
        outFile.close();
        logFunc("dump.cs written to: " + dumpPath);

        // Run post-process tool (StructGenerator + DummyDLL) if available
        if (postProcessToolPath && postProcessToolPath[0] != '\0') {
            std::string toolPath(postProcessToolPath);
            // Ensure execute permission, remove quarantine, ad-hoc re-sign
            // (macOS 14+ kills unsigned bundled binaries with SIGKILL = exit 137)
            system(("chmod +x \"" + toolPath + "\" 2>/dev/null").c_str());
            system(("xattr -dr com.apple.quarantine \"" + toolPath + "\" 2>/dev/null").c_str());
            system(("codesign --force --sign - \"" + toolPath + "\" 2>/dev/null").c_str());

            logFunc("Running post-process (struct + dummy dll)...");
            std::string cmd = "\"" + toolPath + "\" "
                + "\"" + il2cppPath + "\" "
                + "\"" + metadataPath + "\" "
                + "\"" + outPath + "\" 2>&1";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (pipe) {
                char buf[512];
                while (fgets(buf, sizeof(buf), pipe) != nullptr) {
                    std::string line(buf);
                    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
                    if (!line.empty()) logFunc("[PostProcess] " + line);
                }
                int ret = pclose(pipe);
                int exitCode = WEXITSTATUS(ret);
                if (ret == 0) {
                    logFunc("Post-process complete!");
                } else if (exitCode == 137) {
                    logFunc("Post-process killed by macOS (signature issue). Try running:");
                    logFunc("  sudo xattr -dr com.apple.quarantine /Applications/Il2CppDumperGUI.app");
                    logFunc("  sudo codesign --force --deep --sign - /Applications/Il2CppDumperGUI.app");
                } else {
                    logFunc("Post-process failed (exit code " + std::to_string(exitCode) + ")");
                }
            } else {
                logFunc("Post-process: failed to launch tool");
            }
        }

        logFunc("Done!");
        return 0;

    } catch (std::exception& ex) {
        lastError = ex.what();
        logFunc(std::string("ERROR: ") + ex.what());
        return 1;
    }
}

// Simple API (backwards compatible)
int il2cpp_dump(
    const char* il2cppPath,
    const char* metadataPath,
    const char* outputDir,
    int dumpMethod, int dumpField, int dumpProperty, int dumpAttribute,
    int dumpFieldOffset, int dumpMethodOffset, int dumpTypeDefIndex,
    int forceIl2CppVersion, double forceVersion, int forceDump,
    Il2CppLogCallback logCallback, void* logContext)
{
    Il2CppDumperConfig config;
    config.DumpMethod = dumpMethod != 0;
    config.DumpField = dumpField != 0;
    config.DumpProperty = dumpProperty != 0;
    config.DumpAttribute = dumpAttribute != 0;
    config.DumpFieldOffset = dumpFieldOffset != 0;
    config.DumpMethodOffset = dumpMethodOffset != 0;
    config.DumpTypeDefIndex = dumpTypeDefIndex != 0;
    config.ForceIl2CppVersion = forceIl2CppVersion != 0;
    config.ForceVersion = forceVersion;
    config.ForceDump = forceDump != 0;

    return il2cpp_dump_impl(il2cppPath, metadataPath, outputDir, config, 0, 0, nullptr, logCallback, logContext);
}

// Extended API with manual addresses and post-process
int il2cpp_dump_ex(
    const char* il2cppPath,
    const char* metadataPath,
    const char* outputDir,
    int dumpMethod, int dumpField, int dumpProperty, int dumpAttribute,
    int dumpFieldOffset, int dumpMethodOffset, int dumpTypeDefIndex,
    int forceIl2CppVersion, double forceVersion, int forceDump,
    const char* manualCodeRegistration,
    const char* manualMetadataRegistration,
    const char* postProcessToolPath,
    Il2CppLogCallback logCallback, void* logContext)
{
    Il2CppDumperConfig config;
    config.DumpMethod = dumpMethod != 0;
    config.DumpField = dumpField != 0;
    config.DumpProperty = dumpProperty != 0;
    config.DumpAttribute = dumpAttribute != 0;
    config.DumpFieldOffset = dumpFieldOffset != 0;
    config.DumpMethodOffset = dumpMethodOffset != 0;
    config.DumpTypeDefIndex = dumpTypeDefIndex != 0;
    config.ForceIl2CppVersion = forceIl2CppVersion != 0;
    config.ForceVersion = forceVersion;
    config.ForceDump = forceDump != 0;

    uint64_t codeReg = parseHex(manualCodeRegistration);
    uint64_t metaReg = parseHex(manualMetadataRegistration);

    return il2cpp_dump_impl(il2cppPath, metadataPath, outputDir, config, codeReg, metaReg, postProcessToolPath, logCallback, logContext);
}
