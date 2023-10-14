#pragma once

#include <stdexcept>
#include "memory.hpp"

inline static void bitSet(uint8_t& number, uint8_t n, bool x) {
    number = (number & ~((uint8_t)1 << n)) | ((uint8_t)x << n);
}

inline static bool bitGet(uint8_t val, uint8_t k) {
    return (val & ( 1 << k )) >> k;
}

#define ASSERT(x) if (!(x)) throw std::runtime_error(#x);