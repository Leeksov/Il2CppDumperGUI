#include "utils/arm_utils.h"

#include <cstddef>

namespace il2cppdumper {

uint32_t decodeMov(const uint8_t* asmBytes) {
    const uint8_t* b = asmBytes;
    uint16_t low = static_cast<uint16_t>(b[2] + ((b[3] & 0x70) << 4) + ((b[1] & 0x04) << 9) +
                                         ((b[0] & 0x0f) << 12));
    uint16_t high = static_cast<uint16_t>(b[6] + ((b[7] & 0x70) << 4) + ((b[5] & 0x04) << 9) +
                                          ((b[4] & 0x0f) << 12));
    return (static_cast<uint32_t>(high) << 16) + low;
}

uint64_t decodeAdr(uint64_t pc, uint32_t inst) {
    uint64_t immhi = (inst >> 5) & 0x7FFFF;
    uint64_t immlo = (inst >> 29) & 0x3;
    uint64_t imm = (immhi << 2) | immlo;
    if (imm & (1ull << 20)) {
        imm |= ~((1ull << 21) - 1);
    }
    return pc + static_cast<int64_t>(imm);
}

uint64_t decodeAdrp(uint64_t pc, uint32_t inst) {
    uint64_t immhi = (inst >> 5) & 0x7FFFF;
    uint64_t immlo = (inst >> 29) & 0x3;
    uint64_t imm = ((immhi << 2) | immlo) << 12;
    if (imm & (1ull << 32)) {
        imm |= ~((1ull << 33) - 1);
    }
    pc &= ~0xFFFull;
    return pc + static_cast<int64_t>(imm);
}

uint64_t decodeAdd(uint32_t inst) {
    uint64_t imm12 = (inst >> 10) & 0xFFF;
    if (inst & (1u << 22)) {
        imm12 <<= 12;
    }
    return imm12;
}

bool isAdr(uint32_t inst) {
    return (inst >> 31) == 0 && ((inst >> 24) & 0x1F) == 0b10000;
}

}
