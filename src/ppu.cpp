#include "ppu.hpp"
#include "common.hpp"
#include <memory>

Ppu::Ppu(Memory *mem_ref, uint8_t* frame_buffer_ref) : m_memory{mem_ref}, m_framebuffer{frame_buffer_ref} {
}

Ppu::~Ppu() {}

void Ppu::oamScan(OAM_t* oam_buffer, uint8_t oam_buffer_index) {

    for (uint16_t ptr = OAM_ADDR; ptr < OAM_ADDR + OAM_SIZE && oam_buffer_index < 10; ptr += 4) {
        OAM_t oam = {
            m_memory->ReadByte(ptr),
            m_memory->ReadByte(ptr+1),
            m_memory->ReadByte(ptr+2),
            m_memory->ReadByte(ptr+3),
        };

        bool double_height_mode = bitGet(m_memory->ReadByte(LCDC_ADDR), 2); 

        uint8_t ly = m_memory->ReadByte(LY_ADDR);

        bool x_condition = oam.x_position < 8;
        bool ly_y_condition = ly + 16 >= oam.y_position;
        bool ly_h_condition = ly + 16 >= oam.y_position + (8 + 8 * double_height_mode);

        if (x_condition && ly_y_condition && ly_h_condition) {
            oam_buffer[oam_buffer_index++] = oam;
        }
    }
}

void Ppu::renderBackgroundLine(uint8_t scanline) {

    uint8_t LCDC = m_memory->ReadByte(LCDC_ADDR);
    uint16_t tilemap_addr = (bitGet(LCDC, 3) == 1) ? 0x9c00 : 0x9800;    

    bool tile_data_unsigned_addressing = bitGet(LCDC, 4);

    uint8_t scx = m_memory->ReadByte(SCX_ADDR);
    uint8_t scy = m_memory->ReadByte(SCY_ADDR);

    uint8_t pixel_coord_y = (scanline + scy) & 0xFF;
    uint8_t y_tile = pixel_coord_y / 8;

    for (uint8_t x_offset = 0; x_offset < SCREEN_WIDTH; ++x_offset) {
        
        uint8_t pixel_coord_x = (x_offset + scx) & 0xFF;
        uint8_t x_tile = pixel_coord_x / 8;

        uint8_t tile_idnex = m_memory->ReadByte(tilemap_addr + (32 * y_tile + x_tile));

        uint16_t tile_addr = 0;

        if (tile_data_unsigned_addressing) {
            tile_addr = 0x8000 + 16 * tile_idnex;
        } else {
            tile_addr = 0x8800 + 16 * (int8_t)tile_idnex;
        }

        uint8_t local_pixel_x = 7 - (pixel_coord_x % 8);
        uint8_t local_pixel_y = (pixel_coord_y % 8);

        uint8_t first_tile_byte = m_memory->ReadByte(tile_addr + 2 * local_pixel_y);
        uint8_t second_tile_byte = m_memory->ReadByte(tile_addr + 2 * local_pixel_y + 1);

        uint8_t pixel_val = (static_cast<uint8_t>(bitGet(first_tile_byte, local_pixel_x))) | 
                            (static_cast<uint8_t>(bitGet(second_tile_byte, local_pixel_x)) << 1);

        const uint8_t palette[] = {0xFF, 0x00, 0x00, 0x00};

        m_framebuffer[scanline * SCREEN_WIDTH * 3 + x_offset * 3] = palette[pixel_val];
        m_framebuffer[scanline * SCREEN_WIDTH * 3 + x_offset * 3 + 1] = palette[pixel_val];
        m_framebuffer[scanline * SCREEN_WIDTH * 3 + x_offset * 3 + 2] = palette[pixel_val];

    }   
}

uint8_t Ppu::readByteWrap(uint16_t addr) {

    return 0xFF;
}   

void Ppu::writeByteWrap(uint16_t addr, uint8_t byte) {

}


void Ppu::PpuStep(unsigned int last_m_cycle_count) {

    if (!bitGet(m_memory->ReadByte(LCDC_ADDR), 7)) return;

    m_dot_pool += 4 * last_m_cycle_count;
    static uint8_t current_scanline = 0;

    while (m_dot_pool >= m_current_dots_need) {
        switch (m_current_mode) {
            case mode_t::H_Blank:
                hBlankStep(current_scanline);
                break;
            case mode_t::V_Blank:
                vBlankStep(current_scanline);
                break;
            case mode_t::OAM_Scan:
                oamScanStep(current_scanline);
                break;
            case mode_t::Drawing:
                drawingStep(current_scanline);
                break;
            default:
                printf("Unreckognised current ppu mode: %d\n", m_current_mode);
        }
    }
}

void Ppu::hBlankStep(uint8_t& current_scanline) {
    static unsigned int checkpoint = 0;

    switch (checkpoint) {
        case 0:

            if (m_dot_pool < 87) {
                m_current_dots_need = 87;
                return;
            }
            m_dot_pool -= 87;
            m_current_dots_need = 0;
            
            if (current_scanline == SCREEN_HEIGHT - 1) {
                m_current_mode = mode_t::V_Blank;
            } else {
                current_scanline++;
                m_current_mode = mode_t::OAM_Scan;
            }

            break;
        default:
            printf("Wrong checkpoint in hBlankStep: %u\n", checkpoint);
    }
}

void Ppu::vBlankStep(uint8_t& current_scanline) {
    
    static unsigned int checkpoint = 0;

    switch (checkpoint) {
        case 0: {
            uint8_t requests = m_memory->ReadByte(0xFF0F);
            bitSet(requests, 0, 1);
            m_memory->WriteByte(0xFF0F, requests);
            checkpoint++;
            break;
        }
        case 1: {

            if (m_dot_pool < 456) {
                m_current_dots_need = 456;
                return;
            }
            m_dot_pool -= 456;

            checkpoint++;

            break;
        }
        case 2: {

            if (current_scanline == 152) {
                m_current_mode = mode_t::OAM_Scan;
                current_scanline = 0;
            } else {
                current_scanline++;
                m_memory->WriteByte(LY_ADDR, current_scanline);
            }

            checkpoint = 0;

            break;
        }
        default:
            printf("Wrong checkpoint in hBlankStep: %u\n", checkpoint);
    }

}

void Ppu::oamScanStep(uint8_t& current_scanline) {
    static unsigned int checkpoint = 0;

    switch (checkpoint) {
        case 0:

            if (m_dot_pool < 80) {
                m_current_dots_need = 80;
                return;
            }
            m_dot_pool -= 80;
            m_current_dots_need = 0;
            m_current_mode = mode_t::Drawing;

            break;
        default:
            printf("Wrong checkpoint in oamScanStep: %u\n", checkpoint);
    }
}

void Ppu::drawingStep(uint8_t& current_scanline) {
    static unsigned int checkpoint = 0;

    switch (checkpoint) {
        case 0:
            if (m_dot_pool < 289) {
                m_current_dots_need = 289;
                return;
            }
            m_dot_pool -= 289;
            m_current_dots_need = 0;

            m_memory->WriteByte(LY_ADDR, current_scanline);
            
            renderBackgroundLine(current_scanline);

            // for (uint8_t i = 0;i < SCREEN_WIDTH; ++i) {
            //     const unsigned int current_frame_buffer_index = current_scanline * SCREEN_WIDTH * 3 + i * 3;
            //     m_framebuffer[current_frame_buffer_index] = current_scanline * (0xFF / SCREEN_HEIGHT);
            //     m_framebuffer[current_frame_buffer_index + 1] = i * (0xFF / SCREEN_WIDTH);
            //     m_framebuffer[current_frame_buffer_index + 2] = i * (0xFF / SCREEN_WIDTH);
            // }

            m_current_mode = mode_t::H_Blank;

            break;
        default:
            printf("Wrong checkpoint in drawingStep: %u\n", checkpoint);
    }
}


