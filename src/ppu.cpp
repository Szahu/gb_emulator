#include "ppu.hpp"
#include "common.hpp"
#include <memory>
#include <stdexcept>

Ppu::Ppu(Memory *mem_ref, std::vector<uint8_t>& frame_buffer_ref, std::function<void()> frame_ready_callback)
:m_memory{mem_ref}, m_framebuffer{frame_buffer_ref} {
    m_frame_ready_callback = frame_ready_callback;
}

Ppu::~Ppu() {}

void Ppu::oamScan(uint8_t scanline, OAM_t* oam_buffer, uint8_t& oam_buffer_index, uint16_t& oam_ptr) {

    bool found = false;

    while (!found && oam_ptr < OAM_ADDR + OAM_SIZE) {
        const OAM_t oam = {
            m_memory->ReadByteDirect(oam_ptr),
            m_memory->ReadByteDirect(oam_ptr+1),
            m_memory->ReadByteDirect(oam_ptr+2),
            m_memory->ReadByteDirect(oam_ptr+3),
        };

        const bool x_condition = oam.x_position > 0;
        const bool y_condition = oam.y_position >= 16;
        const bool y_s_condition = scanline >= oam.y_position - 16 &&
                                    scanline < oam.y_position - 8;

        if (x_condition && y_condition && y_s_condition) {
            oam_buffer[oam_buffer_index++] = oam;
            found = true;
        }

        oam_ptr += 4;
    }

    
}

void Ppu::renderObjectLine(uint8_t scanline, OAM_t* oam_buffer, uint8_t buffer_size) {
    ASSERT(scanline < SCREEN_HEIGHT);

    const uint8_t lcdc = m_memory->ReadByteDirect(LCDC_ADDR);
    const bool double_height_mode = bitGet(lcdc, 2);
    if (double_height_mode) printf("DOUBLE HEIGHT NOT SUPPORTED!\n");

    for (uint8_t object_index = 0; object_index < buffer_size; ++object_index) {
        OAM_t current_oam = oam_buffer[object_index];
        
        if (double_height_mode) {
            current_oam.tile_index &= 0xFE; // ignore the least significant bit in double mode
        }

        const bool flip_x = bitGet(current_oam.flags, 5);
        const bool flip_y = bitGet(current_oam.flags, 6);

        const uint16_t tile_addr = 0x8000 + 16 * current_oam.tile_index;
        uint8_t y_offset_local = (scanline - (current_oam.y_position & 0x7)) & 0x7;

        if (flip_y) y_offset_local = 7 - y_offset_local;

        const uint8_t first_byte = m_memory->ReadByteDirect(tile_addr + 2 * y_offset_local);
        const uint8_t second_byte = m_memory->ReadByteDirect(tile_addr + 2 * y_offset_local + 1);

        int x_offset = 0 + current_oam.x_position - 8;
        for (uint8_t pixel_x = 0; pixel_x < 8 && x_offset < static_cast<int>(SCREEN_WIDTH); pixel_x++) {
            const uint8_t bit_number = flip_x ? pixel_x : 7 - pixel_x;
            const uint8_t color_index = (static_cast<uint8_t>(bitGet(first_byte, bit_number))) | 
                                        (static_cast<uint8_t>(bitGet(second_byte, bit_number)) << 1);
            
            x_offset = pixel_x + current_oam.x_position - 8;

            if (x_offset < 0) continue;

            if (color_index != 0) {
                m_framebuffer[scanline * SCREEN_WIDTH * 3 + x_offset * 3] = PALETTE[color_index * 3];
                m_framebuffer[scanline * SCREEN_WIDTH * 3 + x_offset * 3 + 1] = PALETTE[color_index * 3 + 1];
                m_framebuffer[scanline * SCREEN_WIDTH * 3 + x_offset * 3 + 2] = PALETTE[color_index * 3 + 2];
            }
        }
    }    

}

void Ppu::renderBackgroundLine(uint8_t scanline) {
        
    if (scanline >= SCREEN_HEIGHT) {
        throw std::runtime_error("Out of range rendering!");
    }

    const uint8_t LCDC = m_memory->ReadByteDirect(LCDC_ADDR);
    const bool tile_data_unsigned_addressing = bitGet(LCDC, 4);

    const uint8_t scx = m_memory->ReadByteDirect(SCX_ADDR);
    const uint8_t scy = m_memory->ReadByteDirect(SCY_ADDR);
    const uint8_t wx = m_memory->ReadByteDirect(WX_ADDR);
    const uint8_t wy = m_memory->ReadByteDirect(WY_ADDR);

    const uint8_t pixel_coord_y = (scanline + scy) & 0xFF;
    const uint16_t y_tile = pixel_coord_y / 8;

    const uint8_t pallete = m_memory->ReadByteDirect(0xFF47);

    for (uint8_t x_offset = 0; x_offset < SCREEN_WIDTH; ++x_offset) {
        
        const bool inside_window = x_offset + 7 >= wx && scanline >= wy && bitGet(LCDC, 5);

        uint16_t tilemap_addr = 0x9800;
        if (!inside_window && bitGet(LCDC, 3)) tilemap_addr = 0x9C00;
        if (inside_window && bitGet(LCDC, 6)) tilemap_addr = 0x9C00;

        const uint8_t pixel_coord_x = (x_offset + (scx & 0xFF)) & 0xFF;
        const uint16_t x_tile = pixel_coord_x / 8;

        const uint8_t tile_idnex = m_memory->ReadByteDirect(tilemap_addr + x_tile + (y_tile << 5));

        uint16_t tile_addr = 0;

        if (tile_data_unsigned_addressing) {
            tile_addr = 0x8000 + 16 * tile_idnex;
        } else {
            tile_addr = 0x9000 + 16 * (int8_t)tile_idnex;
        }

        const uint8_t local_pixel_x = 7 - (pixel_coord_x % 8);
        const uint8_t local_pixel_y = (pixel_coord_y % 8);

        const uint8_t first_tile_byte = m_memory->ReadByteDirect(tile_addr + 2 * local_pixel_y);
        const uint8_t second_tile_byte = m_memory->ReadByteDirect(tile_addr + 2 * local_pixel_y + 1);

        const uint8_t pixel_val = (static_cast<uint8_t>(bitGet(first_tile_byte, local_pixel_x))) | 
                                  (static_cast<uint8_t>(bitGet(second_tile_byte, local_pixel_x)) << 1);

        m_framebuffer[scanline * SCREEN_WIDTH * 3 + x_offset * 3] = PALETTE[pixel_val * 3];
        m_framebuffer[scanline * SCREEN_WIDTH * 3 + x_offset * 3 + 1] = PALETTE[pixel_val * 3 + 1];
        m_framebuffer[scanline * SCREEN_WIDTH * 3 + x_offset * 3 + 2] = PALETTE[pixel_val * 3 + 2];

    }   
}

void Ppu::newScanlineCallback(uint8_t current_scanline) {

    m_memory->WriteByteDirect(LY_ADDR, current_scanline);

    uint8_t stat = m_memory->ReadByteDirect(STAT_ADDR);
    uint8_t lyc = m_memory->ReadByteDirect(LYC_ADDR);

    if (current_scanline == lyc) {
        bitSet(stat, 2, 1);
        if (bitGet(stat, 6)) {
            requestStatInterrupt();
        }
    } else {
        bitSet(stat, 2, 0);
    }
    m_memory->WriteByteDirect(STAT_ADDR, stat);

}

void Ppu::requestStatInterrupt() {
    uint8_t requests = m_memory->ReadByteDirect(0xFF0F);
    bitSet(requests, 1, 1);
    m_memory->WriteByteDirect(0xFF0F, requests);
}

void Ppu::PpuStep(unsigned int last_m_cycle_count) {

    static uint8_t current_scanline = 0;
    static uint8_t buffer_size = 0;
    
    if (!bitGet(m_memory->ReadByteDirect(LCDC_ADDR), 7)) {
        m_memory->WriteByteDirect(LY_ADDR, 0);
        current_scanline = 0;
        buffer_size = 0;
        m_current_mode = ppu_mode_t::OAM_Scan;
        m_dot_pool = 0;
        return;
    }

    // update current mode
    uint8_t stat = m_memory->ReadByteDirect(STAT_ADDR);
    uint8_t cur_mode = static_cast<uint8_t>(m_current_mode);
    stat &= ~0x3; // clera last 2 bits
    stat = static_cast<uint8_t>(stat | cur_mode);
    m_memory->WriteByteDirect(STAT_ADDR, stat);

    m_dot_pool += 4 * last_m_cycle_count;

    static OAM_t oam_buffer[10];

    while (m_dot_pool >= m_current_dots_need) {
        switch (m_current_mode) {
            case ppu_mode_t::H_Blank:
                hBlankStep(current_scanline);
                break;
            case ppu_mode_t::V_Blank:
                vBlankStep(current_scanline);
                break;
            case ppu_mode_t::OAM_Scan:
                oamScanStep(current_scanline, oam_buffer, buffer_size);
                break;
            case ppu_mode_t::Drawing:
                drawingStep(current_scanline, oam_buffer, buffer_size);
                break;
            default:
                printf("Unreckognised current ppu mode: %d\n", m_current_mode);
                ASSERT(false);
        }
    }
}

void Ppu::hBlankStep(uint8_t& current_scanline) {
    static unsigned int checkpoint = 0;

    switch (checkpoint) {
        case 0: {
            if (bitGet(m_memory->ReadByteDirect(STAT_ADDR), 3)) {
                requestStatInterrupt();
            }
            checkpoint++;
            break;
        }
        case 1:

            if (m_dot_pool < 87) {
                m_current_dots_need = 87;
                return;
            }
            m_dot_pool -= 87;
            m_current_dots_need = 0;
            
            if (current_scanline == SCREEN_HEIGHT - 1) {
                m_current_mode = ppu_mode_t::V_Blank;
                current_scanline++;
                newScanlineCallback(current_scanline);
                m_frame_ready_callback();
            } else {
                current_scanline++;
                newScanlineCallback(current_scanline);
                m_current_mode = ppu_mode_t::OAM_Scan;
            }

            checkpoint = 0;

            break;
        default:
            printf("Wrong checkpoint in hBlankStep: %u\n", checkpoint);
            ASSERT(false);
    }
}

void Ppu::vBlankStep(uint8_t& current_scanline) {
    
    static unsigned int checkpoint = 0;

    switch (checkpoint) {
        case 0: {
            uint8_t requests = m_memory->ReadByteDirect(0xFF0F);
            bitSet(requests, 0, 1);
            m_memory->WriteByteDirect(0xFF0F, requests);

            if (bitGet(m_memory->ReadByteDirect(STAT_ADDR), 4)) {
                requestStatInterrupt();
            }

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

            if (current_scanline == 153) {
                m_current_mode = ppu_mode_t::OAM_Scan;
                current_scanline = 0;
                newScanlineCallback(current_scanline);
                checkpoint = 0;
            } else {
                current_scanline++;
                newScanlineCallback(current_scanline);
                checkpoint = 1;
            }

            break;
        }
        default:
            printf("Wrong checkpoint in hBlankStep: %u\n", checkpoint);
            ASSERT(false);
    }

}

void Ppu::oamScanStep(uint8_t& current_scanline, OAM_t* oam_buffer, uint8_t& oam_buffer_size) {
    static unsigned int checkpoint = 0;

    switch (checkpoint) {
        case 0: {
            if (bitGet(m_memory->ReadByteDirect(STAT_ADDR), 5)) {
                requestStatInterrupt();
            }
        
            oam_buffer_size = 0;
            checkpoint++;
            break;
        }
        case 1: {
            static unsigned int total_dots = 0;
            static uint16_t oam_ptr = OAM_ADDR;

            if (m_dot_pool < 8) {
                m_current_dots_need = 8;
                return;
            }

            oamScan(current_scanline, oam_buffer, oam_buffer_size, oam_ptr);

            m_dot_pool -= 8;
            total_dots += 8;

            if (total_dots == 80) {
                m_current_mode = ppu_mode_t::Drawing;
                m_current_dots_need = 0;
                checkpoint = 0;
                total_dots = 0;
                oam_ptr = OAM_ADDR;
            } else {
                m_current_dots_need = 8;
            }
            
            break;
        }
        default:
            printf("Wrong checkpoint in oamScanStep: %u\n", checkpoint);
            ASSERT(false);
    }
}

void Ppu::drawingStep(uint8_t& current_scanline, OAM_t* oam_buffer, uint8_t buffer_size) {
    static unsigned int checkpoint = 0;

    switch (checkpoint) {
        case 0: {
            checkpoint++;
            break;
        }
        case 1: {
            if (m_dot_pool < 289) {
                m_current_dots_need = 289;
                return;
            }
            m_dot_pool -= 289;
            m_current_dots_need = 0;

            renderBackgroundLine(current_scanline);
            renderObjectLine(current_scanline, oam_buffer, buffer_size);

            m_current_mode = ppu_mode_t::H_Blank;
            checkpoint = 0;
            break;
        }
        default:
            printf("Wrong checkpoint in drawingStep: %u\n", checkpoint);
            ASSERT(false);
    }
}

