#include "apu.hpp"
#include "common.hpp"

Apu::Apu(Memory* memory_ref, AudioLayer* audio_layer_ref):
 m_memory(memory_ref), m_audio_layer(audio_layer_ref) {
    std::memset(m_mixer_buffer, 0x0, 4 * sizeof(int16_t));
}

void Apu::ApuStep(unsigned int m_cycle_count) {
    static unsigned int cycle_pool = 0;
    cycle_pool += m_cycle_count;
    static const unsigned int CYCLES_PER_SAMPLE = (1 << 20) / m_audio_layer->GetSampleRate();
    const bool produce_sample = cycle_pool >= CYCLES_PER_SAMPLE;

    handleChannel1(m_cycle_count, produce_sample);

    if (produce_sample) {
        cycle_pool -= CYCLES_PER_SAMPLE;
        // float sample = 0;
        // sample += m_mixer_buffer[0];
        // sample += m_mixer_buffer[1];
        // sample += m_mixer_buffer[2];
        // sample += m_mixer_buffer[3];

        // const float volume = 0.5f;
        // sample *= volume;
        m_audio_layer->PushSample(m_mixer_buffer[0]);
    }

}

void Apu::pushSampleToMixer(int16_t sample, unsigned int channel) {
    m_mixer_buffer[channel] = sample;
}

void Apu::handleChannel1(unsigned int m_cycle_count, bool produce_sample) {
    static unsigned int cycle_pool = 0;
    cycle_pool += m_cycle_count;

    const uint8_t sweep_register = m_memory->ReadByteDirect(CHANNEL_1_SWEEP_ADDR); 
    const uint8_t length_and_duty_register = m_memory->ReadByteDirect(CHANNEL_1_LENGTH_AND_DUTY_ADDR);
    const uint8_t volume_and_envelope_register = m_memory->ReadByteDirect(CHANNEL_1_VOLUME_AND_ENVELOPE_ADDR);
    const uint8_t period_low_register = m_memory->ReadByteDirect(CHANNEL_1_PERIOD_LOW_ADDR);
    const uint8_t period_high_and_control_register = m_memory->ReadByteDirect(CHANNER_1_PERIOD_HIGH_ADDR);
    
    const uint8_t pace = (sweep_register >> 4) & 0b111;
    const uint8_t direction = (sweep_register >> 3) & 0b111;
    const uint8_t individual_step = sweep_register & 0b111;
    const uint8_t wave_duty = (length_and_duty_register >> 5) & 0x3;

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
        pushSampleToMixer(DUTY_CYCLES[2 * 8 + sample_counter], 0);
    }
}