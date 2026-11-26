#include "il2cpp_native.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/types.h>

// macOS ships libarchive but does not include its public headers in the SDK.
// These are stable C API declarations for the small ZIP reader surface used here.
extern "C" {
struct archive;
struct archive_entry;
archive* archive_read_new(void);
int archive_read_support_format_zip(archive*);
int archive_read_support_filter_all(archive*);
int archive_read_open_filename(archive*, const char*, size_t);
int archive_read_next_header(archive*, archive_entry**);
ssize_t archive_read_data(archive*, void*, size_t);
int archive_read_data_skip(archive*);
int archive_read_free(archive*);
const char* archive_entry_pathname(archive_entry*);
unsigned int archive_entry_filetype(archive_entry*);
}

namespace fs = std::filesystem;

extern "C" int il2cpp_extract_zip(const char* archive_path, const char* destination_directory) {
    if (!archive_path || !destination_directory) return 1;
    archive* reader = archive_read_new();
    if (!reader) return 1;
    int result = 1;
    do {
        if (archive_read_support_format_zip(reader) < 0 || archive_read_support_filter_all(reader) < 0)
            break;
        if (archive_read_open_filename(reader, archive_path, 10240) != 0) break;
        fs::path destination(destination_directory);
        std::error_code error;
        fs::create_directories(destination, error);
        if (error) break;
        bool failed = false;
        archive_entry* entry = nullptr;
        int status;
        while ((status = archive_read_next_header(reader, &entry)) == 0) {
            const char* rawName = archive_entry_pathname(entry);
            if (!rawName) { failed = true; break; }
            std::string name(rawName);
            std::replace(name.begin(), name.end(), '\\', '/');
            fs::path relative(name);
            if (relative.is_absolute() || relative.has_root_name() || relative.empty()) {
                failed = true; break;
            }
            for (const auto& part : relative) {
                if (part == "..") { failed = true; break; }
            }
            if (failed) break;
            auto fileType = archive_entry_filetype(entry);
            auto output = destination / relative;
            auto component = destination;
            for (const auto& part : relative) {
                component /= part;
                if (fs::is_symlink(component, error)) { failed = true; break; }
                if (error && error != std::errc::no_such_file_or_directory) { failed = true; break; }
                error.clear();
            }
            if (failed) break;
            if (fileType == 0040000) {
                fs::create_directories(output, error);
            } else if (fileType == 0100000 || fileType == 0) {
                fs::create_directories(output.parent_path(), error);
                if (!error) {
                    std::ofstream file(output, std::ios::binary | std::ios::trunc);
                    if (!file) error = std::make_error_code(std::errc::io_error);
                    std::array<char, 64 * 1024> buffer;
                    while (!error) {
                        auto count = archive_read_data(reader, buffer.data(), buffer.size());
                        if (count < 0) { error = std::make_error_code(std::errc::io_error); break; }
                        if (count == 0) break;
                        file.write(buffer.data(), count);
                        if (!file) error = std::make_error_code(std::errc::io_error);
                    }
                }
            } else {
                archive_read_data_skip(reader); // Do not create symlinks or special files.
            }
            if (error) { failed = true; break; }
        }
        if (!failed && status == 1) result = 0;
    } while (false);
    archive_read_free(reader);
    return result;
}
