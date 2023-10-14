#include "memory.hpp"
#include <cstring>
#include <cstdio>
#include <stdexcept>

#ifdef LOG_MEM_LEVEL_VERBOSE
#define LOG_MEM_VERBOSE(x) x
#else
#define LOG_MEM_VERBOSE(x)
#endif

Memory::Memory(size_t memory_size) {

    m_memory = new uint8_t[memory_size];
    m_memory_size = memory_size;
    CleanMemory();

}

Memory::~Memory() {

    for (uint8_t* bank : m_rom_banks) {
        delete[] bank;
    }

    for (uint8_t* bank : m_ram_banks) {
        delete[] bank;
    }

    delete[] m_memory;

}

uint8_t Memory::ReadByte(uint16_t addr) const {
    LOG_MEM_VERBOSE(printf("MEM: ReadByte | addr: %02x | byte: %01x\n", addr, m_memory[addr]));
    
    if (addr >= 0x4000 && addr <= 0x7FFF) {
        // rom bank
        return m_rom_banks[m_current_rom_bank][addr % 0x4000];
    }

    if (addr >= 0xA000 && addr <= 0xBFFF) {
        if (!m_ram_enable) return 0xFF;
        // ram bank
        return m_ram_banks[m_current_ram_bank][addr % 0x2000];
    }

    return m_memory[addr];
}

void Memory::ReadByte(uint16_t addr, void* dest) const {
    *(static_cast<uint8_t*>(dest)) = ReadByte(addr);
}

void Memory::WriteByte(uint16_t addr, uint8_t byte) {
    LOG_MEM_VERBOSE(printf("MEM: WriteByte | addr: %02x | byte: %01x\n", addr, byte));

    unsigned int consumed_m_cycles = 0;

    if (addr >= 0 && addr <= 0x1FFF) {
        // Write only ram enable
        m_ram_enable = (byte & 0x0F) == 0x0A && m_ram_banks.size() != 0;
        return;
    } 
    
    if (addr >= 0x2000 && addr <= 0x3FFF) {
        // Write only Rom bank number
        uint8_t val = byte & 0b00011111;
        if (val != 0) val--;
        if (val >= m_rom_banks.size()) {
            printf("Trying to set current rom bank to a wrong value %u\n", val);
            throw std::runtime_error("Trying to set current rom bank to a wrong value");
        }
        m_current_rom_bank = val;
        return;
    }

    if (addr == 0xFF46) {
        // oam dma transfer
        for (uint16_t i = 0; i < 0xA0; ++i) {
            WriteByte(0xFE00 + i, ReadByte((byte << 8) + i));
        }
        consumed_m_cycles = 160;
        return;
    }

    if (addr >= 0x4000 && addr <= 0x5fff) {
        // ram bank selector
        if (m_advanced_banking_mode) {
            m_current_ram_bank = byte & 0b11;
        } else {
            m_current_rom_bank = m_current_rom_bank | ((byte & 0b11) << 5);
        }

        return;
    }

    if (addr >= 0xA000 && addr <= 0xBFFF) {
        if (!m_ram_enable) return;

        //external ram
        m_ram_banks[m_current_ram_bank][addr % 0x2000] = byte;
        return;
    }

    if (addr >= 0x6000 && addr <= 0x7FFF) {
        // ROM/RAM selector
        m_advanced_banking_mode = (byte & 0b1) && 
            m_rom_banks.size() > 32 && m_ram_banks.size() > 1;
        return;
    }

    m_memory[addr] = byte;
}

void Memory::LoadRom(uint8_t* buffer, size_t size) {

    uint8_t rom_type = buffer[0x0147];
    size_t rom_size = (32768) * (1 << buffer[0x0148]);
    uint8_t number_of_rom_banks = (2 << buffer[0x0148]);
   
    uint8_t number_of_ram_banks = 1 << (buffer[0x0149] - 1);
    if (buffer[0x0149] < 2) number_of_ram_banks = 0;    

    m_multicart_rom = number_of_rom_banks > 32 && number_of_ram_banks > 1; 

    if (rom_type > 0x03) {
        printf("UNSUPPORTED MBC TYPE: %02x\n", rom_type);
    }

    std::memcpy(m_memory, buffer, 0x4000); 

    for (uint8_t i = 1;i < number_of_rom_banks; ++i) {
        uint8_t* bank_mem = new uint8_t[0x4000];
        std::memcpy(bank_mem, buffer + i * 0x4000, 0x4000); 
        m_rom_banks.push_back(bank_mem);
    }

    for (uint8_t i = 0;i < number_of_ram_banks; ++i) {
        m_rom_banks.push_back(new uint8_t[1 << 13]);
    }

    printf("ROM type: %u ROM banks: %u RAM banks: %u\n", 
            rom_type, number_of_rom_banks, number_of_ram_banks);

}

void Memory::CleanMemory() {
    std::memset(m_memory, 0x00, m_memory_size);
}

