#pragma once
#include "memory.hpp"
#include "audio_layer.hpp"

class Apu {
public:
    Apu(Memory* memory_ref, AudioLayer* audio_layer_ref);

    void ApuStep(unsigned int m_cycle_count);

private:

    void handleChannel1(unsigned int m_cycle_count, bool produce_sample);
    void handleChannel2(unsigned int m_cycle_count, bool produce_sample);
    void handleChannel3(unsigned int m_cycle_count, bool produce_sample);
    void pushSampleToMixer(float sampleL, float sampleR, unsigned int channel);

private:

    struct sample_t {
        float left;
        float right;
    };

    Memory* m_memory = nullptr;
    AudioLayer* m_audio_layer = nullptr;
    float m_mixer_buffer[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    float m_volume = 0.5f;


    // Global registers
    static constexpr uint16_t AUDIO_MASTER_CONTROLL_ADDR = 0xFF26; // NR52 
    static constexpr uint16_t SOUND_PANNING_ADDR = 0xFF25; // NR51 
    static constexpr uint16_t MASTER_VOLUME_ADDR = 0xFF24; // NR50

    // Per channel registers

    // channel 1
    static constexpr uint16_t CHANNEL_1_SWEEP_ADDR = 0xFF10; // NR10
    static constexpr uint16_t CHANNEL_1_LENGTH_AND_DUTY_ADDR = 0xFF11; // NR11
    static constexpr uint16_t CHANNEL_1_VOLUME_AND_ENVELOPE_ADDR = 0xFF12; // NR12
    static constexpr uint16_t CHANNEL_1_PERIOD_LOW_ADDR = 0xFF13; // NR13
    static constexpr uint16_t CHANNEL_1_PERIOD_HIGH_ADDR = 0xFF14; // NR14

    // channel 2
    static constexpr uint16_t CHANNEL_2_LENGTH_AND_DUTY_ADDR = 0xFF16; // NR11
    static constexpr uint16_t CHANNEL_2_VOLUME_AND_ENVELOPE_ADDR = 0xFF17; // NR12
    static constexpr uint16_t CHANNEL_2_PERIOD_LOW_ADDR = 0xFF18; // NR13
    static constexpr uint16_t CHANNEL_2_PERIOD_HIGH_ADDR = 0xFF19; // NR14

    // channel 3
    static constexpr uint16_t CHANNEL_3_DAC_ENABLE_ADDR = 0xFF1A; // NR30
    static constexpr uint16_t CHANNEL_3_LENGTH_TIMER_ADDR = 0xFF1B; // NR31
    static constexpr uint16_t CHANNEL_3_OUTPUT_LEVEL_ADDR = 0xFF1C; // NR32
    static constexpr uint16_t CHANNEL_3_PERIOD_LOW_ADDR = 0xFF1D; // NR33
    static constexpr uint16_t CHANNEL_3_PERIOD_HIGH_CONTROL_ADDR = 0xFF1E; // NR34
    static constexpr uint16_t CHANNEL_3_WAVE_RAM_ADDR = 0xFF30; // NR34
    static constexpr uint16_t CHANNEL_3_WAVE_RAM_SIZE = 0x10; // NR34

    static constexpr uint8_t DUTY_CYCLES[] = {
        1, 1, 1, 1, 1, 1, 1, 0,
        0, 1, 1, 1, 1, 1, 1, 0,
        0, 1, 1, 1, 1, 0, 0, 0,
        1, 0, 0, 0, 0, 0, 0, 0,
    };
};