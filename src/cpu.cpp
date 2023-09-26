#include "cpu.hpp"
#include <iostream>

#define FLAG_H getBitFrom(m_regs[REG_F], FLAG_H_BIT)
#define FLAG_C getBitFrom(m_regs[REG_F], FLAG_C_BIT)
#define FLAG_Z getBitFrom(m_regs[REG_F], FLAG_Z_BIT)
#define FLAG_N getBitFrom(m_regs[REG_F], FLAG_N_BIT)

#define LOG_CPU_LEVEL_VERBOSE

#ifdef LOG_CPU_LEVEL_VERBOSE
#define LOG_CPU_VERBOSE(x) x
#else 
#define LOG_CPU_VERBOSE(x)
#endif

Cpu::Cpu(Memory* memory_ref, size_t clock_speed): m_memory{memory_ref}, m_clock_speed{clock_speed} {
    memset(m_regs, 0x00, 8);
    PC = ROM_LOCATION;
    m_cycle_duration_micros = static_cast<unsigned int>((1.0f / static_cast<float>(clock_speed)) * 1000000.0f);
}

bool Cpu::checkFlagsConditions(uint8_t condition) {

    bool res = false;

    switch (condition) {
        case 0b00:
            res = getBitFrom(m_regs[REG_F], FLAG_Z_BIT) == 0;
            break;
        case 0b01:
            res = getBitFrom(m_regs[REG_F], FLAG_Z_BIT) == 1;
            break;
        case 0b10:
            res = getBitFrom(m_regs[REG_F], FLAG_C_BIT) == 0;
            break;
        case 0b11:
            res = getBitFrom(m_regs[REG_F], FLAG_C_BIT) == 1;
            break;
    }

    return res;
}

uint8_t Cpu::add(uint8_t a, uint8_t b, bool add_carry, bool* half_carry, bool* full_carry) {

    uint8_t c = add_carry;

    *half_carry = ((a & 0x0F) + (b & 0x0F) + (c & 0x0F)) & 0x10;
    *full_carry = a + c > 0xFF - b;

    return a + b + c;
}

uint8_t Cpu::sub(uint8_t a, uint8_t b, bool use_carry, bool* half_carry, bool* full_carry) {

    uint8_t c = use_carry;

    *half_carry = ((a & 0xf) - (b & 0xf) - (c & 0xf)) & 0x10;
    *full_carry = ((uint16_t)a - (uint16_t)b - (uint16_t)c) & 0x100;

    return a - b - c;
}

void Cpu::add_A_reg(uint8_t operand, bool with_carry) {
    bool half_carry;
    bool full_carry;
    m_regs[REG_A] = add(m_regs[REG_A], operand, with_carry, &half_carry, &full_carry);
    setFlags(half_carry, full_carry, m_regs[REG_A] == 0, 0);
}

void Cpu::sub_A_reg(uint8_t operand, bool with_carry, bool ignore_res) {
    bool half_carry;
    bool full_carry;
    uint8_t res = sub(m_regs[REG_A], operand, with_carry, &half_carry, &full_carry);
    if (!ignore_res) m_regs[REG_A] = res;
    setFlags(half_carry, full_carry, m_regs[REG_A] == 0, 1);
}

void Cpu::setFlags(bool h, bool c, bool z, bool n) {
    bitSetTo(m_regs[REG_F], FLAG_H_BIT, h);
    bitSetTo(m_regs[REG_F], FLAG_C_BIT, c);
    bitSetTo(m_regs[REG_F], FLAG_Z_BIT, z);
    bitSetTo(m_regs[REG_F], FLAG_N_BIT, n);
}

void Cpu::CpuStep(bool& stop_signal) {

    // fetch next opcode
    uint8_t instruction = m_memory->ReadByte(PC);
    LOG_CPU_VERBOSE(printf("opcode: %02x PC: %024x\n", instruction, PC);)

    PC += 1;

    unsigned int cycle_count = 0;

    auto start = std::chrono::high_resolution_clock::now();    

    decodeAndExecute(instruction, stop_signal, cycle_count);

    auto stop = std::chrono::high_resolution_clock::now();
    unsigned int duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count();

    unsigned int expected_furation = cycle_count * m_cycle_duration_micros;

    if (duration < expected_furation) {
        SLEEP_MICROS(m_cycle_duration_micros - duration);
    } else {
        printf("Cycle too long! Took %u isntead of %u\n", duration, expected_furation);
    }
}

void Cpu::decodeAndExecute(uint8_t opcode, bool& stop_signal, unsigned int& m_cycles_count) {

    switch (opcode) {

        // NOOP| 1 M-cycle
        case 0x00:
        {
            m_cycles_count = 1;
            break;
        }
        // ld reg, reg | 1 M-cycles
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x47: // ld b,reg
        case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4f: // ld c,reg
        case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x57: // ld d,reg
        case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5f: // ld e,reg
        case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x67: // ld h,reg
        case 0x68: case 0x69: case 0x6a: case 0x6b: case 0x6c: case 0x6d: case 0x6f: // ld l,reg
        case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7f: // ld a,reg
        {
            m_regs[(opcode >> 3) & 0b00000111] = m_regs[opcode & 7];
            m_cycles_count = 1;
            break;
        }
        // ld reg, [HL] | 2 M-cycles
        case 0x7E: case 0x46: case 0x4E: case 0x56: case 0x5E: case 0x66: case 0x6E:
        {
            uint8_t dst = m_regs[(opcode >> 3) & 0b00000111];
            m_memory->ReadByte(HL_VAL, &m_regs[dst]);
            m_cycles_count = 2;
            break;
        }
        // ld [HL], reg | 2 M-cycles
        case 0x77: case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75:
        {
            uint8_t src = m_regs[opcode & 0b00000111];
            m_memory->WriteByte(HL_VAL, m_regs[src]);
            m_cycles_count = 2;
            break;
        }
        // ld [HL], n8 | 2 M-cycles 
        case 0x36:
        {
            uint8_t byte_to_write = m_memory->ReadByte(PC++);
            m_memory->WriteByte(HL_VAL, byte_to_write);
            m_cycles_count = 2;
            break;
        }
        // ld A, [BC] | 2 M-cycles
        case 0x0A:
        {
            m_memory->ReadByte(BC_VAL, &m_regs[REG_A]);
            m_cycles_count = 2;
            break;
        }
        // ld [BC], A | 2 M-cycles
        case 0x02:
        {
            m_memory->WriteByte(BC_VAL, m_regs[REG_A]);
            m_cycles_count = 2;
            break;
        }
        // ld A, [DE] | 2 M-cycles
        case 0x1A:
        {
            m_memory->ReadByte(DE_VAL, &m_regs[REG_A]);
            m_cycles_count = 2;
            break;
        }
        // ld [DE], A | 2 M-cycles
        case 0x12:
        {
            m_memory->WriteByte(DE_VAL, m_regs[REG_A]);
            m_cycles_count = 2;
            break;
        }
        // ld reg, n8 | 2 M-cycles
        case 0x06: case 0x0E: case 0x16: case 0x1E: case 0x26: case 0x2E: case 0x3E:
        {
            uint8_t byte_to_load = m_memory->ReadByte(PC++);
            m_regs[(opcode >> 3) & 0b00000111] = byte_to_load;
            m_cycles_count = 2;
            break;
        }
        // ld A, [a16] | 4 M-cycles
        case 0xFA:
        {
            uint8_t lsb = m_memory->ReadByte(PC++);
            uint8_t msb = m_memory->ReadByte(PC++);
            uint16_t addr = lsb | (msb << 8);
            m_memory->ReadByte(addr, &m_regs[REG_A]);
            m_cycles_count = 4;
            break;
        }
        // ld [a16], A | 4 M-cycles
        case 0xEA:
        {
            uint8_t lsb = m_memory->ReadByte(PC++);
            uint8_t msb = m_memory->ReadByte(PC++);
            uint16_t addr = lsb | (msb << 8);
            m_memory->WriteByte(addr, m_regs[REG_A]);
            m_cycles_count = 4;
            break;
        }
        // ldh A, [C] | 2 M-cyces
        case 0xF2:
        {
            m_memory->ReadByte(0xFF00 | m_regs[REG_C], &m_regs[REG_A]);
            m_cycles_count = 2;
            break;
        }
        // ldh [C], A | 2 M-cycles
        case 0xE2: 
        {
            m_memory->WriteByte(0xFF00 | m_regs[REG_C], m_regs[REG_A]);
            m_cycles_count = 2;
            break;
        }
        // ldh A, [n8] | 3 M-cycles
        case 0xF0:
        {
            uint8_t addr_lsb = m_memory->ReadByte(PC++);
            m_memory->ReadByte(0xFF00 | addr_lsb, &m_regs[REG_A]);
            m_cycles_count = 3;
            break;
        }
        // ldh [n8], A | 3 M-cycles
        case 0xE0:
        {
            uint8_t addr_lsb = m_memory->ReadByte(PC++);
            m_memory->WriteByte(0xFF00 | addr_lsb, m_regs[REG_A]);
            m_cycles_count = 3;
            break;
        }
        // ld A, [HL-] | 2 M-cycles
        case 0x3A:
        {
            m_memory->ReadByte(HL_VAL, &m_regs[REG_A]);
            HL_VAL--;
            m_cycles_count = 2;
            break;
        }
        // ld [HL-], A | 2 M-cycles
        case 0x32:
        {
            m_memory->WriteByte(HL_VAL, m_regs[REG_A]);
            HL_VAL--;
            m_cycles_count = 2;
            break;
        }
        // ld A, [HL+] | 2 M-cycles
        case 0x2A: 
        {
            m_memory->ReadByte(HL_VAL, &m_regs[REG_A]);
            HL_VAL++;
            m_cycles_count = 2;
            break;
        }
        // ld [HL+], A | 2 M-cycles
        case 0x22:
        {
            m_memory->WriteByte(HL_VAL, m_regs[REG_A]);
            HL_VAL++;
            m_cycles_count = 2;
            break;
        }
        // ld rr, a16 | 3 M-cycles
        case 0x01: case 0x11: case 0x21: 
        {
            uint8_t lsb = m_memory->ReadByte(PC++);
            uint8_t msb = m_memory->ReadByte(PC++);
            uint16_t val = lsb | (msb << 8);
            m_regs[(opcode >> 4) * 2] = lsb;
            m_regs[(opcode >> 4) * 2 + 1] = msb;
            m_cycles_count = 3;
            break;
        }
        // ld [a16], SP | 5 M-cycles
        case 0x08:
        {
            uint8_t lsb = m_memory->ReadByte(PC++);
            uint8_t msb = m_memory->ReadByte(PC++);
            uint16_t addr = lsb | (msb << 8);
            m_memory->WriteByte(addr, SP);
            m_cycles_count = 5;
            break;
        }
        // ld SP, HL | 2 M-cycles
        case 0xF9:
        {
            SP = HL_VAL;
            m_cycles_count = 2;
            break;
        } 
        // push rr | 4 M-cycles
        case 0xC5: case 0xD5: case 0xE5:
        {
            uint8_t reg_index = (opcode >> 4) & 0b00000011;
            uint16_t val = *(m_regs + reg_index * 2);
            SP--;
            m_memory->WriteByte(SP--, val & 0xFF00);
            m_memory->WriteByte(SP, val & 0x00FF);
            m_cycles_count = 4;
            break;
        }
        // push AF | 4 M-cycles
        case 0xF5: 
        {
            SP--;
            m_memory->WriteByte(SP--, m_regs[REG_A]);
            m_memory->WriteByte(SP, m_regs[REG_F]);
            m_cycles_count = 4;
            break;
        }
        // pop rr | 3 M-cycles
        case 0xC1: case 0xD1: case 0xE1:
        {
            uint8_t reg_index = (opcode >> 4) & 0b00000011;
            uint16_t val = m_memory->ReadByte(SP++) | (m_memory->ReadByte(SP++) << 8);
            *reinterpret_cast<uint16_t*>(m_regs + reg_index * 2) = val;
            m_cycles_count = 3;
            break;
        }
        // pop AF | 3 M-cycles
        case 0xF1:
        {
            m_regs[REG_F] = m_memory->ReadByte(SP++);
            m_regs[REG_A] = m_memory->ReadByte(SP++);
            m_cycles_count = 3;
            break;
        }
        // add A, reg | 1 M-cycle
        case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x87: // add a,reg
        case 0x88: case 0x89: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8f: // adc a,reg
        {
            bool carry = (opcode & 8) && getBitFrom(m_regs[REG_F], FLAG_C_BIT);
            add_A_reg(m_regs[opcode & 0x07], carry);
            m_cycles_count = 1;
            break;
        }
        // add [HL] | 2 M-cycles
        case 0x86:
        {
            add_A_reg(m_memory->ReadByte(HL_VAL), false);
            m_cycles_count = 2;
            break;
        }
        // add n8 | 2 M-cycles
        case 0xC6:
        {
            add_A_reg(m_memory->ReadByte(PC++), false);
            m_cycles_count = 2;
            break;
        }
        // adc [HL] | 2 M-cycles
        case 0x8E:
        {
            add_A_reg(m_memory->ReadByte(HL_VAL), true);
            m_cycles_count = 2;
            break;
        }
        // adc n | 2 M-cycles
        case 0xCE:
        {
            add_A_reg(m_memory->ReadByte(PC++), true);
            m_cycles_count = 2;
            break;
        }
        // sub/sbc A, r | 1 M-cycle
        case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x97:
        case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0x9D: case 0x9F:
        {
            bool carry = (opcode & 0x08) && getBitFrom(m_regs[REG_F], FLAG_C_BIT);
            sub_A_reg(m_regs[opcode & 0x07], carry);
            m_cycles_count = 1;
            break;
        }
        // sub [HL] | 2 M-cycles
        case 0x96:
        {
            sub_A_reg(m_memory->ReadByte(HL_VAL), false);
            m_cycles_count = 2;
            break;
        }
        // sub n8 | 2 M-cycles
        case 0xD6:
        {
            sub_A_reg(m_memory->ReadByte(PC++), false);
            m_cycles_count = 2;
            break;
        }
        // sbc [HL] | 2 M-cycles
        case 0x9E:
        {
            sub_A_reg(m_memory->ReadByte(HL_VAL), true);
            m_cycles_count = 2;
            break;
        }
        // sbc n8 | 2 M-cycles
        case 0xDE:
        {
            sub_A_reg(m_memory->ReadByte(PC++), false);
            m_cycles_count = 2;
            break;
        }
        // CP A, r | 1 M-cycle
        case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBF:
        {
            sub_A_reg(m_regs[opcode & 0x07], false, true);
            m_cycles_count = 1;
            break;
        }
        // CP A, [HL] | 2 M-cycles
        case 0xBE: 
        {
            sub_A_reg(m_memory->ReadByte(HL_VAL), false, true);
            m_cycles_count = 2;
            break;
        }
        // CP A, n8 | 2 M-cycles
        case 0xFE:
        {
            sub_A_reg(m_memory->ReadByte(PC++), false, true);
            m_cycles_count = 2;
            break;
        }
        // INC r | 1 M-cycle
        case 0x04: case 0x14: case 0x24: case 0x0C: case 0x1C: case 0x2C: case 0x3C:
        {
            bool half_carry;
            bool full_carry;
            m_regs[(opcode & 0b00111000) >> 3] = add(m_regs[(opcode & 0b00111000) >> 3], 1, false, &half_carry, &full_carry);
            bitSetTo(m_regs[REG_F], FLAG_H_BIT, half_carry);
            bitSetTo(m_regs[REG_F], FLAG_Z_BIT, m_regs[(opcode & 0b00111000) >> 3] == 0);
            bitSetTo(m_regs[REG_F], FLAG_N_BIT, 0);
            m_cycles_count = 1;
            break;
        }
        // INC [HL] | 3 M-cycles
        case 0x34:
        {
            bool half_carry;
            bool full_carry;
            uint8_t res = add(m_memory->ReadByte(HL_VAL), 1, false, &half_carry, &full_carry);
            m_memory->WriteByte(HL_VAL, res);
            bitSetTo(m_regs[REG_F], FLAG_H_BIT, half_carry);
            bitSetTo(m_regs[REG_F], FLAG_Z_BIT, res == 0);
            bitSetTo(m_regs[REG_F], FLAG_N_BIT, 0);
            m_cycles_count = 3;
            break;
        }
        // DEC r | 1 M-cycles
        case 0x05: case 0x15: case 0x25: case 0x0D: case 0x1D: case 0x2D: case 0x3D:
        {
            bool half_carry;
            bool full_carry;
            m_regs[(opcode & 0b00111000) >> 3] = sub(m_regs[(opcode & 0b00111000) >> 3], 1, false, &half_carry, &full_carry);
            bitSetTo(m_regs[REG_F], FLAG_H_BIT, half_carry);
            bitSetTo(m_regs[REG_F], FLAG_Z_BIT, m_regs[(opcode & 0b00111000) >> 3] == 0);
            bitSetTo(m_regs[REG_F], FLAG_N_BIT, 1);
            m_cycles_count = 1;
            break;
        }
        // DEC [HL] | 3 M-cycles
        case 0x35:
        {
            bool half_carry;
            bool full_carry;
            uint8_t res = sub(m_memory->ReadByte(HL_VAL), 1, false, &half_carry, &full_carry);
            m_memory->WriteByte(HL_VAL, res);
            bitSetTo(m_regs[REG_F], FLAG_H_BIT, half_carry);
            bitSetTo(m_regs[REG_F], FLAG_Z_BIT, res == 0);
            bitSetTo(m_regs[REG_F], FLAG_N_BIT, 1);
            m_cycles_count = 3;
            break;
        }
        // AND A, reg | 1 M-cycle
        case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: case 0xA7:
        {
            m_regs[REG_A] &= m_regs[opcode & 0x07];
            setFlags(1, 0, m_regs[REG_A] == 0, 0);
            m_cycles_count = 1;
            break;
        }
        // AND [HL] | 2 M-cycles
        case 0xA6:
        {
            m_regs[REG_A] &= m_memory->ReadByte(HL_VAL);
            setFlags(1, 0, m_regs[REG_A] == 0, 0);
            m_cycles_count = 2;
            break;
        }
        // AND A, n8 | 2 M-cycles
        case 0xE6:
        {
            m_regs[REG_A] &= m_memory->ReadByte(PC++);
            setFlags(1, 0, m_regs[REG_A] == 0, 0);
            m_cycles_count = 2;
            break;
        }
        // OR A, reg | 1 M-cycle
        case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB7:
        {
            m_regs[REG_A] |= m_regs[opcode & 0x07];
            setFlags(0, 0, m_regs[REG_A] == 0, 0);
            m_cycles_count = 1;
            break;
        }
        // OR [HL] | 2 M-cycles
        case 0xB6:
        {
            m_regs[REG_A] |= m_memory->ReadByte(HL_VAL);
            setFlags(0, 0, m_regs[REG_A] == 0, 0);
            m_cycles_count = 2;
            break;
        }
        //OR A, n8 | 2 M-cycles
        case 0xF6:
        {
            m_regs[REG_A] |= m_memory->ReadByte(PC++);
            setFlags(0, 0, m_regs[REG_A] == 0, 0);
            m_cycles_count = 2;
            break;
        }
        // XOR A, reg | 1 M-cycle
        case 0xA8: case 0xA9: case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAF:
        {
            m_regs[REG_A] ^= m_regs[opcode & 0x07];
            setFlags(0, 0, m_regs[REG_A] == 0, 0);
            m_cycles_count = 1;
            break;
        }
        // XOR [HL] | 2 M-cycles
        case 0xAE:
        {
            m_regs[REG_A] ^= m_memory->ReadByte(HL_VAL);
            setFlags(0, 0, m_regs[REG_A] == 0, 0);
            m_cycles_count = 2;
            break;
        }
        // XOR A, n8 | 2 M-cycles
        case 0xEE:
        {
            m_regs[REG_A] ^= m_memory->ReadByte(PC++);
            setFlags(0, 0, m_regs[REG_A] == 0, 0);
            m_cycles_count = 2;
            break;
        }
        // CCF | 1 M-cycle
        case 0x3F:
        {
            bitSetTo(m_regs[REG_F], FLAG_N_BIT, 0);
            bitSetTo(m_regs[REG_F], FLAG_H_BIT, 0);
            bitSetTo(m_regs[REG_F], FLAG_C_BIT, !FLAG_C);
            m_cycles_count = 1;
            break;
        }
        // SCF | 1 M-cycle
        case 0x37:
        {
            bitSetTo(m_regs[REG_F], FLAG_N_BIT, 0);
            bitSetTo(m_regs[REG_F], FLAG_H_BIT, 0);
            bitSetTo(m_regs[REG_F], FLAG_C_BIT, 1);
            m_cycles_count = 1;
            break;
        }
        // DAA | 1 M-cycle
        case 0x27:
        {
            uint8_t& a = m_regs[REG_A];

            if (!FLAG_N) {  
                if (FLAG_C || a > 0x99) { a += 0x60; bitSetTo(m_regs[REG_F], FLAG_C_BIT, 1); }
                if (FLAG_H || (a & 0x0f) > 0x09) { a += 0x6; }
            } else {  
                if (FLAG_C) { a -= 0x60; }
                if (FLAG_H) { a -= 0x6; }
            }

            bitSetTo(m_regs[REG_F], FLAG_Z_BIT, (a == 0));
            bitSetTo(m_regs[REG_F], FLAG_H_BIT, 0);

            m_cycles_count = 1;
            break;
        }
        // CPL | 1 M-cycle
        case 0x2F:
        {
            m_regs[REG_A] = ~m_regs[REG_A];
            bitSetTo(m_regs[REG_F], FLAG_N_BIT, 1);
            bitSetTo(m_regs[REG_F], FLAG_H_BIT, 1);
            m_cycles_count = 1;
            break;
        }
        // JP a16 | 4 M-cycles
        case 0xC3:
        {
            uint8_t lsb = m_memory->ReadByte(PC++);
            uint8_t msb = m_memory->ReadByte(PC++);
            PC = (msb << 8) | lsb;
            m_cycles_count = 4;
            break;
        }
        // JP HL | 1 M-cycle
        case 0xE9:
        {
            PC = HL_VAL;
            m_cycles_count = 1;
            break;
        }
        // JP/JR c8, a16 | 3/4 M-cycles
        case 0xC2: case 0xD2: case 0xCA: case 0xDA: // jump absolute 
        case 0x20: case 0x30: case 0x28: case 0x38: // jump relative
        {
            bool offset_mode = getBitFrom(opcode, 5);
            uint8_t lsb = m_memory->ReadByte(PC++);
            uint8_t msb = m_memory->ReadByte(PC++);
            uint8_t cond = (opcode & 0b00011000) >> 3;
            bool res = checkFlagsConditions(cond);

            if (res) {
                if (offset_mode) {
                    PC += (msb << 8) | lsb;
                } else {
                    PC = (msb << 8) | lsb;
                }
                
                m_cycles_count = 4;
            } else {
                m_cycles_count = 3;
            }

            if (offset_mode) m_cycles_count--;

            break;

        }
        // JR n8 | 3 M-cycles
        case 0x18:
        {
            int8_t offest = m_memory->ReadByte(PC++);
            PC = PC + offest;
            m_cycles_count = 3;
            break;
        }
        // CALL a16 | 6 M-cycles
        case 0xCD:
        {
            uint8_t lsb = m_memory->ReadByte(PC++);
            uint8_t msb = m_memory->ReadByte(PC++);
            uint16_t addr = (msb << 8) | lsb;
            SP--;
            m_memory->WriteByte(SP--, (PC & 0xF0) >> 8);
            m_memory->WriteByte(SP, (PC & 0x0F));
            PC = addr;
            m_cycles_count = 6;
            break;
        }
        // CALL cc, a16 | 3/6 M-cycles
        case 0xC4: case 0xD4: case 0xCC: case 0xDC:
        {
            uint8_t lsb = m_memory->ReadByte(PC++);
            uint8_t msb = m_memory->ReadByte(PC++);
            uint16_t addr = (msb << 8) | lsb;
            bool res = checkFlagsConditions((opcode & 0b00011000) >> 3);
            if (res) {
                SP--;
                m_memory->WriteByte(SP--, (PC & 0xF0) >> 8);
                m_memory->WriteByte(SP, (PC & 0x0F));
                PC = addr;
                m_cycles_count = 6;
            } else {
                m_cycles_count = 3;
            }
            break;
        }
        // RET | 4 M-cycles
        case 0xC9: 
        {
            uint8_t lsb = m_memory->ReadByte(SP++);
            uint8_t msb = m_memory->ReadByte(SP++);
            uint16_t addr = (msb << 8) | lsb;
            PC = addr;
            m_cycles_count = 4;
            break;
        }
        // RET cc | 2/5 M-cycles:
        case 0xC0: case 0xD0: case 0xC8: case 0xD8:
        {
            if (checkFlagsConditions((opcode & 0b00011000) >> 3)) {
                uint8_t lsb = m_memory->ReadByte(SP++);
                uint8_t msb = m_memory->ReadByte(SP++);
                uint16_t addr = (msb << 8) | lsb;
                PC = addr;
                m_cycles_count = 5;
            } else {
                m_cycles_count = 2;
            }
            break;
        }
        // RETI | 4 M-cycles
        case 0xD9:
        {
            uint8_t lsb = m_memory->ReadByte(SP++);
            uint8_t msb = m_memory->ReadByte(SP++);
            uint16_t addr = (msb << 8) | lsb;
            PC = addr;
            // TODO IME = 1
            m_cycles_count = 4;
            break;
        }
        // RST n | 4 M-cycles
        case 0xC7: case 0xD7: case 0xE7: case 0xF7: case 0xCF: case 0xDF: case 0xEF: case 0xFF:
        {
            uint8_t rst_addr = (opcode & 0b00111000) >> 3;
            SP--;
            m_memory->WriteByte(SP--, (PC & 0xF0) >> 8);
            m_memory->WriteByte(SP, PC & 0x0F);
            PC = static_cast<uint16_t>(rst_addr);
            m_cycles_count = 4;
            break;
        }
        // STOP
        case 0x10:
        {
            m_cycles_count = 0;
            stop_signal = true;
        }
        //TODO HALT STOP DI EI
        default:
        {
            printf("Unrecognised instruction: %02x\n", opcode);
        }
    }

}