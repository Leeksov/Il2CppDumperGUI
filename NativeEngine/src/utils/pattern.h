#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace il2cppdumper {


std::vector<size_t> searchPattern(const std::vector<uint8_t>& source, const std::vector<uint8_t>& pattern);

std::vector<size_t> searchPattern(const std::vector<uint8_t>& source, const std::string& stringPattern);

}
