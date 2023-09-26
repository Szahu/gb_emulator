#include "memory.hpp"
#include <cstring>

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

    delete[] m_memory;

}

void Memory::ReadByte(uint16_t addr, void* dest) const {
    LOG_MEM_VERBOSE(printf("MEM: ReadByte | addr: %02x | byte: %01x\n", addr, m_memory[addr]));
    *(static_cast<uint8_t*>(dest)) = m_memory[addr];
}

uint8_t Memory::ReadByte(uint16_t addr) const {
    LOG_MEM_VERBOSE(printf("MEM: ReadByte | addr: %02x | byte: %01x\n", addr, m_memory[addr]));
    return m_memory[addr];
}

void Memory::WriteByte(uint16_t addr, uint8_t byte) {
    LOG_MEM_VERBOSE(printf("MEM: WriteByte | addr: %02x | byte: %01x\n", addr, byte));
    m_memory[addr] = byte;
}

void Memory::CleanMemory() {
    std::memset(m_memory, 0x00, m_memory_size);
}

