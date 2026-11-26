#pragma once

#include <cstdint>

namespace il2cppdumper {


uint32_t decodeMov(const uint8_t* asmBytes);

uint64_t decodeAdr(uint64_t pc, uint32_t inst);

uint64_t decodeAdrp(uint64_t pc, uint32_t inst);

uint64_t decodeAdd(uint32_t inst);

bool isAdr(uint32_t inst);

}
