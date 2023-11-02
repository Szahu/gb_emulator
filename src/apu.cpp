#include "apu.hpp"
#include "common.hpp"

Apu::Apu(Memory* memory_ref, AudioLayer* audio_layer_ref):
 m_memory(memory_ref), m_audio_layer(audio_layer_ref) {

    m_memory->AddToWriteAddressMapper(CHANNEL_1_PERIOD_HIGH_ADDR, [&] (uint8_t byte) {
        m_channel_1_trigger = bitGet(byte, 7);
        m_memory->WriteByteDirect(CHANNEL_1_PERIOD_HIGH_ADDR, byte);
    });

    m_memory->AddToWriteAddressMapper(CHANNEL_2_PERIOD_HIGH_ADDR, [&] (uint8_t byte) {
        m_channel_2_trigger = bitGet(byte, 7);
        m_memory->WriteByteDirect(CHANNEL_2_PERIOD_HIGH_ADDR, byte);
    });

    m_memory->AddToWriteAddressMapper(CHANNEL_3_PERIOD_HIGH_CONTROL_ADDR, [&] (uint8_t byte) {
        m_channel_3_trigger = bitGet(byte, 7);
        m_memory->WriteByteDirect(CHANNEL_3_PERIOD_HIGH_CONTROL_ADDR, byte);
    });

    m_memory->AddToWriteAddressMapper(CHANNEL_4_CONTROL_ADDR, [&] (uint8_t byte) {
        m_channel_4_trigger = bitGet(byte, 7);
        m_memory->WriteByteDirect(CHANNEL_4_CONTROL_ADDR, byte);
    });

    m_memory->AddToWriteAddressMapper(CHANNEL_4_LENGTH_TIMER_ADDR, [&] (uint8_t byte) {
        m_channel_4_length_reload = true;
        m_memory->WriteByteDirect(CHANNEL_4_LENGTH_TIMER_ADDR, byte);
    });

}

void Apu::ApuStep(unsigned int m_cycle_count) {
    static unsigned int cycle_pool = 0;
    cycle_pool += m_cycle_count;
    static const unsigned int CYCLES_PER_SAMPLE = (1 << 20) / m_audio_layer->GetSampleRate();
    const bool produce_sample = cycle_pool >= CYCLES_PER_SAMPLE;

    const uint8_t master_control = m_memory->ReadByteDirect(AUDIO_MASTER_CONTROLL_ADDR);

    if (!bitGet(master_control, 7)) {
        cycle_pool = 0;
        return;
    }

    std::memset(m_mixer_buffer, 0x0, sizeof(m_mixer_buffer));

    // handleChannel1(m_cycle_count, produce_sample);
    // handleChannel2(m_cycle_count, produce_sample);
    // handleChannel3(m_cycle_count, produce_sample);
    handleChannel4(m_cycle_count, produce_sample);

    if (produce_sample) {
        cycle_pool -= CYCLES_PER_SAMPLE;

        sample_t final_sample = {0, 0};
        for (int i = 0;i < 4; ++i) {
            final_sample.left += m_mixer_buffer[2 * i];
            final_sample.right += m_mixer_buffer[2 * i + 1];
        }
        final_sample.left *= m_volume;
        final_sample.right *= m_volume;

        m_audio_layer->PushSamples(&final_sample, sizeof(float) * 2);
    }

}

void Apu::pushSampleToMixer(float sampleL, float sampleR, unsigned int channel) {
    m_mixer_buffer[2 * channel] = sampleL;
    m_mixer_buffer[2 * channel + 1] = sampleR;
}

void Apu::handleChannel1(unsigned int m_cycle_count, bool produce_sample) {
    static unsigned int timer_cycle_pool = 0;
    static bool is_active = false;
    static unsigned int length_timer = 0;
    static unsigned int pace_cycle_pool = 0;
    static unsigned int envelope_cycle_pool = 0;
    static constexpr unsigned int CYCLES_PER_APU_DIV_TICK = (1 << 20) / 256;
    static constexpr unsigned int CYCLES_PER_SWEEP_ITERATION = (1 << 20) / 128;
    static constexpr unsigned int CYCLES_PER_ENV_ITERATION = (1 << 20) / 64;

    const uint8_t length_and_duty_register = m_memory->ReadByteDirect(CHANNEL_1_LENGTH_AND_DUTY_ADDR);
    const uint8_t volume_and_envelope_register = m_memory->ReadByteDirect(CHANNEL_1_VOLUME_AND_ENVELOPE_ADDR);
    const uint8_t period_low_register = m_memory->ReadByteDirect(CHANNEL_1_PERIOD_LOW_ADDR);
    const uint8_t period_high_and_control_register = m_memory->ReadByteDirect(CHANNEL_1_PERIOD_HIGH_ADDR);
    
    static uint8_t wave_duty = (length_and_duty_register >> 6) & 0x3;

    static uint8_t pace = (m_memory->ReadByteDirect(CHANNEL_1_SWEEP_ADDR) >> 4) & 0x7;
    static uint8_t direction = (m_memory->ReadByteDirect(CHANNEL_1_SWEEP_ADDR) >> 3) & 0x1;
    static uint8_t indiviual_step = m_memory->ReadByteDirect(CHANNEL_1_SWEEP_ADDR) & 0x7;

    uint8_t audio_master_control = m_memory->ReadByteDirect(AUDIO_MASTER_CONTROLL_ADDR);
    bitSet(audio_master_control, 0, is_active);
    m_memory->WriteByteDirect(AUDIO_MASTER_CONTROLL_ADDR, audio_master_control);

    static uint16_t period_value = 0;
    static uint8_t current_volume = 0;
    static uint8_t current_env_dir = 0;
    static uint8_t current_env_pace = 0;

    if (m_channel_1_trigger) {
        // trigger the channel
        is_active = true;
        m_channel_1_trigger = false;
        length_timer = length_and_duty_register & 0b111111;
        wave_duty = (length_and_duty_register >> 6) & 0x3;

        pace = (m_memory->ReadByteDirect(CHANNEL_1_SWEEP_ADDR) >> 4) & 0x7;
        direction = (m_memory->ReadByteDirect(CHANNEL_1_SWEEP_ADDR) >> 3) & 0x1;
        indiviual_step = m_memory->ReadByteDirect(CHANNEL_1_SWEEP_ADDR) & 0x7;
        period_value = period_low_register | ((period_high_and_control_register & 0b111) << 8);

        current_volume = volume_and_envelope_register >> 4;
        current_env_dir = (volume_and_envelope_register >> 3) & 0x1;
        current_env_pace = volume_and_envelope_register & 0x7;
    }

    if (bitGet(period_high_and_control_register, 6) && length_timer == 64) {
        // timer runs out 
        is_active = false;
    }

    if ((volume_and_envelope_register >> 3) == 0) {
        is_active = false;
    } 

    if (!is_active) {
        timer_cycle_pool = 0;
        pace_cycle_pool = 0;
        envelope_cycle_pool = 0;
        return;
    }

    timer_cycle_pool += m_cycle_count;
    if (timer_cycle_pool >= CYCLES_PER_APU_DIV_TICK) {
        length_timer++;
        timer_cycle_pool -= CYCLES_PER_APU_DIV_TICK;
    }

    pace_cycle_pool += m_cycle_count;

    if (pace != 0 && pace_cycle_pool >= CYCLES_PER_SWEEP_ITERATION) {
        if (period_value >= 0x7FF) {
            is_active = false;
        }
        pace_cycle_pool = 0;
        pace = (m_memory->ReadByteDirect(CHANNEL_1_SWEEP_ADDR) >> 4) & 0x7;
        period_value = period_value + (period_value / (1 << indiviual_step)) * (1 - 2 * direction);
        m_memory->WriteByteDirect(CHANNEL_1_PERIOD_LOW_ADDR, period_value & 0xFF);
        uint8_t new_period_high_and_control_register = period_high_and_control_register & 0b11111000;
        new_period_high_and_control_register |= (period_value >> 8) & 0x7;
        m_memory->WriteByteDirect(CHANNEL_1_PERIOD_HIGH_ADDR, new_period_high_and_control_register);
    }

    envelope_cycle_pool += m_cycle_count;
    if (current_env_pace != 0 && envelope_cycle_pool >= current_env_pace * CYCLES_PER_ENV_ITERATION) {
        envelope_cycle_pool -= current_env_pace * CYCLES_PER_ENV_ITERATION;
        // TODO FIX THIS
        // if (current_volume != 0 && current_volume != 0xFF) {
        //     current_volume -= 1 - 2 * current_env_dir; 
        // }
    }

    static uint16_t period_clock = 2048;
    period_clock += m_cycle_count;

    static unsigned int sample_counter = 0;

    if (period_clock >= 2048) {
        if (sample_counter == 7) {
            period_value = period_low_register | ((period_high_and_control_register & 0b111) << 8);
        }
        period_clock = period_value;
        sample_counter = (sample_counter + 1) % 8;
    }

    if (produce_sample) {
        float sample = static_cast<float>(DUTY_CYCLES[wave_duty * 8 + sample_counter]);
        sample *= static_cast<float>(current_volume);
        pushSampleToMixer(sample, sample, 0);
    }
}

void Apu::handleChannel2(unsigned int m_cycle_count, bool produce_sample) {
    static unsigned int timer_cycle_pool = 0;
    static bool is_active = false;
    static unsigned int length_timer = 0;
    static unsigned int envelope_cycle_pool = 0;
    static constexpr unsigned int CYCLES_PER_APU_DIV_TICK = (1 << 20) / 256;
    static constexpr unsigned int CYCLES_PER_ENV_ITERATION = (1 << 20) / 64;

    const uint8_t length_and_duty_register = m_memory->ReadByteDirect(CHANNEL_2_LENGTH_AND_DUTY_ADDR);
    const uint8_t volume_and_envelope_register = m_memory->ReadByteDirect(CHANNEL_2_VOLUME_AND_ENVELOPE_ADDR);
    const uint8_t period_low_register = m_memory->ReadByteDirect(CHANNEL_2_PERIOD_LOW_ADDR);
    const uint8_t period_high_and_control_register = m_memory->ReadByteDirect(CHANNEL_2_PERIOD_HIGH_ADDR);
    
    static uint8_t wave_duty = (length_and_duty_register >> 6) & 0x3;

    uint8_t audio_master_control = m_memory->ReadByteDirect(AUDIO_MASTER_CONTROLL_ADDR);
    bitSet(audio_master_control, 1, is_active);
    m_memory->WriteByteDirect(AUDIO_MASTER_CONTROLL_ADDR, audio_master_control);

    static uint16_t period_value = 0;
    static uint8_t current_volume = 0;
    static uint8_t current_env_dir = 0;
    static uint8_t current_env_pace = 0;

    if (m_channel_2_trigger) {
        // trigger the channel
        is_active = true;
        m_channel_2_trigger = false;
        length_timer = length_and_duty_register & 0b111111;
        wave_duty = (length_and_duty_register >> 6) & 0x3;

        period_value = period_low_register | ((period_high_and_control_register & 0b111) << 8);

        current_volume = volume_and_envelope_register >> 4;
        current_env_dir = (volume_and_envelope_register >> 3) & 0x1;
        current_env_pace = volume_and_envelope_register & 0x7;
    }

    if (bitGet(period_high_and_control_register, 6) && length_timer == 64) {
        // timer runs out 
        is_active = false;
    }

    if ((volume_and_envelope_register >> 3) == 0) {
        is_active = false;
    } 

    if (!is_active) {
        timer_cycle_pool = 0;
        envelope_cycle_pool = 0;
        return;
    }

    timer_cycle_pool += m_cycle_count;
    if (timer_cycle_pool >= CYCLES_PER_APU_DIV_TICK) {
        length_timer++;
        timer_cycle_pool -= CYCLES_PER_APU_DIV_TICK;
    }

    envelope_cycle_pool += m_cycle_count;
    if (current_env_pace != 0 && envelope_cycle_pool >= current_env_pace * CYCLES_PER_ENV_ITERATION) {
        envelope_cycle_pool -= current_env_pace * CYCLES_PER_ENV_ITERATION;
        // TODO FIX THIS AS WELL
        // if (current_volume != 0 && current_volume != 0xFF) {
        //     current_volume -= 1 - 2 * current_env_dir; 
        // }
    }

    static uint16_t period_clock = 2048;
    period_clock += m_cycle_count;

    static unsigned int sample_counter = 0;

    if (period_clock >= 2048) {
        if (sample_counter == 7) {
            period_value = period_low_register | ((period_high_and_control_register & 0b111) << 8);
        }
        period_clock = period_value;
        sample_counter = (sample_counter + 1) % 8;
    }

    if (produce_sample) {
        float sample = static_cast<float>(DUTY_CYCLES[wave_duty * 8 + sample_counter]);
        sample *= static_cast<float>(current_volume);
        pushSampleToMixer(sample, sample, 1);
    }
}

void Apu::handleChannel3(unsigned int m_cycle_count, bool produce_sample) {
    static bool is_active = false;

    const uint8_t period_low_register = m_memory->ReadByteDirect(CHANNEL_3_PERIOD_LOW_ADDR);
    const uint8_t period_high_control_register = m_memory->ReadByteDirect(CHANNEL_3_PERIOD_HIGH_CONTROL_ADDR);
    
    const unsigned int period_value = period_low_register | ((period_high_control_register & 0x7) << 8);

    static unsigned int period = 2048;
    static unsigned int sample_counter = 0;
    static uint8_t current_volume = 0;

    uint8_t audio_master_control = m_memory->ReadByteDirect(AUDIO_MASTER_CONTROLL_ADDR);
    bitSet(audio_master_control, 3, is_active);
    m_memory->WriteByteDirect(AUDIO_MASTER_CONTROLL_ADDR, audio_master_control);

    if (m_channel_3_trigger) {
        is_active = true;
        m_channel_3_trigger = false;
        period = period_value;
        current_volume = (m_memory->ReadByte(CHANNEL_3_OUTPUT_LEVEL_ADDR) >> 5) & 0x3;
        sample_counter = 0;
    }

    is_active = bitGet(m_memory->ReadByteDirect(CHANNEL_3_DAC_ENABLE_ADDR), 7);

    if (!is_active) {
        return;
    }

    period += 2 * m_cycle_count;

    if (period >= 2048) {
        period = period_value;
        sample_counter = (sample_counter + 1) % 32;
    }

    if (produce_sample) {
        uint8_t sample = m_memory->ReadByteDirect(CHANNEL_3_WAVE_RAM_ADDR + (sample_counter / 2));
        if (sample_counter & 0b1) {
            sample = sample & 0x0F;
        } else {
            sample = sample >> 4;
        }
        if (current_volume == 0) {
            sample = 0;
        } else {
            sample = sample >> (current_volume - 1);
        }
        pushSampleToMixer(static_cast<float>(sample), static_cast<float>(sample), 2);
    }
}

void Apu::handleChannel4(unsigned int m_cycle_count, bool produce_sample) {
    const uint8_t initial_length_timer = m_memory->ReadByteDirect(CHANNEL_4_LENGTH_TIMER_ADDR);
    const uint8_t volume_and_envelope = m_memory->ReadByteDirect(CHANNEL_4_VOLUME_AND_ENVELOPE_ADDR);
    const uint8_t frequency_and_randomness = m_memory->ReadByteDirect(CHANNEL_4_FREQUENCY_AND_RANDOMNESS_ADDR);
    const uint8_t control_register = m_memory->ReadByteDirect(CHANNEL_4_CONTROL_ADDR);

    static constexpr uint8_t divisor_table[] = {
        8, 16, 32, 48, 64, 80, 96, 112
    };
    static constexpr unsigned int CYCLES_PER_LENGTH_TIMER_TICK = (1 << 20) / 256;

    static bool is_active = false;

    static unsigned int clock = 0;
    static uint8_t current_LFSR_width = 0;

    static unsigned int cycles_per_clock_tick = 0;
    static uint16_t LFSR = 0;
    static uint8_t current_volume = 0;

    static unsigned int length_counter = 0;
    static unsigned int length_timer_pool = 0; 

    uint8_t audio_master_control = m_memory->ReadByteDirect(AUDIO_MASTER_CONTROLL_ADDR);
    bitSet(audio_master_control, 3, is_active);
    m_memory->WriteByteDirect(AUDIO_MASTER_CONTROLL_ADDR, audio_master_control);

    if (m_channel_4_trigger) {
        m_channel_4_trigger = false;
        is_active = true;
        clock = 0;

        uint8_t divider = frequency_and_randomness & 0x7;
        uint8_t shift = frequency_and_randomness >> 4;
        current_LFSR_width = (frequency_and_randomness >> 3) & 0x1; 
        if (divider == 0) {
            divider = 1;
            if (shift != 0) shift--;
        }
        unsigned int frequency = (262144 / (divider * (1 << shift)));
        cycles_per_clock_tick = (1 << 20) / frequency;
        LFSR = 0xFFFF;

        current_volume = volume_and_envelope >> 4;

        length_timer_pool = 0;
        length_counter = initial_length_timer;
    }
    
    if (!is_active) {
        return;
    }

    if (m_channel_4_length_reload) {
        m_channel_4_length_reload = false;
        length_counter = initial_length_timer;
    }

    length_timer_pool += m_cycle_count;
    if (length_timer_pool >= CYCLES_PER_LENGTH_TIMER_TICK) {
        length_timer_pool -= CYCLES_PER_LENGTH_TIMER_TICK;
        length_counter++;
        if (length_counter == 64 && bitGet(control_register, 6)) {
            is_active = false;
        }
    }

    clock += m_cycle_count;
    if (clock >= cycles_per_clock_tick) {
        clock -= cycles_per_clock_tick;
        bool xor_res = bitGet(LFSR, 0) ^ bitGet(LFSR, 1);
        LFSR = LFSR >> 1;
        bitSet(LFSR, 14, xor_res);
        if (current_LFSR_width) {
            bitSet(LFSR, 6, xor_res);
        }
    }

    if (produce_sample) {
        uint8_t sample = (!bitGet(LFSR, 0)) * current_volume;
        pushSampleToMixer(static_cast<float>(sample), static_cast<float>(sample), 3);
    }

}