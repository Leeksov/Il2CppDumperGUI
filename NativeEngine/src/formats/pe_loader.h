#pragma once

#include "formats/pe.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace il2cppdumper {

class PELoader {
public:
    static std::unique_ptr<PE> load(const std::string& fileName, const std::vector<uint8_t>& buff);
};

}
