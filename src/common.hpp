#pragma once

#include "memory.hpp"

inline static void bitSet(uint8_t& number, uint8_t n, bool x) {
    number = (number & ~((uint8_t)1 << n)) | ((uint8_t)x << n);
}

inline static bool bitGet(uint8_t val, uint8_t k) {
    return (val & ( 1 << k )) >> k;
}

inline static uint16_t Read2Bytes(Memory* mem, uint16_t addr) {
    uint16_t res = (mem->ReadByte(addr) << 8) | (mem->ReadByte(addr + 8));
    return res;
}

inline static uint32_t Read4Bytes(Memory* mem, uint16_t addr) {
    uint32_t res = (mem->ReadByte(addr) << 24) | (mem->ReadByte(addr + 8) << 16) | 
                   (mem->ReadByte(addr + 16) << 8) | (mem->ReadByte(addr + 24));
    return res;
}

inline static uint32_t CombineBytes(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
    uint32_t res = (b1 << 24) | (b2 << 16) | (b3 << 8) | (b4);
    return res;
}