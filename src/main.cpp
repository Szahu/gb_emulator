#include <iostream>
#include <fstream>
#include "cpu.hpp"
#include <thread>
#include <chrono>
#include <memory>

void load_program_from_file(const std::string &filename, char *mem, const unsigned int &pc) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char *>(mem + pc), size)) {
        throw std::runtime_error("Failed to load the program");
    }
}

int main(int argc, char** argv) {

    std::unique_ptr<Memory> mem(new Memory(0xFFFF));
    Cpu cpu(mem.get(), 1000);
    
    load_program_from_file(PROJECT_DIR"/test/cpu_instrs/individual/06-ld r,r.gb", (char*)cpu.GetMemory()->GetBufferLocation(), 0);

    bool stop_signal = false;

    while (!stop_signal) {
        cpu.CpuStep(stop_signal);
    }

    std::cout << "Hello World!" << std::endl;

    return 0;
}