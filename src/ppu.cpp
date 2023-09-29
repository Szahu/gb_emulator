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

// void Ppu::renderBackgroundLine(uint8_t scanline) {

//     uint16_t tilemap_addr = 0x9800;
//     unsigned int x_position = 0;
//     uint8_t LCDC = m_memory->ReadByte(LCDC_ADDR);
//     uint8_t wx = m_memory->ReadByte(WX_ADDR);

//     bool tile_data_unsigned_addressing = bitGet(LCDC, 4);

//     if (bitGet(LCDC, 3) && x_position < wx - 7) tilemap_addr = 0x9C00;
//     if (bitGet(LCDC, 6) && x_position >= wx - 7) tilemap_addr = 0x9C00;

//     // uint8_t scx = m_memory->ReadByte(SCX_ADDR);
//     // uint8_t scy = m_memory->ReadByte(SCY_ADDR);

//     uint8_t scx = 0;
//     uint8_t scy = 0;

//     uint8_t pixel_coord_y = (scanline + scy) & 0xFF;

//     for (uint8_t x_offset = 0; x_offset < SCREEN_WIDTH; ++x_offset) {
//         uint8_t pixel_coord_x = ((scx / 8) + x_position) & 0x1F;
//         uint8_t tile_map_index = (pixel_coord_y / 8) * 32 + (pixel_coord_x / 8);
//         uint8_t first_line_addr;
//         if (tile_data_unsigned_addressing) {
//             first_line_addr = 0x8000 + tile_map_index * 16 + pixel_coord_y % 8;
//         } else {
//             first_line_addr = 0x8800 + (int8_t)tile_map_index * 16 + pixel_coord_y % 8;
//         }

//         uint8_t tile_first_line = m_memory->ReadByte(first_line_addr);
//         uint8_t tile_second_line = m_memory->ReadByte(first_line_addr + 1);

//         uint8_t col_val = (tile_first_line & (0b10000000 >> (pixel_coord_x % 8))) | 
//                           ((tile_second_line & (0b10000000 >> (pixel_coord_x % 8))) << 1);

//         m_framebuffer[scanline * SCREEN_WIDTH * 3 + x_offset * 3] = col_val ? 0xFF : 0;
//         m_framebuffer[scanline * SCREEN_WIDTH * 3 + x_offset * 3 + 1] = col_val ? 0xFF : 0;
//         m_framebuffer[scanline * SCREEN_WIDTH * 3 + x_offset * 3 + 2] = col_val ? 0xFF : 0;
//     }   
// }

uint8_t Ppu::readByteWrap(uint16_t addr) {

    return 0xFF;
}   

void Ppu::writeByteWrap(uint16_t addr, uint8_t byte) {

}


void Ppu::PpuStep(unsigned int last_m_cycle_count) {

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
    static unsigned int dots_used = 0;

    switch (checkpoint) {
        case 0:

            if (m_dot_pool < 6) {
                m_current_dots_need = 6;
                return;
            }
            dots_used += 6;
            m_dot_pool -= 6;

            if (dots_used == 456) {
                checkpoint++;
            }

            break;
        case 1:

            if (current_scanline == 152) {
                m_current_mode = mode_t::OAM_Scan;
                current_scanline = 0;
            } else {
                current_scanline++;
                m_memory->WriteByte(LY_ADDR, current_scanline);
            }

            checkpoint = 0;
            dots_used = 0;

            break;
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
            
            for (uint8_t i = 0;i < SCREEN_WIDTH; ++i) {
                m_framebuffer[current_scanline * SCREEN_WIDTH * 3 + i * 3] = current_scanline * (0xFF / SCREEN_HEIGHT);
                m_framebuffer[current_scanline * SCREEN_WIDTH * 3 + i * 3 + 1] = i * (0xFF / SCREEN_WIDTH);
                m_framebuffer[current_scanline * SCREEN_WIDTH * 3 + i * 3 + 2] = i * (0xFF / SCREEN_WIDTH);
            }

            m_current_mode = mode_t::H_Blank;

            break;
        default:
            printf("Wrong checkpoint in drawingStep: %u\n", checkpoint);
    }
}


