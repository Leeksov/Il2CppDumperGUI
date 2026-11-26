#include "utils/string_utils.h"

namespace il2cppdumper {

std::string toEscapedString(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    const size_t n = s.size();
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        size_t len = 0;
        if (c < 0x80) {
            len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
        }
        if (len > 1 && i + len <= n) {
            bool valid = true;
            for (size_t j = 1; j < len; j++) {
                if ((static_cast<unsigned char>(s[i + j]) & 0xC0) != 0x80) {
                    valid = false;
                    break;
                }
            }
            if (valid) {
                if (c == 0xC2 && s[i + 1] == static_cast<char>(0x85)) {
                    out += "\\u0085";
                } else if (c == 0xE2 && s[i + 1] == static_cast<char>(0x80) &&
                           (s[i + 2] == static_cast<char>(0xA8) || s[i + 2] == static_cast<char>(0xA9))) {
                    out += (s[i + 2] == static_cast<char>(0xA8)) ? "\\u2028" : "\\u2029";
                } else {
                    out.append(s, i, len);
                }
                i += len;
                continue;
            }
        }
        switch (c) {
            case '\'':
                out += "\\'";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\0':
                out += "\\0";
                break;
            case '\a':
                out += "\\a";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            case '\v':
                out += "\\v";
                break;
            case 0x85:
                out += "\\u0085";
                break;
            default:
                out += static_cast<char>(c);
                break;
        }
        i++;
    }
    return out;
}

}
