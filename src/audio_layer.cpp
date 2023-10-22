#include "audio_layer.hpp"
#include "common.hpp"

AudioLayer::AudioLayer() {

    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        printf("%s\n", SDL_GetError());
        ASSERT(false);
    }

    m_device_id = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, NULL);
    if (m_device_id == 0) {
        printf("%s\n", SDL_GetError());
        ASSERT(false);
    }

    if (SDL_GetAudioDeviceFormat(m_device_id, &m_device_spec, NULL) != 0) {
        printf("%s\n", SDL_GetError());
        ASSERT(false);
    }

    m_audio_stream = SDL_CreateAudioStream(&INPUT_AUDIO_SPEC, &m_device_spec);
    if (!m_audio_stream) {
        printf("%s\n", SDL_GetError());
        ASSERT(false);
    }

    if (SDL_BindAudioStream(m_device_id, m_audio_stream) != 0) {
        printf("%s\n", SDL_GetError());
        ASSERT(false);
    }

}

AudioLayer::~AudioLayer() {
    
    SDL_CloseAudioDevice(m_device_id);
    SDL_DestroyAudioStream(m_audio_stream);
    SDL_Quit();

}

void AudioLayer::PushSamples(void* ptr, size_t len) {

    if (SDL_PutAudioStreamData(m_audio_stream, ptr, len) != 0) {
        printf("%s\n", SDL_GetError());
        ASSERT(false);
    }

}


void AudioLayer::PushSample(float sample) {

    if (SDL_PutAudioStreamData(m_audio_stream, &sample, sizeof(sample)) != 0) {
        printf("%s\n", SDL_GetError());
        ASSERT(false);
    }

}