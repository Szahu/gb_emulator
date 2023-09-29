#include <iostream>
#include <fstream>
#include "cpu.hpp"
#include <thread>
#include <chrono>
#include <memory>
#include "ppu.hpp"
#include "gui.hpp"
#include <atomic>

void load_program_from_file(const std::string &filename, char *mem, const unsigned int &pc) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char *>(mem + pc), size)) {
        throw std::runtime_error("Failed to load the program");
    }
}

int main(int argc, char** argv) {

    std::cout << "Starting the emulator" << std::endl;

    const unsigned int frequency = 1000000;
    const unsigned int mem_size = 0xFFFF + 1;
    const unsigned int sync_interval_nanos = 100000;

    uint8_t* framebuffer = new uint8_t[160 * 144 * 3];

    Memory mem(mem_size);
    Cpu cpu(&mem, frequency);
    Ppu ppu(&mem, framebuffer);

    std::atomic<bool> stop_signal = false;

    const long m_cycle_duration_nanos = static_cast<unsigned int>((1.0f / static_cast<float>(frequency)) * 1000000000.0f);

    
    load_program_from_file(PROJECT_DIR "/roms/tetris.gb",
                           (char *)cpu.GetMemory()->GetBufferLocation(), 0x0);
    load_program_from_file(PROJECT_DIR "/roms/dmg_boot.bin",
                           (char *)cpu.GetMemory()->GetBufferLocation(), 0x0);
    unsigned int cycle_count = 0;

    auto start = std::chrono::high_resolution_clock::now();    

    std::thread gui_thread([&] () {
        Gui gui(160, 144, framebuffer);
        while (!stop_signal) gui.RenderFrame(stop_signal);
    });

    while (!stop_signal) {

        unsigned int tmp = 1;
        cpu.CpuStep(stop_signal, tmp);
        ppu.PpuStep(tmp);
        cycle_count += tmp;

        
        auto stop = std::chrono::high_resolution_clock::now();
        long duration = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
        if (duration >= sync_interval_nanos) {
            long simulated_running_time_ns = cycle_count * m_cycle_duration_nanos;
            SLEEP_NANOS(simulated_running_time_ns - duration);
            if (duration > frequency * m_cycle_duration_nanos) {
                printf("%u cycles took too long: %u\n", cycle_count, duration);
            }
            start = std::chrono::high_resolution_clock::now(); 
            cycle_count = 0;
        }
    }

    gui_thread.join();

    delete[] framebuffer;
    std::cout << "Terminating the emulator" << std::endl;

    return 0;
}