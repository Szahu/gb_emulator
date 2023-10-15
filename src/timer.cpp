#include "timer.hpp"
#include "common.hpp"

#define max(a,b) a > b ? a : b

Timer::Timer(Memory* mem_ref): m_memory{mem_ref} {}

void Timer::TimerStep(unsigned int m_cycles_count) {
    // DIV
    m_cycle_pool_div += m_cycles_count;

    uint8_t current_val = m_memory->ReadByteDirect(DIV_ADDR);

    if (m_cycle_pool_div >= 64) {
        m_cycle_pool_div -= 64;
        // cannot go through Write cause it resets
        m_memory->WriteByteDirect(DIV_ADDR, current_val + 1);
    }

    uint8_t tac = m_memory->ReadByteDirect(TAC_ADDR);

    // TIMA
    if (!bitGet(tac, 2)) return;

    uint8_t timer_counter = m_memory->ReadByteDirect(TIMA_ADDR);
    uint8_t timer_modulo = m_memory->ReadByteDirect(TMA_ADDR);
    m_cycle_pool_tima += m_cycles_count;

    const unsigned int clock_speed_m_cycles[] = {256, 4, 128, 64};
    const uint8_t setting_index = tac & 0x03;
    const uint8_t setting = clock_speed_m_cycles[setting_index];

    while (m_cycle_pool_tima >= setting) {
        timer_counter++;

        if (timer_counter == 0x00) {
            timer_counter = timer_modulo;
            uint8_t interrupts = m_memory->ReadByteDirect(0xFF0F);
            bitSet(interrupts, 2, true);
            m_memory->WriteByteDirect(0xFF0F, interrupts);
        }

        m_cycle_pool_tima -= setting;
    }

    m_memory->WriteByteDirect(TIMA_ADDR, timer_counter);
}

