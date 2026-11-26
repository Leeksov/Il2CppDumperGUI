
#include "config.h"
#include "il2cpp_native.h"
#include "outputs/decompiler.h"
#include "outputs/dummy_assembly_generator.h"
#include "formats/elf.h"
#include "formats/elf64.h"
#include "utils/il2cpp_executor.h"
#include "formats/macho.h"
#include "formats/macho64.h"
#include "formats/macho_fat.h"
#include "il2cpp/metadata.h"
#include "il2cpp/unity_version.h"
#include "formats/nso.h"
#include "formats/pe.h"
#include "formats/pe_loader.h"
#include "outputs/struct_generator.h"
#include "formats/webassembly.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <streambuf>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace il2cppdumper {
namespace {

struct RunOptions {
    uint64_t codeRegistration = 0;
    uint64_t metadataRegistration = 0;
    uint64_t dumpBase = 0;
    bool nonInteractive = false;
};


std::vector<uint8_t> readFileBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    in.seekg(0, std::ios::end);
    auto size = static_cast<size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(size);
    if (size > 0) {
        in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    }
    return data;
}

uint32_t readU32LE(const std::vector<uint8_t>& b) {
    if (b.size() < 4) {
        return 0;
    }
    return static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
           (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
}

uint64_t readHexLine() {
    std::string line;
    if (!std::getline(std::cin, line)) {
        return 0;
    }
    size_t start = 0;
    while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) {
        start++;
    }
    size_t end = line.size();
    while (end > start && std::isspace(static_cast<unsigned char>(line[end - 1]))) {
        end--;
    }
    line = line.substr(start, end - start);
    if (line.rfind("0x", 0) == 0 || line.rfind("0X", 0) == 0) {
        line = line.substr(2);
    }
    try {
        return std::stoull(line, nullptr, 16);
    } catch (...) {
        return 0;
    }
}

bool process(const Config& config, const std::string& il2cppPath, const std::string& metadataPath,
             const std::string& outputDir, const RunOptions& options) {
    std::cout << "Initializing metadata...\n";
    auto metadataBytes = readFileBytes(metadataPath);
    auto metadata = std::make_unique<Metadata>(std::move(metadataBytes));
    std::cout << "Metadata Version: " << metadata->version << "\n";
    std::cout << "Detected Unity Version: " << unityVersionRange(metadata->version) << "\n";

    std::cout << "Initializing il2cpp file...\n";
    auto il2CppBytes = readFileBytes(il2cppPath);
    auto magic = readU32LE(il2CppBytes);
    std::unique_ptr<Il2Cpp> il2Cpp;
    switch (magic) {
        default:
            throw std::runtime_error("ERROR: il2cpp file not supported.");
        case 0x6D736100: {
            WebAssembly web(il2CppBytes);
            il2Cpp = web.createMemory();
            break;
        }
        case 0x304F534E: {
            auto nso = std::make_unique<NSO>(il2CppBytes);
            auto uncompressed = nso->unCompress();
            if (uncompressed) {
                il2Cpp = std::move(uncompressed);
            } else {
                il2Cpp = std::move(nso);
            }
            break;
        }
        case 0x905A4D:
            il2Cpp = std::make_unique<PE>(il2CppBytes);
            break;
        case 0x464C457F:
            if (il2CppBytes.size() > 4 && il2CppBytes[4] == 2) {
                il2Cpp = std::make_unique<Elf64>(il2CppBytes);
            } else {
                il2Cpp = std::make_unique<Elf>(il2CppBytes);
            }
            break;
        case 0xCAFEBABE:
        case 0xBEBAFECA: {
            auto machofat = std::make_unique<MachoFat>(il2CppBytes);
            std::cout << "Select Platform: ";
            for (size_t i = 0; i < machofat->fats.size(); i++) {
                const auto& fat = machofat->fats[i];
                if (fat.magic == 0xFEEDFACF) {
                    std::cout << (i + 1) << ".64bit ";
                } else {
                    std::cout << (i + 1) << ".32bit ";
                }
            }
            std::cout << "\n";
            std::string line;
            std::getline(std::cin, line);
            int index = 0;
            try {
                index = std::stoi(line) - 1;
            } catch (...) {
                index = 0;
            }
            if (index < 0) {
                index = 0;
            }
            if (!machofat->fats.empty()) {
                index %= static_cast<int>(machofat->fats.size());
            }
            auto magicInner = machofat->fats[static_cast<size_t>(index)].magic;
            il2CppBytes = machofat->getMacho(index);
            if (magicInner == 0xFEEDFACF) {
                il2Cpp = std::make_unique<Macho64>(il2CppBytes);
            } else {
                il2Cpp = std::make_unique<Macho>(il2CppBytes);
            }
            break;
        }
        case 0xFEEDFACF:
            il2Cpp = std::make_unique<Macho64>(il2CppBytes);
            break;
        case 0xFEEDFACE:
            il2Cpp = std::make_unique<Macho>(il2CppBytes);
            break;
    }

    auto version = config.ForceIl2CppVersion ? config.ForceVersion : metadata->version;
    il2Cpp->metadata = metadata.get();
    il2Cpp->setProperties(version, metadata->metadataUsagesCount);
    std::cout << "Il2Cpp Version: " << il2Cpp->version << "\n";
    if (config.ForceDump || il2Cpp->checkDump()) {
        if (auto* elf = dynamic_cast<ElfBase*>(il2Cpp.get())) {
            std::cout << "Detected this may be a dump file.\n";
            if (!options.nonInteractive) std::cout << "Input il2cpp dump address or input 0 to force continue:\n";
            auto dumpAddr = options.nonInteractive ? options.dumpBase : readHexLine();
            if (dumpAddr != 0) {
                il2Cpp->imageBase = dumpAddr;
                il2Cpp->isDumped = true;
                if (!config.NoRedirectedPointer) {
                    elf->reload();
                }
            } else if (config.ForceDump && options.nonInteractive) {
                il2Cpp->isDumped = true;
            }
        } else {
            il2Cpp->isDumped = true;
        }
    }

    std::cout << "Searching...\n";
    try {
        int methodCount = 0;
        for (const auto& methodDef : metadata->methodDefs) {
            if (methodDef.methodIndex >= 0) {
                methodCount++;
            }
        }
        auto doPlusSearch = [&]() {
            return il2Cpp->plusSearch(methodCount, static_cast<int>(metadata->typeDefs.size()),
                                      static_cast<int>(metadata->imageDefs.size()));
        };
        bool flag = false;
        if (options.nonInteractive && options.codeRegistration && options.metadataRegistration) {
            il2Cpp->init(options.codeRegistration, options.metadataRegistration);
            flag = true;
        } else {
            flag = doPlusSearch();
        }
#ifdef _WIN32
        if (!flag && dynamic_cast<PE*>(il2Cpp.get()) != nullptr) {
            std::cout << "Use custom PE loader\n";
            il2Cpp = PELoader::load(il2cppPath, il2CppBytes);
            il2Cpp->metadata = metadata.get();
            il2Cpp->setProperties(version, metadata->metadataUsagesCount);
            flag = doPlusSearch();
        }
#endif
        if (!flag) {
            flag = il2Cpp->search();
        }
        if (!flag) {
            flag = il2Cpp->symbolSearch();
        }
        if (!flag) {
            if (options.nonInteractive) {
                if (options.codeRegistration == 0 || options.metadataRegistration == 0) {
                    throw std::runtime_error("Cannot find registrations; manual addresses are required");
                }
                il2Cpp->init(options.codeRegistration, options.metadataRegistration);
                flag = true;
            } else {
            std::cout << "ERROR: Can't use auto mode to process file, try manual mode.\n";
            std::cout << "Input CodeRegistration: ";
            auto codeRegistration = readHexLine();
            std::cout << "Input MetadataRegistration: ";
            auto metadataRegistration = readHexLine();
            il2Cpp->init(codeRegistration, metadataRegistration);
            }
        }
        if (il2Cpp->version >= 27 && il2Cpp->isDumped) {
            const auto& typeDef = metadata->typeDefs[0];
            const auto& il2CppType = il2Cpp->types[static_cast<size_t>(typeDef.byvalTypeIndex)];
            metadata->imageBase = il2CppType.typeHandle() - metadata->header.typeDefinitionsOffset;
        }
    } catch (const std::exception& e) {
        std::cout << e.what() << "\n";
        std::cout << "ERROR: An error occurred while processing.\n";
        return false;
    }

    std::cout << "Dumping...\n";
    Il2CppExecutor executor(*metadata, *il2Cpp);
    Il2CppDecompiler decompiler(executor);
    decompiler.decompile(config, outputDir);
    std::cout << "Done!\n";
    if (config.GenerateStruct) {
        std::cout << "Generate struct...\n";
        StructGenerator scriptGenerator(executor);
        scriptGenerator.writeScript(outputDir);
        std::cout << "Done!\n";
    }
    if (config.GenerateDummyDll) {
        std::cout << "Generate dummy dll...\n";
        try {
            DummyAssemblyGenerator dummy(executor, config.DummyDllAddToken);
            fs::path dllDir = fs::path(outputDir) / "DummyDll";
            std::error_code ec;
            fs::remove_all(dllDir, ec);
            fs::create_directories(dllDir, ec);
            if (ec) {
                std::cout << "ERROR: cannot create " << dllDir.string() << ": " << ec.message() << "\n";
                return false;
            } else {
                for (const auto& out : dummy.assemblies) {
                    std::ofstream f(dllDir / out.fileName, std::ios::binary);
                    f.write(reinterpret_cast<const char*>(out.bytes.data()),
                            static_cast<std::streamsize>(out.bytes.size()));
                }
                std::cout << "Dummy dlls: " << dummy.assemblies.size() << " -> " << dllDir.string() << "/\n";
            }
        } catch (const std::exception& e) {
            std::cout << "ERROR: " << e.what() << "\n";
            return false;
        }
        std::cout << "Done!\n";
    }
    return true;
}

}

class CallbackLogBuffer : public std::streambuf {
public:
    CallbackLogBuffer(Il2CppNativeLogCallback callback, void* context)
        : callback_(callback), context_(context) {}
    ~CallbackLogBuffer() override { sync(); }

protected:
    int overflow(int ch) override {
        if (ch != traits_type::eof()) append(static_cast<char>(ch));
        return ch;
    }
    std::streamsize xsputn(const char* data, std::streamsize count) override {
        for (std::streamsize i = 0; i < count; ++i) append(data[i]);
        return count;
    }
    int sync() override {
        if (!line_.empty() && callback_) callback_(line_.c_str(), context_);
        line_.clear();
        return 0;
    }

private:
    void append(char ch) {
        if (ch == '\n') sync();
        else if (ch != '\r') line_ += ch;
    }
    Il2CppNativeLogCallback callback_;
    void* context_;
    std::string line_;
};

extern "C" int il2cpp_dump_native(const char* binary_path,
                                     const char* metadata_path,
                                     const char* output_directory,
                                     const char* config_path,
                                     const char* code_registration,
                                     const char* metadata_registration,
                                     Il2CppNativeLogCallback callback,
                                     void* context) {
    CallbackLogBuffer logBuffer(callback, context);
    auto* oldOut = std::cout.rdbuf(&logBuffer);
    auto* oldErr = std::cerr.rdbuf(&logBuffer);
    int result = 1;
    try {
        if (!binary_path || !metadata_path || !output_directory || !config_path)
            throw std::runtime_error("Missing dump path");
        std::string output(output_directory);
        if (output.empty()) throw std::runtime_error("Missing output directory");
        fs::create_directories(output);
        if (output.back() != '/') output += '/';
        RunOptions options;
        options.nonInteractive = true;
        if (code_registration && *code_registration)
            options.codeRegistration = std::stoull(code_registration, nullptr, 16);
        if (metadata_registration && *metadata_registration)
            options.metadataRegistration = std::stoull(metadata_registration, nullptr, 16);
        auto config = loadConfig(config_path);
        result = process(config, binary_path, metadata_path, output, options) ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
    }
    std::cout.flush();
    std::cerr.flush();
    std::cout.rdbuf(oldOut);
    std::cerr.rdbuf(oldErr);
    return result;
}
}
