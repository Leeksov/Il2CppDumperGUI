#include "../NativeEngine/include/il2cpp_native.h"
#include <cstdlib>

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    return il2cpp_extract_zip(argv[1], argv[2]);
}
