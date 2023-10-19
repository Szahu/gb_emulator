#include "cpu.hpp"
#include <cstring>
#include <iostream>

#define LOG_CPU_LEVEL_VERBOSE

#ifdef LOG_CPU_LEVEL_VERBOSE
#define LOG_CPU_VERBOSE(x) if (m_log_verbose) x
#else 
#define LOG_CPU_VERBOSE(x)
#endif

#define LSB(x) (x & 0x00FF)
#define MSB(x) ((x & 0xFF00) >> 8)

Cpu::Cpu(Memory* memory_ref, size_t clock_speed): m_memory{memory_ref}, m_clock_speed{clock_speed} {
    std::memset(m_regs, 0x00, 8);
    PC = ROM_LOCATION;
}

bool Cpu::checkFlagsConditions(uint8_t condition) {

    bool res = false;

    switch (condition) {
        case 0b00:
            res = bitGet(m_regs[REG_F], FLAG_Z_BIT) == 0;
            break;
        case 0b01:
            res = bitGet(m_regs[REG_F], FLAG_Z_BIT) == 1;
            break;
        case 0b10:
            res = bitGet(m_regs[REG_F], FLAG_C_BIT) == 0;
            break;
        case 0b11:
            res = bitGet(m_regs[REG_F], FLAG_C_BIT) == 1;
            break;
    }

    return res;
}

uint8_t Cpu::add(uint8_t a, uint8_t b, bool add_carry, bool* half_carry, bool* full_carry) {

    uint8_t c = add_carry && FLAG_C;

    *half_carry = ((a & 0x0F) + (b & 0x0F) + (c & 0x0F)) & 0x10;
    *full_carry = a + c > 0xFF - b;

    return a + b + c;
}

uint16_t Cpu::add_16(uint16_t a, uint16_t b, bool* half_carry, bool* full_carry) {

    *half_carry = ((a & 0x0FFF) + (b & 0x0FFF)) & 0x1000;
    *full_carry = a > 0xFFFF - b;

    return a + b;
}

uint8_t Cpu::sub(uint8_t a, uint8_t b, bool use_carry, bool* half_carry, bool* full_carry) {

    uint8_t c = use_carry && FLAG_C;

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
    setFlags(half_carry, full_carry, res == 0, 1);
}

void Cpu::setFlags(bool h, bool c, bool z, bool n) {
    bitSet(m_regs[REG_F], FLAG_H_BIT, h);
    bitSet(m_regs[REG_F], FLAG_C_BIT, c);
    bitSet(m_regs[REG_F], FLAG_Z_BIT, z);
    bitSet(m_regs[REG_F], FLAG_N_BIT, n);
}

void Cpu::handleInterrupts(unsigned int& cycle_count) {
    uint8_t interrupts_requests = m_memory->ReadByte(IE_FLAG_ADDR);
    uint8_t interrupts_enables = m_memory->ReadByte(IE_ENABLE_ADDR);
    // VBlank, LCD_STAT, Timer, Serial, Joypad
    uint16_t handlers[] = {0x40, 0x48, 0x50, 0x58, 0x60};

    for (uint8_t i = 0;i < 5; ++i) {
        if (bitGet(interrupts_requests, i) && bitGet(interrupts_enables, i)) {
            m_halted = false;

            if (m_ime) {
                LOG_CPU_VERBOSE(printf("Handling interrupt %u\n", i);)
                m_ime = false;
                bitSet(interrupts_requests, i, 0);
                m_memory->WriteByte(IE_FLAG_ADDR, interrupts_requests);
                SP--;
                m_memory->WriteByte(SP--, MSB(PC));
                m_memory->WriteByte(SP, LSB(PC));
                PC = handlers[i];
                cycle_count += 5;
            }
            
            break;
        }
    }

}

void Cpu::CpuStep(bool& stop_signal, unsigned int& cycle_count) {
    
    handleInterrupts(cycle_count);

    if (m_enable_ime_next_cycle) { 
        m_ime = true;
        m_enable_ime_next_cycle = false;
    }

    if (!m_halted) {
        uint8_t instruction = m_memory->ReadByte(PC);

        LOG_CPU_VERBOSE(
            printf("PC: %04X opcode: %02X AF: %04X BC: %04X DE: %04X HL: %04X SP: %04X IME: %d IE: %02X IF: %02X\n", 
            PC, instruction, AF_GET, BC_GET, DE_GET, HL_GET, SP, m_ime, m_memory->ReadByteDirect(IE_ENABLE_ADDR), m_memory->ReadByteDirect(IE_FLAG_ADDR));
        )

        if (instruction == 0xFF) {
            printf("Read insutrction 0xFF\n");
            throw std::runtime_error("Read insutrction 0xFF\n");
        }
        
        if (m_past_instrs.size() == PAST_INTRS_BUFFER_SIZE) m_past_instrs.pop_front();
        m_past_instrs.push_back({PC, instruction});
        PC += 1;

        if (instruction == 0xCB) {
            instruction = m_memory->ReadByte(PC++);
            decodeAndExecuteCB(instruction, cycle_count);
        } else {
            decodeAndExecuteNonCB(instruction, stop_signal, cycle_count);
        }
    } else {
        cycle_count += 1;
    }
}

void Cpu::decodeAndExecuteNonCB(uint8_t opcode, bool& stop_signal, unsigned int& m_cycles_count) {

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
            uint8_t* dst = &m_regs[(opcode & 0b00111000) >> 3];
            m_memory->ReadByte(HL_GET, dst);
            m_cycles_count = 2;
            break;
        }
        // ld [HL], reg | 2 M-cycles
        case 0x77: case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75:
        {
            uint8_t src = m_regs[opcode & 0b00000111];
            m_memory->WriteByte(HL_GET, src);
            m_cycles_count = 2;
            break;
        }
        // ld [HL], n8 | 3 M-cycles 
        case 0x36:
        {
            uint8_t byte_to_write = m_memory->ReadByte(PC++);
            m_memory->WriteByte(HL_GET, byte_to_write);
            m_cycles_count = 3;
            break;
        }
        // ld A, [BC] | 2 M-cycles
        case 0x0A:
        {
            m_memory->ReadByte(BC_GET, &m_regs[REG_A]);
            m_cycles_count = 2;
            break;
        }
        // ld [BC], A | 2 M-cycles
        case 0x02:
        {
            m_memory->WriteByte(BC_GET, m_regs[REG_A]);
            m_cycles_count = 2;
            break;
        }
        // ld A, [DE] | 2 M-cycles
        case 0x1A:
        {
            m_memory->ReadByte(DE_GET, &m_regs[REG_A]);
            m_cycles_count = 2;
            break;
        }
        // ld [DE], A | 2 M-cycles
        case 0x12:
        {
            m_memory->WriteByte(DE_GET, m_regs[REG_A]);
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
            m_memory->ReadByte(HL_GET, &m_regs[REG_A]);
            HL_SET(HL_GET - 1);
            m_cycles_count = 2;
            break;
        }
        // ld [HL-], A | 2 M-cycles
        case 0x32:
        {
            m_memory->WriteByte(HL_GET, m_regs[REG_A]);
            HL_SET(HL_GET - 1);
            m_cycles_count = 2;
            break;
        }
        // ld A, [HL+] | 2 M-cycles
        case 0x2A: 
        {
            m_memory->ReadByte(HL_GET, &m_regs[REG_A]);
            HL_SET(HL_GET + 1);
            m_cycles_count = 2;
            break;
        }
        // ld [HL+], A | 2 M-cycles
        case 0x22:
        {
            m_memory->WriteByte(HL_GET, m_regs[REG_A]);
            HL_SET(HL_GET + 1);
            m_cycles_count = 2;
            break;
        }
        // ld rr, a16 | 3 M-cycles
        case 0x01: case 0x11: case 0x21: 
        {
            uint8_t lsb = m_memory->ReadByte(PC++);
            uint8_t msb = m_memory->ReadByte(PC++);
            m_regs[(opcode >> 4) * 2] = msb;
            m_regs[(opcode >> 4) * 2 + 1] = lsb;
            m_cycles_count = 3;
            break;
        }
        // ld SP, a16
        case 0x31:
        {
            uint8_t lsb = m_memory->ReadByte(PC++);
            uint8_t msb = m_memory->ReadByte(PC++);
            SP = (msb << 8) | lsb;
            m_cycles_count = 3;
            break;
        }
        // ld [a16], SP | 5 M-cycles
        case 0x08:
        {
            uint8_t lsb = m_memory->ReadByte(PC++);
            uint8_t msb = m_memory->ReadByte(PC++);
            uint16_t addr = lsb | (msb << 8);
            m_memory->WriteByte(addr, LSB(SP));
            m_memory->WriteByte(addr + 1, MSB(SP));
            m_cycles_count = 5;
            break;
        }
        // ld SP, HL | 2 M-cycles
        case 0xF9:
        {
            SP = HL_GET;
            m_cycles_count = 2;
            break;
        } 
        // push rr | 4 M-cycles
        case 0xC5: case 0xD5: case 0xE5:
        {
            uint8_t reg_index = (opcode >> 4) & 0b00000011;
            uint16_t val = (m_regs[reg_index * 2] << 8) | m_regs[reg_index * 2 + 1];
            SP--;
            m_memory->WriteByte(SP--, MSB(val));
            m_memory->WriteByte(SP, LSB(val));
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
            uint8_t msb = m_memory->ReadByte(SP++);
            uint8_t lsb = m_memory->ReadByte(SP++);
            uint16_t val = lsb | (msb << 8);
            *reinterpret_cast<uint16_t*>(m_regs + reg_index * 2) = val;
            m_cycles_count = 3;
            break;
        }
        // pop AF | 3 M-cycles
        case 0xF1:
        {
            m_regs[REG_F] = m_memory->ReadByte(SP++) & 0xF0;
            m_regs[REG_A] = m_memory->ReadByte(SP++);
            m_cycles_count = 3;
            break;
        }
        // add A, reg | 1 M-cycle
        case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x87: // add a,reg
        case 0x88: case 0x89: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8f: // adc a,reg
        {
            bool carry = (opcode & 8) && bitGet(m_regs[REG_F], FLAG_C_BIT);
            add_A_reg(m_regs[opcode & 0x07], carry);
            m_cycles_count = 1;
            break;
        }
        // add [HL] | 2 M-cycles
        case 0x86:
        {
            add_A_reg(m_memory->ReadByte(HL_GET), false);
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
            add_A_reg(m_memory->ReadByte(HL_GET), true);
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
            bool carry = (opcode & 0x08) && FLAG_C;
            sub_A_reg(m_regs[opcode & 0x07], carry);
            m_cycles_count = 1;
            break;
        }
        // sub [HL] | 2 M-cycles
        case 0x96:
        {
            sub_A_reg(m_memory->ReadByte(HL_GET), false);
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
            sub_A_reg(m_memory->ReadByte(HL_GET), true);
            m_cycles_count = 2;
            break;
        }
        // sbc n8 | 2 M-cycles
        case 0xDE:
        {
            sub_A_reg(m_memory->ReadByte(PC++), true);
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
            sub_A_reg(m_memory->ReadByte(HL_GET), false, true);
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
            bitSet(m_regs[REG_F], FLAG_H_BIT, half_carry);
            bitSet(m_regs[REG_F], FLAG_Z_BIT, m_regs[(opcode & 0b00111000) >> 3] == 0);
            bitSet(m_regs[REG_F], FLAG_N_BIT, 0);
            m_cycles_count = 1;
            break;
        }
        // INC [HL] | 3 M-cycles
        case 0x34:
        {
            bool half_carry;
            bool full_carry;
            uint8_t byte = m_memory->ReadByte(HL_GET);
            uint8_t res = add(byte, 1, false, &half_carry, &full_carry);
            m_memory->WriteByte(HL_GET, res);
            bitSet(m_regs[REG_F], FLAG_H_BIT, half_carry);
            bitSet(m_regs[REG_F], FLAG_Z_BIT, res == 0);
            bitSet(m_regs[REG_F], FLAG_N_BIT, 0);
            m_cycles_count = 3;
            break;
        }
        // INC rr | 2 M-cycles
        case 0x03: case 0x13: case 0x23: 
        {
            uint8_t lsb = m_regs[(opcode >> 4) * 2 + 1];
            uint8_t msb = m_regs[(opcode >> 4) * 2 ];
            uint16_t new_val = ((msb << 8) | lsb) + 1;
            lsb = new_val & 0x00FF;
            msb = new_val >> 8;  
            m_regs[(opcode >> 4) * 2 + 1] = lsb;
            m_regs[(opcode >> 4) * 2 ] = msb;
            m_cycles_count = 2;
            break;
        }
        // INC SP | 2 M-cycles
        case 0x33: 
        {
            SP++;
            m_cycles_count = 2;
            break;
        }
        // DEC r | 1 M-cycles
        case 0x05: case 0x15: case 0x25: case 0x0D: case 0x1D: case 0x2D: case 0x3D:
        {
            bool half_carry;
            bool full_carry;
            m_regs[(opcode & 0b00111000) >> 3] = sub(m_regs[(opcode & 0b00111000) >> 3], 1, false, &half_carry, &full_carry);
            bitSet(m_regs[REG_F], FLAG_H_BIT, half_carry);
            bitSet(m_regs[REG_F], FLAG_Z_BIT, m_regs[(opcode & 0b00111000) >> 3] == 0);
            bitSet(m_regs[REG_F], FLAG_N_BIT, 1);
            m_cycles_count = 1;
            break;
        }
        // DEC [HL] | 3 M-cycles
        case 0x35:
        {
            bool half_carry;
            bool full_carry;
            uint8_t res = sub(m_memory->ReadByte(HL_GET), 1, false, &half_carry, &full_carry);
            m_memory->WriteByte(HL_GET, res);
            bitSet(m_regs[REG_F], FLAG_H_BIT, half_carry);
            bitSet(m_regs[REG_F], FLAG_Z_BIT, res == 0);
            bitSet(m_regs[REG_F], FLAG_N_BIT, 1);
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
            m_regs[REG_A] &= m_memory->ReadByte(HL_GET);
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
            m_regs[REG_A] |= m_memory->ReadByte(HL_GET);
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
            m_regs[REG_A] ^= m_memory->ReadByte(HL_GET);
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
        // RLCA | 1 M-cycle
        case 0x07:
        {
            bool sign_bit = bitGet(m_regs[REG_A], 7);
            m_regs[REG_A] = m_regs[REG_A] << 1;
            bitSet(m_regs[REG_A], 0, sign_bit);
            setFlags(0, sign_bit, 0, 0);
            m_cycles_count = 1;
            break;
        }
        // RRCA | 1 M-cycles
        case 0x0F:
        {
            bool sign_bit = bitGet(m_regs[REG_A], 0);
            m_regs[REG_A] = m_regs[REG_A] >> 1;
            bitSet(m_regs[REG_A], 7, sign_bit);
            setFlags(0, sign_bit, 0, 0);
            m_cycles_count = 1;
            break;
        }
        // RLA | 1 M-cycles
        case 0x17:
        {
            bool sign_bit = bitGet(m_regs[REG_A], 7);
            m_regs[REG_A] = m_regs[REG_A] << 1;
            bitSet(m_regs[REG_A], 0, FLAG_C);
            setFlags(0, sign_bit, 0, 0);
            m_cycles_count = 1;
            break;
        }
        // RRA | 1 M-cycle
        case 0x1F:
        {
            bool sign_bit = bitGet(m_regs[REG_A], 0);
            m_regs[REG_A] = m_regs[REG_A] >> 1;
            bitSet(m_regs[REG_A], 7, FLAG_C);
            setFlags(0, sign_bit, 0, 0);
            m_cycles_count = 1;
            break;
        }
        // ADD HL, rr | 2 M-cycles
        case 0x09: case 0x19: case 0x29:
        {
            bool half_carry;
            bool full_carry;
            uint16_t operand = m_regs[(opcode >> 4) * 2 + 1] | (m_regs[(opcode >> 4) * 2] << 8);
            HL_SET(add_16(HL_GET, operand, &half_carry, &full_carry));
            setFlags(half_carry, full_carry, FLAG_Z, 0);
            m_cycles_count = 2;
            break;
        }
        // ADD HL, SP | 2 M-cycles
        case 0x39:
        {
            bool half_carry;
            bool full_carry;
            HL_SET(add_16(HL_GET, SP, &half_carry, &full_carry));
            setFlags(half_carry, full_carry, FLAG_Z, 0);
            m_cycles_count = 2;
            break;
        }
        // ADD SP, n8 | 4 M-cycles
        case 0xE8:
        {
            int8_t operand = m_memory->ReadByte(PC++);
            bool half_carry;
            bool full_carry;
            add(SP & 0x00FF, operand, false, &half_carry, &full_carry);
            SP += operand;
            setFlags(half_carry, full_carry, 0, 0);
            m_cycles_count = 4;
            break;
        }
        // LD HL, SP + dd | 3 M-cycles
        case 0xF8:
        {
            int8_t operand = m_memory->ReadByte(PC++);
            bool half_carry;
            bool full_carry;
            add((SP & 0x00FF), operand, false, &half_carry, &full_carry);
            HL_SET(SP + operand);
            setFlags(half_carry, full_carry, 0, 0);
            m_cycles_count = 3;
            break;
        }
        // DEC rr | 2 M-cycles
        case 0x0B: case 0x1B: case 0x2B:
        {
            uint16_t operand = m_regs[(opcode >> 4) * 2 + 1] | (m_regs[(opcode >> 4) * 2] << 8);
            operand--;
            m_regs[(opcode >> 4) * 2 + 1] = LSB(operand);
            m_regs[(opcode >> 4) * 2] = MSB(operand);
            m_cycles_count = 2;
            break;
        }
        // DEC SP
        case 0x3B:
        {
            SP--;
            m_cycles_count = 2;
            break;
        }
        // CCF | 1 M-cycle
        case 0x3F:
        {
            bitSet(m_regs[REG_F], FLAG_N_BIT, 0);
            bitSet(m_regs[REG_F], FLAG_H_BIT, 0);
            bitSet(m_regs[REG_F], FLAG_C_BIT, !FLAG_C);
            m_cycles_count = 1;
            break;
        }
        // SCF | 1 M-cycle
        case 0x37:
        {
            bitSet(m_regs[REG_F], FLAG_N_BIT, 0);
            bitSet(m_regs[REG_F], FLAG_H_BIT, 0);
            bitSet(m_regs[REG_F], FLAG_C_BIT, 1);
            m_cycles_count = 1;
            break;
        }
        // DAA | 1 M-cycle
        case 0x27:
        {
            uint8_t& a = m_regs[REG_A];

            if (!FLAG_N) {  
                if (FLAG_C || a > 0x99) { a += 0x60; bitSet(m_regs[REG_F], FLAG_C_BIT, 1); }
                if (FLAG_H || (a & 0x0f) > 0x09) { a += 0x6; }
            } else {  
                if (FLAG_C) { a -= 0x60; }
                if (FLAG_H) { a -= 0x6; }
            }

            bitSet(m_regs[REG_F], FLAG_Z_BIT, (a == 0));
            bitSet(m_regs[REG_F], FLAG_H_BIT, 0);

            m_cycles_count = 1;
            break;
        }
        // CPL | 1 M-cycle
        case 0x2F:
        {
            m_regs[REG_A] = ~m_regs[REG_A];
            bitSet(m_regs[REG_F], FLAG_N_BIT, 1);
            bitSet(m_regs[REG_F], FLAG_H_BIT, 1);
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
            PC = HL_GET;
            m_cycles_count = 1;
            break;
        }
        // JP cc, a16 | 3/4 M-cycles
        case 0xC2: case 0xD2: case 0xCA: case 0xDA: // jump absolute 
        {
            uint8_t lsb = m_memory->ReadByte(PC++);
            uint8_t msb = m_memory->ReadByte(PC++);
            uint8_t cond = (opcode & 0b00011000) >> 3;
            bool res = checkFlagsConditions(cond);

            if (res) {
                PC = (msb << 8) | lsb;
                m_cycles_count = 4;
            } else {
                m_cycles_count = 3;
            }

            break;
        }
        // JR NZ, r8 | 2/3 M-cycles
        case 0x20: case 0x30: case 0x28: case 0x38: 
        {
            int8_t offset = m_memory->ReadByte(PC++);
            uint8_t cond = (opcode & 0b00011000) >> 3;
            bool res = checkFlagsConditions(cond);
            
            if (res) {
                PC += offset;
                m_cycles_count = 3;
            } else {
                m_cycles_count = 2;
            }

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
            m_memory->WriteByte(SP--, MSB(PC));
            m_memory->WriteByte(SP, LSB(PC));
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
                m_memory->WriteByte(SP--, MSB(PC));
                m_memory->WriteByte(SP, LSB(PC));
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
            m_enable_ime_next_cycle = true;
            m_cycles_count = 4;
            break;
        }
        // RST n | 4 M-cycles
        case 0xC7: case 0xD7: case 0xE7: case 0xF7: case 0xCF: case 0xDF: case 0xEF: case 0xFF:
        {
            const uint8_t addresses[] = {0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38};
            uint8_t rst_addr = addresses[(opcode & 0b00111000) >> 3];
            SP--;
            m_memory->WriteByte(SP--, MSB(PC));
            m_memory->WriteByte(SP, LSB(PC));
            PC = static_cast<uint16_t>(rst_addr);
            m_cycles_count = 4;
            break;
        }
        // STOP
        case 0x10:
        {
            m_cycles_count = 0;
            stop_signal = true;
            m_halted = true;
            break;
        }
        // HALT 
        case 0x76:
        {
            m_cycles_count = 1;
            m_halted = true;
            break;
        }
        // DI | 1 M-cycle
        case 0xF3:
        {
            m_ime = false;
            m_cycles_count = 1;
            break;
        }
        // EI | 1 M-cycle
        case 0xFB:
        {
            m_enable_ime_next_cycle = true;
            m_cycles_count = 1;
            break;
        }
        default:
        {
            printf("Unrecognised instruction: %02x\n", opcode);
            break;
        }
    }
}

void Cpu::decodeAndExecuteCB(uint8_t opcode, unsigned int& m_cycles_count) {

    switch (opcode)
    {
        // RLC r | 2 M-cycles
        case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x07:
        {
            uint8_t& reg = m_regs[opcode & 0x0F];
            bool side_bit = bitGet(reg, 7);
            reg = reg << 1;
            bitSet(reg, 0, side_bit);
            setFlags(0, side_bit, reg == 0, 0);
            m_cycles_count = 2;
            break;
        }
        // RLC [HL] | 4 M-cycles
        case 0x06:
        {
            uint8_t val = m_memory->ReadByte(HL_GET);
            bool side_bit = bitGet(val, 7);
            val = val << 1;
            bitSet(val, 0, side_bit);
            setFlags(0, side_bit, val == 0, 0);
            m_memory->WriteByte(HL_GET, val);
            m_cycles_count = 4;
            break;
        }
        // case RRC | 2 M-cycles
        case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: case 0x0F:
        {
            uint8_t& reg = m_regs[opcode & 0x07];
            bool side_bit = bitGet(reg, 0);
            reg = reg >> 1;
            bitSet(reg, 7, side_bit);
            setFlags(0, side_bit, reg == 0, 0);
            m_cycles_count = 2;
            break;
        }
        // RRC [HL] | 4 M-cycles
        case 0x0E:
        {
            uint8_t val = m_memory->ReadByte(HL_GET);
            bool side_bit = bitGet(val, 0);
            val = val >> 1;
            bitSet(val, 7, side_bit);
            setFlags(0, side_bit, val == 0, 0);
            m_memory->WriteByte(HL_GET, val);
            m_cycles_count = 4;
            break;
        }
        // RL r | 2 M-cycles
        case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x17:
        {   
            uint8_t& reg = m_regs[opcode & 0x0F];
            bool side_bit = bitGet(reg, 7);
            reg = reg << 1;
            bitSet(reg, 0, FLAG_C);
            setFlags(0, side_bit, reg == 0, 0);
            m_cycles_count = 2;
            break;
        }
        // RL [HL] | 4 M-cycles
        case 0x16:
        {
            uint8_t val = m_memory->ReadByte(HL_GET);
            bool side_bit = bitGet(val, 7);
            val = val << 1;
            bitSet(val, 0, FLAG_C);
            setFlags(0, side_bit, val == 0, 0);
            m_memory->WriteByte(HL_GET, val);
            m_cycles_count = 4;
            break;
        }
        // RR r | 2 M-cycles
        case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1F:
        {
            uint8_t& reg = m_regs[opcode & 0x07];
            bool side_bit = bitGet(reg, 0);
            reg = reg >> 1;
            bitSet(reg, 7, FLAG_C);
            setFlags(0, side_bit, reg == 0, 0);
            m_cycles_count = 2;
            break;
        }
        // RR [HL] | 4 M-cycles
        case 0x1E:
        {
            uint8_t val = m_memory->ReadByte(HL_GET);
            bool side_bit = bitGet(val, 0);
            val = val >> 1;
            bitSet(val, 7, FLAG_C);
            setFlags(0, side_bit, val == 0, 0);
            m_memory->WriteByte(HL_GET, val);
            m_cycles_count = 4;
            break;
        }
        // SLA r | 2 M-cycles
        case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x27:
        {
            uint8_t& reg = m_regs[opcode & 0x07];
            bool side_bit = bitGet(reg, 7);
            reg = reg << 1;
            setFlags(0, side_bit, reg == 0, 0);
            m_cycles_count = 2;
            break;
        }
        // SLA [HL] | 4 M-cycles
        case 0x26:
        {
            uint8_t val = m_memory->ReadByte(HL_GET);
            bool side_bit = bitGet(val, 7);
            val = val << 1;
            setFlags(0, side_bit, val == 0, 0);
            m_memory->WriteByte(HL_GET, val);
            m_cycles_count = 4;
            break;
        }
        // SRA r | 2 M-cycles
        case 0x28: case 0x29: case 0x2A: case 0x2B: case 0x2C: case 0x2D: case 0x2F:
        {
            uint8_t& reg = m_regs[opcode & 0x07];
            bool side_bit = bitGet(reg, 0);
            bool sign_bit = bitGet(reg, 7);
            reg = reg >> 1;
            bitSet(reg, 7, sign_bit);
            setFlags(0, side_bit, reg == 0, 0);
            m_cycles_count = 2;
            break;
        } 
        // SRA [HL] | 4 M-cycles
        case 0x2E:
        {
            uint8_t val = m_memory->ReadByte(HL_GET);
            bool side_bit = bitGet(val, 0);
            bool sign_bit = bitGet(val, 7);
            val = val >> 1;
            bitSet(val, 7, sign_bit);
            setFlags(0, side_bit, val == 0, 0);
            m_memory->WriteByte(HL_GET, val);
            m_cycles_count = 4;
            break;
        }
        // SWAP r | 2 M-cycles
        case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x37:
        {
            uint8_t& reg = m_regs[opcode & 0x07];
            reg = ((reg & 0xF0) >> 4) | ((reg & 0x0F) << 4);
            setFlags(0, 0, reg == 0, 0);
            m_cycles_count = 2;
            break;
        }
        // SWAP [HL] | 4 M-cycles
        case 0x36:
        {
            uint8_t val = m_memory->ReadByte(HL_GET);
            val = ((val & 0xF0) >> 4) | ((val & 0x0F) << 4);
            setFlags(0, 0, val == 0, 0);
            m_memory->WriteByte(HL_GET, val);
            m_cycles_count = 4;
            break;
        }
        // SRL B | 2 M-cycles
        case 0x38: case 0x39: case 0x3A: case 0x3B: case 0x3C: case 0x3D: case 0x3F:
        {
            uint8_t& reg = m_regs[opcode & 0x07];
            bool side_bit = bitGet(reg, 0);
            reg = reg >> 1; 
            setFlags(0, side_bit, reg == 0, 0);
            m_cycles_count = 2;
            break;
        }
        // SRL [HL]
        case 0x3E:
        {
            uint8_t val = m_memory->ReadByte(HL_GET);
            bool side_bit = bitGet(val, 0);
            val = val >> 1; 
            setFlags(0, side_bit, val == 0, 0);
            m_memory->WriteByte(HL_GET, val);
            m_cycles_count = 4;
            break;
        }
        // BIT r | 2 M-cycles
        case 0x40: case 0x41: case 0x42: case 0x43:
        case 0x44: case 0x45: case 0x47:
        case 0x48: case 0x49: case 0x4A: case 0x4B:
        case 0x4C: case 0x4D: case 0x4F:
        case 0x50: case 0x51: case 0x52: case 0x53:
        case 0x54: case 0x55: case 0x57:
        case 0x58: case 0x59: case 0x5A: case 0x5B:
        case 0x5C: case 0x5D: case 0x5F:
        case 0x60: case 0x61: case 0x62: case 0x63:
        case 0x64: case 0x65: case 0x67:
        case 0x68: case 0x69: case 0x6A: case 0x6B:
        case 0x6C: case 0x6D: case 0x6F:
        case 0x70: case 0x71: case 0x72: case 0x73:
        case 0x74: case 0x75: case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B:
        case 0x7C: case 0x7D: case 0x7F:
        {
            uint8_t& reg = m_regs[opcode & 0x07];
            uint8_t bit = (opcode & 0b00111000) >> 3;
            bool check = bitGet(reg, bit);
            setFlags(1, FLAG_C, !check, 0);
            m_cycles_count = 2;
            break;
        }
        // BIT [HL] | 3 m_cycles
        case 0x46: case 0x56: case 0x66: case 0x76:
        case 0x4E: case 0x5E: case 0x6E: case 0x7E:
        {
            uint8_t val = m_memory->ReadByte(HL_GET);
            uint8_t bit = (opcode & 0b00111000) >> 3;
            bool check = bitGet(val, bit);
            setFlags(1, FLAG_C, !check, 0);
            m_cycles_count = 3;
            break;
        }
        // RES r | 2 M-cycles
        case 0x80: case 0x81: case 0x82: case 0x83:
        case 0x84: case 0x85: case 0x87:
        case 0x88: case 0x89: case 0x8A: case 0x8B:
        case 0x8C: case 0x8D: case 0x8F:
        case 0x90: case 0x91: case 0x92: case 0x93:
        case 0x94: case 0x95: case 0x97:
        case 0x98: case 0x99: case 0x9A: case 0x9B:
        case 0x9C: case 0x9D: case 0x9F:
        case 0xA0: case 0xA1: case 0xA2: case 0xA3:
        case 0xA4: case 0xA5: case 0xA7:
        case 0xA8: case 0xA9: case 0xAA: case 0xAB:
        case 0xAC: case 0xAD: case 0xAF:
        case 0xB0: case 0xB1: case 0xB2: case 0xB3:
        case 0xB4: case 0xB5: case 0xB7:
        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
        case 0xBC: case 0xBD: case 0xBF:
        {
            uint8_t& reg = m_regs[opcode & 0x07];
            uint8_t bit = (opcode & 0b00111000) >> 3;
            bitSet(reg, bit, 0);
            m_cycles_count = 2;
            break;
        }
        // RES [HL] | 4 M-cycles
        case 0x86: case 0x8E: case 0x96: case 0x9E:
        case 0xA6: case 0xAE: case 0xB6: case 0xBE:
        {
            uint8_t val = m_memory->ReadByte(HL_GET);
            uint8_t bit = (opcode & 0b00111000) >> 3;
            bitSet(val, bit, 0);
            m_memory->WriteByte(HL_GET, val);
            m_cycles_count = 4;
            break;
        }
        // SET r | 2 M-cycles
        case 0xC0: case 0xC1: case 0xC2: case 0xC3:
        case 0xC4: case 0xC5: case 0xC7:
        case 0xC8: case 0xC9: case 0xCA: case 0xCB:
        case 0xCC: case 0xCD: case 0xCF:
        case 0xD0: case 0xD1: case 0xD2: case 0xD3:
        case 0xD4: case 0xD5: case 0xD7:
        case 0xD8: case 0xD9: case 0xDA: case 0xDB:
        case 0xDC: case 0xDD: case 0xDF:
        case 0xE0: case 0xE1: case 0xE2: case 0xE3:
        case 0xE4: case 0xE5: case 0xE7:
        case 0xE8: case 0xE9: case 0xEA: case 0xEB:
        case 0xEC: case 0xED: case 0xEF:
        case 0xF0: case 0xF1: case 0xF2: case 0xF3:
        case 0xF4: case 0xF5: case 0xF7:
        case 0xF8: case 0xF9: case 0xFA: case 0xFB:
        case 0xFC: case 0xFD: case 0xFF:
        {
            uint8_t& reg = m_regs[opcode & 0x07];
            uint8_t bit = (opcode & 0b00111000) >> 3;
            bitSet(reg, bit, 1);
            m_cycles_count = 2;
            break;
        }
        // SET [HL] | 4 M-cycles
        case 0xC6: case 0xCE: case 0xD6: case 0xDE:
        case 0xE6: case 0xEE: case 0xF6: case 0xFE:
        {
            uint8_t val = m_memory->ReadByte(HL_GET);
            uint8_t bit = (opcode & 0b00111000) >> 3;
            bitSet(val, bit, 1);
            m_memory->WriteByte(HL_GET, val);
            m_cycles_count = 4;
            break;
        }
        default:
        {
            printf("Unrecognised instruction: %02x\n", opcode);
            break;
        }
    }

}