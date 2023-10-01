#pragma once

#include "memory.hpp"                    
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdio>
#include "common.hpp"

#define SLEEP_MINIS(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#define SLEEP_MICROS(ms) std::this_thread::sleep_for(std::chrono::microseconds(ms));
#define SLEEP_NANOS(ns) std::this_thread::sleep_for(std::chrono::nanoseconds(ns));

#define REG_B 0
#define REG_C 1
#define REG_D 2
#define REG_E 3
#define REG_H 4
#define REG_L 5
#define REG_F 6
#define REG_A 7

#define FLAG_Z_BIT 7
#define FLAG_N_BIT 6
#define FLAG_H_BIT 5
#define FLAG_C_BIT 4

#define FLAG_H bitGet(m_regs[REG_F], FLAG_H_BIT)
#define FLAG_C bitGet(m_regs[REG_F], FLAG_C_BIT)
#define FLAG_Z bitGet(m_regs[REG_F], FLAG_Z_BIT)
#define FLAG_N bitGet(m_regs[REG_F], FLAG_N_BIT)

#define BC_GET static_cast<uint16_t>((m_regs[REG_B] << 8) | m_regs[REG_C])
#define BC_SET(x) uint16_t tmp = x; m_regs[REG_B] = (tmp & 0xFF00) >> 8; m_regs[REG_C] = (tmp & 0x00FF);

#define DE_GET static_cast<uint16_t>((m_regs[REG_D] << 8) | m_regs[REG_E])
#define DE_SET(x) uint16_t tmp = x; m_regs[REG_D] = (tmp & 0xFF00) >> 8; m_regs[REG_E] = (tmp & 0x00FF);

#define HL_GET static_cast<uint16_t>((m_regs[REG_H] << 8) | m_regs[REG_L])
#define HL_SET(x) uint16_t tmp = x; m_regs[REG_H] = (tmp & 0xFF00) >> 8; m_regs[REG_L] = (tmp & 0x00FF);

class Cpu {
public:

    Cpu(Memory* memory_ref, size_t clock_speed);

    void CpuStep(std::atomic<bool>& stop_signal, unsigned int& cycle_count);

    inline Memory* GetMemory() const {return m_memory;}

    inline void SetLogVerbose(bool val) {m_log_verbose = val;}

    inline void PostBoodSetup() {
        m_regs[REG_A] = 0x01;
        m_regs[REG_F] = 0b10110000;
        m_regs[REG_B] = 0x00;
        m_regs[REG_C] = 0x13;
        m_regs[REG_D] = 0x00;
        m_regs[REG_E] = 0xD8;
        m_regs[REG_H] = 0x01;
        m_regs[REG_L] = 0x4D;
        SP = 0xFFFE;

        m_halted = false;
        m_ime = true;
    }

private:

    void decodeAndExecuteNonCB(uint8_t opcode, std::atomic<bool>& stop_signal, unsigned int& m_cycles_count);
    void decodeAndExecuteCB(uint8_t opcode, unsigned int& m_cycles_count);

    uint8_t add(uint8_t a, uint8_t b, bool add_carry, bool* half_carry, bool* full_carry);
    uint16_t add_16(uint16_t a, uint16_t b, bool* half_carry, bool* full_carry);

    uint8_t sub(uint8_t a, uint8_t b, bool use_carry, bool* half_carry, bool* full_carry);

    void add_A_reg(uint8_t operand, bool with_carry);

    void sub_A_reg(uint8_t operand, bool with_carry, bool ignore_res = false);

    void setFlags(bool z, bool n, bool h, bool c);

    bool checkFlagsConditions(uint8_t condition);

    void handleInterrupts(unsigned int& cycle_count);

private:

    Memory* m_memory = nullptr;
    size_t m_clock_speed = 0;

    // registers B C D E H L F A
    uint8_t m_regs[8];
    uint16_t SP = 0; // stack pointer
    uint16_t PC = 0; // program counter
    const size_t ROM_LOCATION = 0x0100;

    static constexpr uint16_t IE_ENABLE_ADDR = 0xFFFF;
    static constexpr uint16_t IE_FLAG_ADDR = 0xFF0F;

    bool m_halted = false;
    bool m_enable_ime_next_cycle = false;
    bool m_ime = false;

    bool m_log_verbose = true;
};