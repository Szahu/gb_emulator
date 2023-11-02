#pragma once

#include <stdexcept>

inline static void bitSet(uint8_t& number, uint8_t n, bool x) {
    number = (number & ~((uint8_t)1 << n)) | ((uint8_t)x << n);
}

inline static void bitSet(uint16_t& number, uint8_t n, bool x) {
    number = (number & ~((uint16_t)1 << n)) | ((uint16_t)x << n);
}

inline static bool bitGet(uint8_t val, uint8_t k) {
    return (val & ( 1 << k )) >> k;
}

#define ASSERT(x) if (!(x)) throw std::runtime_error(#x);