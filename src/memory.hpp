#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>

/*
Memory map:
0x0000 - 0x3FFF: ROM
0x4000 - 0x7FFF: Switchable ROM Bank
0x8000 - 0x9FFF: Video RAM
0xA000 - 0xBFFF: Switchable RAM Bank
0xC000 - 0xDFFF: Internal RAM
0xE000 - 0xFDFF: Unusable 
0xFE00 - 0xFE9F: Spirte attributes
0xFEA0 - 0xFEFF: Unusable
0xFF00 - 0xFF4B: I/O
0xFF80 - 0xFFFE: High RAM
0xFFFF - Interrupt Register
*/

class Memory {
public:

    Memory(size_t memory_size);
    ~Memory();

    inline uint8_t* GetBufferLocation() const { return m_memory; }

    void ReadByte(uint16_t addr, void* dest) const;
    uint8_t ReadByte(uint16_t addr) const;
    void WriteByte(uint16_t addr, uint8_t byte);

    uint8_t ReadByteDirect(uint16_t addr) const;
    void WriteByteDirect(uint16_t addr, uint8_t byte);

    void LoadRom(uint8_t* buffer, size_t size);

    void CleanMemory();

private:
    uint8_t* m_memory = nullptr;
    size_t m_memory_size = 0;
    std::vector<uint8_t*> m_rom_banks;
    std::vector<uint8_t*> m_ram_banks;
    uint8_t m_current_rom_bank = 0;
    uint8_t m_current_ram_bank = 0;
    bool m_ram_enable = false;
    bool m_advanced_banking_mode = false;
    bool m_multicart_rom = false;
};