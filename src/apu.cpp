#include "apu.hpp"
#include "common.hpp"

Apu::Apu(Memory* memory_ref, AudioLayer* audio_layer_ref):
 m_memory(memory_ref), m_audio_layer(audio_layer_ref) {}

void Apu::ApuStep(unsigned int m_cycle_count) {
    static unsigned int cycle_pool = 0;
    cycle_pool += m_cycle_count;
    static const unsigned int CYCLES_PER_SAMPLE = (1 << 20) / m_audio_layer->GetSampleRate();
    const bool produce_sample = cycle_pool >= CYCLES_PER_SAMPLE;

    handleChannel1(m_cycle_count, produce_sample);
    handleChannel2(m_cycle_count, produce_sample);

    if (produce_sample) {
        cycle_pool -= CYCLES_PER_SAMPLE;

        m_audio_layer->PushSamples(m_mixer_buffer, sizeof(m_mixer_buffer));
    }

}

void Apu::pushSampleToMixer(float sample, unsigned int channel) {
    m_mixer_buffer[channel] = sample;
}

void Apu::handleChannel1(unsigned int m_cycle_count, bool produce_sample) {
    static unsigned int cycle_pool = 0;
    cycle_pool += m_cycle_count;

    const uint8_t length_and_duty_register = m_memory->ReadByteDirect(CHANNEL_1_LENGTH_AND_DUTY_ADDR);
    const uint8_t volume_and_envelope_register = m_memory->ReadByteDirect(CHANNEL_1_VOLUME_AND_ENVELOPE_ADDR);
    const uint8_t period_low_register = m_memory->ReadByteDirect(CHANNEL_1_PERIOD_LOW_ADDR);
    const uint8_t period_high_and_control_register = m_memory->ReadByteDirect(CHANNER_1_PERIOD_HIGH_ADDR);
    
    const uint8_t wave_duty = (length_and_duty_register >> 6) & 0x3;

    const uint16_t period_value = period_low_register | ((period_high_and_control_register & 0b111) << 8);
    const bool trigger = bitGet(period_high_and_control_register, 7);
    const bool length_enable = bitGet(period_high_and_control_register, 6);

    static uint16_t period_clock = 2048;
    period_clock += m_cycle_count;

    static unsigned int sample_counter = 0;

    if (period_clock >= 2048) {
        period_clock = period_value;
        sample_counter = (sample_counter + 1) % 8;
    }

    if (produce_sample) {
        pushSampleToMixer(static_cast<float>(DUTY_CYCLES[wave_duty * 8 + sample_counter]), 0);
    }
}

void Apu::handleChannel2(unsigned int m_cycle_count, bool produce_sample) {
    static unsigned int cycle_pool = 0;
    cycle_pool += m_cycle_count;

    const uint8_t length_and_duty_register = m_memory->ReadByteDirect(CHANNEL_2_LENGTH_AND_DUTY_ADDR);
    const uint8_t volume_and_envelope_register = m_memory->ReadByteDirect(CHANNEL_2_VOLUME_AND_ENVELOPE_ADDR);
    const uint8_t period_low_register = m_memory->ReadByteDirect(CHANNEL_2_PERIOD_LOW_ADDR);
    const uint8_t period_high_and_control_register = m_memory->ReadByteDirect(CHANNER_2_PERIOD_HIGH_ADDR);
    
    const uint8_t wave_duty = (length_and_duty_register >> 6) & 0x3;

    const uint16_t period_value = period_low_register | ((period_high_and_control_register & 0b111) << 8);
    const bool trigger = bitGet(period_high_and_control_register, 7);
    const bool length_enable = bitGet(period_high_and_control_register, 6);

    static uint16_t period_clock = 2048;
    period_clock += m_cycle_count;

    static unsigned int sample_counter = 0;

    if (period_clock >= 2048) {
        period_clock = period_value;
        sample_counter = (sample_counter + 1) % 8;
    }

    if (produce_sample) {
        pushSampleToMixer(static_cast<float>(DUTY_CYCLES[wave_duty * 8 + sample_counter]), 1);
    }
}

void Apu::handleChannel3(unsigned int m_cycle_count, bool produce_sample) {
    static unsigned int cycle_pool = 0;
    cycle_pool += m_cycle_count;

    const uint8_t period_low_register = m_memory->ReadByteDirect(CHANNEL_3_PERIOD_LOW_ADDR);
    const uint8_t period_high_control_register = m_memory->ReadByteDirect(CHANNEL_3_PERIOD_HIGH_CONTROL_ADDR);
    const unsigned int period_value = period_low_register | ((period_high_control_register & 0x3) << 8);
    const uint8_t volume = (m_memory->ReadByte(CHANNEL_3_OUTPUT_LEVEL_ADDR) >> 5) & 0x3;

    static unsigned int period = 0;
    static unsigned int sample_counter = 0;

    period += 2 * m_cycle_count;

    if (period >= 2048) {
        period = period_value;
        sample_counter = (sample_counter + 1) % 32;
    }

    if (produce_sample) {
        uint8_t sample = m_memory->ReadByteDirect(CHANNEL_3_WAVE_RAM_ADDR + sample_counter / 2);
        if (sample_counter & 0b1) {
            sample = sample >> 4;
        } else {
            sample = sample & 0b1111;
        }
        if (volume == 0) {
            sample = 0;
        } else {
            sample = sample >> (volume - 1);
        }
        pushSampleToMixer(static_cast<float>(sample), 3);
    }
}