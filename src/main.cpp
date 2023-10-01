#include <iostream>
#include <fstream>
#include "cpu.hpp"
#include <thread>
#include <chrono>
#include <memory>
#include "ppu.hpp"
#include "gui.hpp"
#include "timer.hpp"
#include <atomic>
#include <vector>

void load_program_from_file(const std::string &filename, std::vector<uint8_t>& data) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    data.reserve(size);
    if (!file.read(reinterpret_cast<char *>(data.data()), size)) {
        throw std::runtime_error("Failed to load the program");
    }
}

void setupPostBootData(Memory& mem);

int main(int argc, char** argv) {

    std::cout << "Starting the emulator" << std::endl;

    const unsigned int frequency = 1048576;
    const unsigned int mem_size = 0xFFFF + 1;
    const unsigned int sync_interval_nanos = 1000000;

    uint8_t* framebuffer = new uint8_t[160 * 144 * 3];

    Memory mem(mem_size);
    Cpu cpu(&mem, frequency);
    Ppu ppu(&mem, framebuffer);
    Timer timer(&mem);

    std::atomic<bool> stop_signal = false;

    const long m_cycle_duration_nanos = static_cast<unsigned int>((1.0f / static_cast<float>(frequency)) * 1000000000.0f);
    
    // const std::string game_rom_path = PROJECT_DIR"/roms/tetris.gb";
    const std::string game_rom_path = PROJECT_DIR"/roms/cpu_instrs/cpu_instrs.gb";

    std::vector<uint8_t> rom_buffer;
    load_program_from_file(game_rom_path, rom_buffer);
    mem.LoadRom(rom_buffer.data(), rom_buffer.size());

    rom_buffer = std::vector<uint8_t>();


    std::cout << "Running rom: " << game_rom_path << std::endl;

    auto start = std::chrono::high_resolution_clock::now();    
    bool boot_done = false;

    setupPostBootData(mem);
    cpu.PostBoodSetup();

    std::thread gui_thread([&] () {
        Gui gui(160, 144, framebuffer);
        while (!stop_signal) gui.RenderFrame(stop_signal);
    });

    unsigned int cycle_count = 0;

    while (!stop_signal) {

        unsigned int tmp = 0;
        cpu.CpuStep(stop_signal, tmp);
        ppu.PpuStep(tmp);
        timer.TimerStep(tmp);

        cycle_count += tmp;

        // auto stop = std::chrono::high_resolution_clock::now();
        // long duration = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
        // if (duration >= sync_interval_nanos) {
        //     long simulated_running_time_ns = cycle_count * m_cycle_duration_nanos;
        //     SLEEP_NANOS(simulated_running_time_ns - duration);
        //     if (duration > frequency * m_cycle_duration_nanos) {
        //         printf("%u cycles took too long: %u\n", cycle_count, duration);
        //     }
        //     start = std::chrono::high_resolution_clock::now(); 
        //     cycle_count = 0;
        // }
    }

    gui_thread.join();

    delete[] framebuffer;
    std::cout << "Terminating the emulator" << std::endl;

    return 0;
}

void setupPostBootData(Memory& mem) {
    mem.WriteByte(0xFF00, 0xCF);   // P1
    mem.WriteByte(0xFF01, 0x00);   // SB
    mem.WriteByte(0xFF02, 0x7E);   // SC
    mem.WriteByte(0xFF04, 0x18);   // DIV
    mem.WriteByte(0xFF05, 0x00);   // TIMA
    mem.WriteByte(0xFF06, 0x00);   // TMA
    mem.WriteByte(0xFF07, 0xF8);   // TAC
    mem.WriteByte(0xFF0F, 0xE1);   // IF
    mem.WriteByte(0xFF10, 0x80);   // NR10
    mem.WriteByte(0xFF11, 0xBF);   // NR11
    mem.WriteByte(0xFF12, 0xF3);   // NR12
    mem.WriteByte(0xFF13, 0xFF);   // NR13
    mem.WriteByte(0xFF14, 0xBF);   // NR14
    mem.WriteByte(0xFF16, 0x3F);   // NR21
    mem.WriteByte(0xFF17, 0x00);   // NR22
    mem.WriteByte(0xFF18, 0xFF);   // NR23
    mem.WriteByte(0xFF19, 0xBF);   // NR24
    mem.WriteByte(0xFF1A, 0x7F);   // NR30
    mem.WriteByte(0xFF1B, 0xFF);   // NR31
    mem.WriteByte(0xFF1C, 0x9F);   // NR32
    mem.WriteByte(0xFF1D, 0xFF);   // NR33
    mem.WriteByte(0xFF1E, 0xBF);   // NR34
    mem.WriteByte(0xFF20, 0xFF);   // NR41
    mem.WriteByte(0xFF21, 0x00);   // NR42
    mem.WriteByte(0xFF22, 0x00);   // NR43
    mem.WriteByte(0xFF23, 0xBF);   // NR44
    mem.WriteByte(0xFF24, 0x77);   // NR50
    mem.WriteByte(0xFF25, 0xF3);   // NR51
    mem.WriteByte(0xFF26, 0xF1);   // NR52
    mem.WriteByte(0xFF40, 0x91);   // LCDC
    mem.WriteByte(0xFF41, 0x81);   // STAT
    mem.WriteByte(0xFF42, 0x00);   // SCY
    mem.WriteByte(0xFF43, 0x00);   // SCX
    mem.WriteByte(0xFF44, 0x91);   // LY
    mem.WriteByte(0xFF45, 0x00);   // LYC
    mem.WriteByte(0xFF46, 0xFF);   // DMA
    mem.WriteByte(0xFF47, 0xFC);   // BGP
    mem.WriteByte(0xFF48, 0x00);   // OBP0
    mem.WriteByte(0xFF49, 0x00);   // OBP1
    mem.WriteByte(0xFF4A, 0x00);   // WY
    mem.WriteByte(0xFF4B, 0x00);   // WX
    mem.WriteByte(0xFF4D, 0xFF);   // KEY1
    mem.WriteByte(0xFF4F, 0xFF);   // VBK
    mem.WriteByte(0xFF51, 0xFF);   // HDMA1
    mem.WriteByte(0xFF52, 0xFF);   // HDMA2
    mem.WriteByte(0xFF53, 0xFF);   // HDMA3
    mem.WriteByte(0xFF54, 0xFF);   // HDMA4
    mem.WriteByte(0xFF55, 0xFF);   // HDMA5
    mem.WriteByte(0xFF56, 0xFF);   // RP
    mem.WriteByte(0xFF68, 0xFF);   // BCPS
    mem.WriteByte(0xFF69, 0xFF);   // BCPD
    mem.WriteByte(0xFF6A, 0xFF);   // OCPS
    mem.WriteByte(0xFF6B, 0xFF);   // OCPD
    mem.WriteByte(0xFF70, 0xFF);   // SVBK
    mem.WriteByte(0xFFFF, 0x00);   // IE
}
