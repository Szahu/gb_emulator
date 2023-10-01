#pragma once
#include "SDL.h"
#include <atomic>

class Gui {
public:
    Gui(uint8_t screen_with, uint8_t screen_height, uint8_t* frame_buffer_ref, bool* button_map);
    ~Gui();

    void RenderFrame(std::atomic<bool>& stop_signal);

private:
    unsigned int gb_screen_width = 160;
    unsigned int gb_screen_height = 144;

    unsigned int m_window_width = 640;
    unsigned int m_window_height = 576;

    SDL_Renderer* m_renderer;
    SDL_Window* m_window;

    SDL_Surface *m_surface;
    SDL_Texture *m_texture;

    uint8_t* m_framebuffer;
    bool* m_button_map;
};