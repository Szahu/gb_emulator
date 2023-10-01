#include "memory.hpp"
#include <cstring>
#include <cstdio>

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

    delete[] m_memory;

}

uint8_t Memory::ReadByte(uint16_t addr) const {
    LOG_MEM_VERBOSE(printf("MEM: ReadByte | addr: %02x | byte: %01x\n", addr, m_memory[addr]));
    
    if (addr >= 0x4000 && addr <= 0x7FFF) {
        return m_rom_banks[m_current_bank][addr % 0x4000];
    }

    return m_memory[addr];
}

void Memory::ReadByte(uint16_t addr, void* dest) const {
    LOG_MEM_VERBOSE(printf("MEM: ReadByte | addr: %02x | byte: %01x\n", addr, m_memory[addr]));
    *(static_cast<uint8_t*>(dest)) = ReadByte(addr);
}

void Memory::WriteByte(uint16_t addr, uint8_t byte) {
    LOG_MEM_VERBOSE(printf("MEM: WriteByte | addr: %02x | byte: %01x\n", addr, byte));

    if (addr >= 0 && addr <= 0x1FFF) {
        // Write only ram enable
        m_ram_enable = (byte & 0x0F) == 0x0A;
        return;
    } 
    
    if (addr >= 0x2000 && addr <= 0x3FFF) {
        // Write only Rom bank number
        uint8_t val = byte & 0b00011111;
        if (val != 0) val--;
        m_current_bank = val;
        return;
    }

    if (addr >= 0x4000 && addr <= 0x5fff) {
        printf("UNSUPPORTED RAM BANK NUMBER WRITE\n");
        return;
    }

    if (addr >= 0x6000 && addr <= 0x7FFF) {
        // ROM/RAM selector
        printf("ADVANCED BANKING MODE NOT SUPPORTED\n");
        return;
    }

    m_memory[addr] = byte;
}

void Memory::LoadRom(uint8_t* buffer, size_t size) {

    uint8_t rom_type = buffer[0x0147];
    size_t rom_size = (32768) * (1 << buffer[0x0148]);
    uint8_t number_of_banks = (2 << buffer[0x0148]);

    if (rom_type > 0x01) {
        printf("UNSUPPORTED MBC TYPE: %02x\n", rom_type);
    }

    std::memcpy(m_memory, buffer, 0x4000); 

    for (uint8_t i = 1;i < number_of_banks; ++i) {
        uint8_t* bank_mem = new uint8_t[0x4000];
        std::memcpy(bank_mem, buffer + i * 0x4000, 0x4000); 
        m_rom_banks.push_back(bank_mem);
    }

}

void Memory::CleanMemory() {
    std::memset(m_memory, 0x00, m_memory_size);
}

