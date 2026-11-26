#include "utils/pattern.h"

#include <cstring>
#include <sstream>

namespace il2cppdumper {

std::vector<size_t> searchPattern(const std::vector<uint8_t>& source, const std::vector<uint8_t>& pattern) {
    std::vector<size_t> results;
    size_t valueLength = source.size();
    size_t patternLength = pattern.size();
    if (valueLength == 0 || patternLength == 0 || patternLength > valueLength) {
        return results;
    }
    int badCharacters[256];
    for (int i = 0; i < 256; i++) {
        badCharacters[i] = static_cast<int>(patternLength);
    }
    size_t lastPatternByte = patternLength - 1;
    for (size_t i = 0; i < lastPatternByte; i++) {
        badCharacters[pattern[i]] = static_cast<int>(lastPatternByte - i);
    }
    size_t index = 0;
    while (index <= valueLength - patternLength) {
        size_t i = lastPatternByte;
        while (source[index + i] == pattern[i]) {
            if (i == 0) {
                results.push_back(index);
                break;
            }
            i--;
        }
        index += static_cast<size_t>(badCharacters[source[index + lastPatternByte]]);
    }
    return results;
}

namespace {

struct MaskedByte {
    bool masked;
    uint8_t value;
};

}

std::vector<size_t> searchPattern(const std::vector<uint8_t>& source, const std::string& stringPattern) {
    std::vector<size_t> results;
    if (source.empty() || stringPattern.empty()) {
        return results;
    }
    std::vector<std::string> tokens;
    std::istringstream ss(stringPattern);
    std::string token;
    while (ss >> token) {
        tokens.push_back(token);
    }
    size_t patternLength = tokens.size();
    if (patternLength == 0 || patternLength > source.size()) {
        return results;
    }
    std::vector<MaskedByte> pattern;
    pattern.reserve(patternLength);
    for (const auto& t : tokens) {
        if (t == "?") {
            pattern.push_back({true, 0});
        } else {
            pattern.push_back({false, static_cast<uint8_t>(std::stoul(t, nullptr, 16))});
        }
    }
    int badCharacters[256];
    for (int i = 0; i < 256; i++) {
        badCharacters[i] = static_cast<int>(patternLength);
    }
    size_t lastPatternByte = patternLength - 1;
    for (size_t i = 0; i < lastPatternByte; i++) {
        if (!pattern[i].masked) {
            badCharacters[pattern[i].value] = static_cast<int>(lastPatternByte - i);
        }
    }
    size_t index = 0;
    while (index <= source.size() - patternLength) {
        size_t i = lastPatternByte;
        while (true) {
            bool equal = pattern[i].masked || source[index + i] == pattern[i].value;
            if (!equal) {
                break;
            }
            if (i == 0) {
                results.push_back(index);
                break;
            }
            i--;
        }
        index += static_cast<size_t>(badCharacters[source[index + lastPatternByte]]);
    }
    return results;
}

}
