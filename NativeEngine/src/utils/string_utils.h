#pragma once

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

namespace il2cppdumper {

std::string toEscapedString(const std::string& s);

template <typename T>
std::string netNumberFormat(T value, int scientificThreshold) {
    if (std::isnan(value)) {
        return "NaN";
    }
    if (std::isinf(value)) {
        return value < 0 ? "-Infinity" : "Infinity";
    }
    bool negative = std::signbit(value);
    T absValue = std::fabs(value);
    if (absValue == 0.0) {
        return negative ? "-0" : "0";
    }
    std::ostringstream number;
    number.imbue(std::locale::classic());
    number << std::setprecision(std::numeric_limits<T>::max_digits10) << absValue;
    std::string s = number.str();
    std::string digits;
    int intDigits = 0;
    int expParsed = 0;
    size_t i = 0;
    bool beforeDot = true;
    for (; i < s.size(); i++) {
        char ch = s[i];
        if (ch == '.') {
            beforeDot = false;
            continue;
        }
        if (ch == 'e' || ch == 'E') {
            int sign = 1;
            if (++i < s.size() && (s[i] == '+' || s[i] == '-')) {
                if (s[i] == '-') {
                    sign = -1;
                }
                i++;
            }
            expParsed = sign * std::atoi(s.c_str() + static_cast<long>(i));
            break;
        }
        if (beforeDot) {
            intDigits++;
        }
        digits += ch;
    }
    size_t first = 0;
    while (first + 1 < digits.size() && digits[first] == '0') {
        first++;
    }
    digits = digits.substr(first);
    int exp10 = (intDigits - 1) - static_cast<int>(first) + expParsed;
    if (exp10 < -4 || exp10 >= scientificThreshold) {
        std::string mant = digits;
        std::string m = mant.substr(0, 1);
        if (mant.size() > 1) {
            m += "." + mant.substr(1);
        }
        std::string e = std::to_string(std::abs(exp10));
        if (e.size() < 2) {
            e = "0" + e;
        }
        return (negative ? "-" : "") + m + "E" + (exp10 < 0 ? "-" : "+") + e;
    }
    int pointPos = exp10 + 1;
    std::string out;
    if (pointPos >= static_cast<int>(digits.size())) {
        out = digits + std::string(static_cast<size_t>(pointPos - static_cast<int>(digits.size())), '0');
    } else if (pointPos > 0) {
        out = digits.substr(0, static_cast<size_t>(pointPos)) + "." +
              digits.substr(static_cast<size_t>(pointPos));
    } else {
        out = "0." + std::string(static_cast<size_t>(-pointPos), '0') + digits;
    }
    size_t dot = out.find('.');
    if (dot != std::string::npos) {
        size_t end = out.size();
        while (end > dot + 1 && out[end - 1] == '0') {
            end--;
        }
        if (end == dot + 1) {
            end = dot;
        }
        out = out.substr(0, end);
    }
    return (negative ? "-" : "") + out;
}

}
