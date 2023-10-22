#pragma once
#include "memory.hpp"
#include "audio_layer.hpp"

class Apu {
public:
    Apu(Memory* memory_ref, AudioLayer* audio_layer_ref);

    void ApuStep(unsigned int m_cycle_count);

private:

    void handleChannel1(unsigned int m_cycle_count, bool produce_sample);
    void pushSampleToMixer(int16_t sample, unsigned int channel);

private:
    Memory* m_memory = nullptr;
    AudioLayer* m_audio_layer = nullptr;
    int16_t m_mixer_buffer[4];

    // Global registers
    static constexpr uint16_t AUDIO_MASTER_CONTROLL_ADDR = 0xFF26; // NR52 
    static constexpr uint16_t SOUND_PANNING_ADDR = 0xFF25; // NR51 
    static constexpr uint16_t MASTER_VOLUME_ADDR = 0xFF24; // NR50

    // Per channel registers

    // channel 1
    static constexpr uint16_t CHANNEL_1_SWEEP_ADDR = 0xFF10; // NR10
    static constexpr uint16_t CHANNEL_1_LENGTH_AND_DUTY_ADDR = 0xFF11; // NR11
    static constexpr uint16_t CHANNEL_1_VOLUME_AND_ENVELOPE_ADDR = 0xF12; // NR12
    static constexpr uint16_t CHANNEL_1_PERIOD_LOW_ADDR = 0xFF13; // NR13
    static constexpr uint16_t CHANNER_1_PERIOD_HIGH_ADDR = 0xFF14; // NR14

    static constexpr unsigned int ITERATION_LENGTH_BASE = 8192; // 2^20 / 128

    static constexpr uint8_t DUTY_CYCLES[] = {
        1, 1, 1, 1, 1, 1, 1, 0,
        0, 1, 1, 1, 1, 1, 1, 0,
        0, 1, 1, 1, 1, 0, 0, 0,
        1, 0, 0, 0, 0, 0, 0, 0,
    };
};