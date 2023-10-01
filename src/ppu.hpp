#pragma once

#include "memory.hpp"
#include <queue>

struct PixelData_t {
    uint8_t color;
    uint8_t pallet;
    bool background_priority;
};

struct OAM_t {
    uint8_t y_position;
    uint8_t x_position;
    uint8_t tile_data;
    uint8_t flags;
};

enum mode_t {
    H_Blank = 0,
    V_Blank = 1,
    OAM_Scan = 2,
    Drawing = 3,
};

class Ppu {
public:
    Ppu(Memory* mem_ref, uint8_t* frame_buffer_ref);
    ~Ppu();

    void PpuStep(unsigned int vailable_cycles);

    inline uint8_t* GetFrameBuffer() {return m_framebuffer;}

private:
    void oamScan(OAM_t* oam_buffer, uint8_t oam_buffer_index);
    void renderBackgroundLine(uint8_t scanline);

    void hBlankStep(uint8_t& current_scanline);
    void vBlankStep(uint8_t& current_scanline);
    void oamScanStep(uint8_t& current_scanline);
    void drawingStep(uint8_t& current_scanline);

    uint8_t readByteWrap(uint16_t addr);
    void writeByteWrap(uint16_t addr, uint8_t byte);

private:

    static constexpr uint16_t LCDC_ADDR = 0xFF40; // address of LCD control register
    static constexpr uint16_t STAT_ADDR = 0xFF41; // address of LCD status register
    static constexpr uint16_t SCY_ADDR = 0xFF42; // address of viewport Y position register
    static constexpr uint16_t SCX_ADDR = 0xFF43; // address of viewpoer X position register
    static constexpr uint16_t LY_ADDR = 0xFF44; 
    static constexpr uint16_t LYC_ADDR = 0xFF45; 
    static constexpr uint16_t DMA_ADDR = 0xFF46; 
    static constexpr uint16_t BGP_ADDR = 0xFF47; 
    static constexpr uint16_t OBP0_ADDR = 0xFF48; 
    static constexpr uint16_t OBP1_ADDR = 0xFF49; 
    static constexpr uint16_t WY_ADDR = 0xFF49; // window y position
    static constexpr uint16_t WX_ADDR = 0xFF49; // window x position + 7 
    static constexpr uint16_t OAM_ADDR = 0xFE00;
    static constexpr uint8_t OAM_SIZE = 160;
    static constexpr unsigned int SCREEN_WIDTH = 160;
    static constexpr unsigned int SCREEN_HEIGHT = 144;

    const unsigned int WINDOW_WIDTH = 256;    
    const unsigned int WINDOW_HEIGHT = 256;    

    uint8_t* m_framebuffer;
    Memory* m_memory;

    mode_t m_current_mode = mode_t::OAM_Scan;
    unsigned int m_dot_pool = 0;
    unsigned int m_current_dots_need = 0;    
};