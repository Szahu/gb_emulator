#pragma once 
#include "memory.hpp"

class Timer {
public:
    Timer(Memory* mem_ref);

    void TimerStep(unsigned int m_cycles_count);

private:
    Memory* m_memory;
    unsigned int m_cycle_pool_tima = 0;
    unsigned int m_cycle_pool_div = 0;

    static constexpr uint16_t DIV_ADDR = 0xFF04;
    static constexpr uint16_t TIMA_ADDR = 0xFF05;
    static constexpr uint16_t TMA_ADDR = 0xFF06;
    static constexpr uint16_t TAC_ADDR = 0xFF07;
};