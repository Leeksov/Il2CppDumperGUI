#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*Il2CppNativeLogCallback)(const char* message, void* context);

int il2cpp_dump_native(const char* binary_path,
                       const char* metadata_path,
                       const char* output_directory,
                       const char* config_path,
                       const char* code_registration,
                       const char* metadata_registration,
                       Il2CppNativeLogCallback callback,
                       void* context);

int il2cpp_extract_zip(const char* archive_path, const char* destination_directory);

#ifdef __cplusplus
}
#endif
