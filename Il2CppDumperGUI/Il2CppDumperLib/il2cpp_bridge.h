#ifndef IL2CPP_BRIDGE_H
#define IL2CPP_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*Il2CppLogCallback)(const char* message, void* context);

// Main dump function
// Returns: 0 on success, non-zero on error
int il2cpp_dump(
    const char* il2cppPath,
    const char* metadataPath,
    const char* outputDir,
    int dumpMethod,
    int dumpField,
    int dumpProperty,
    int dumpAttribute,
    int dumpFieldOffset,
    int dumpMethodOffset,
    int dumpTypeDefIndex,
    int forceIl2CppVersion,
    double forceVersion,
    int forceDump,
    Il2CppLogCallback logCallback,
    void* logContext
);

// Extended dump with manual addresses
int il2cpp_dump_ex(
    const char* il2cppPath,
    const char* metadataPath,
    const char* outputDir,
    int dumpMethod,
    int dumpField,
    int dumpProperty,
    int dumpAttribute,
    int dumpFieldOffset,
    int dumpMethodOffset,
    int dumpTypeDefIndex,
    int forceIl2CppVersion,
    double forceVersion,
    int forceDump,
    const char* manualCodeRegistration,   // hex string or empty
    const char* manualMetadataRegistration, // hex string or empty
    const char* postProcessToolPath,       // path to Il2CppPostProcess binary or NULL
    Il2CppLogCallback logCallback,
    void* logContext
);

const char* il2cpp_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
