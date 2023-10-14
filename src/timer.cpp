#include "timer.hpp"
#include "common.hpp"

#define max(a,b) a > b ? a : b

Timer::Timer(Memory* mem_ref): m_memory{mem_ref} {}

void Timer::TimerStep(unsigned int m_cycles_count) {
    m_cycle_pool_div += m_cycles_count;
    m_cycle_pool_tima += m_cycles_count;

    uint8_t tac = m_memory->ReadByte(TAC_ADDR);
    uint8_t timer_counter = m_memory->ReadByte(TIMA_ADDR);
    uint8_t timer_modulo = m_memory->ReadByte(TMA_ADDR);

    static uint8_t last_val = 24;
    uint8_t current_val = m_memory->ReadByte(DIV_ADDR);

    if (last_val != current_val) {
        current_val = 0;
        last_val = 0;
        m_memory->WriteByte(DIV_ADDR, current_val);
    }

    if (m_cycle_pool_div >= 64) {
        m_cycle_pool_div -= 64;
        m_memory->WriteByte(DIV_ADDR, current_val + 1);
        last_val = current_val + 1;
    }

    const unsigned int clock_speed_m_cycles[] = {256, 4, 128, 64};
    const uint8_t setting_index = tac & 0x03;
    const uint8_t setting = clock_speed_m_cycles[setting_index];

    while (bitGet(tac, 2) && m_cycle_pool_tima >= setting) {
        bool will_overflow = timer_counter == 0xFF;
        m_cycle_pool_tima -= setting;

        timer_counter++;

        if (will_overflow) {
            timer_counter = timer_modulo;
            uint8_t interrupts = m_memory->ReadByte(0xFF0F);
            bitSet(interrupts, 2, 1);
            m_memory->WriteByte(0xFF0F, interrupts);
        }

    }

    m_memory->WriteByte(TIMA_ADDR, timer_counter);
}

