#pragma once

#include <SDL.h>
#include <SDL_audio.h>

class AudioLayer {
public:
    AudioLayer();
    ~AudioLayer();

    void PushSamples(void* ptr, size_t len);
    void PushSample(float sample);

    inline unsigned int GetSampleRate() const { return INPUT_SAMPLE_RATE; }

private:

    SDL_AudioDeviceID m_device_id = 0;
    SDL_AudioSpec m_device_spec; 
    SDL_AudioStream* m_audio_stream = nullptr;

    static constexpr unsigned int INPUT_SAMPLE_RATE = 32768;

    static constexpr SDL_AudioSpec INPUT_AUDIO_SPEC = {
        SDL_AUDIO_S16,
        2,
        INPUT_SAMPLE_RATE
    };

};