#include "gui.hpp"
#include <unordered_map>

Gui::Gui(uint8_t screen_width, uint8_t screen_height, uint8_t* frame_buffer_ref, bool* button_map) {

    m_button_map = button_map;
    m_framebuffer = frame_buffer_ref;

    gb_screen_width = screen_width;
    gb_screen_height = screen_height;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    SDL_CreateWindowAndRenderer(gb_screen_width, gb_screen_height, 0, &m_window, &m_renderer);
    SDL_SetWindowSize(m_window, m_window_width, m_window_height);
    SDL_SetWindowResizable(m_window, SDL_TRUE);

    SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    m_texture = SDL_CreateTexture(m_renderer, SDL_PixelFormatEnum::SDL_PIXELFORMAT_RGB24, SDL_TextureAccess::SDL_TEXTUREACCESS_STREAMING, gb_screen_width, gb_screen_height);
}

void Gui::RenderFrame(bool& stop_signal) {
    
    std::unordered_map<SDL_Keycode, uint8_t> key_map = {
        {SDLK_RIGHT, 0},
        {SDLK_LEFT, 1},
        {SDLK_UP, 2},
        {SDLK_DOWN, 3},
        {SDLK_a, 4},
        {SDLK_s, 5},
        {SDLK_d, 6},
        {SDLK_f, 7},
    };
    
    // Render SDL stuff
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        stop_signal = event.type == SDL_EVENT_QUIT;
        if (event.type == SDL_EVENT_KEY_DOWN) {
            const SDL_Keycode code = event.key.keysym.sym;
            auto itr = key_map.find(code);
            if(itr != key_map.end()) {
                m_button_map[itr->second] = true;
            }

        } else if (event.type == SDL_EVENT_KEY_UP) {
            const SDL_Keycode code = event.key.keysym.sym;
            auto itr = key_map.find(code);
            if(itr != key_map.end()) {
                m_button_map[itr->second] = false;
            }
        }
    }
    SDL_SetRenderDrawColor(m_renderer, 255, 0, 0, 255);
    SDL_RenderClear(m_renderer);

    SDL_UpdateTexture(m_texture, NULL, m_framebuffer, 160 * 3);

    SDL_RenderTexture(m_renderer, m_texture, NULL, NULL);
    SDL_RenderPresent(m_renderer);
}

Gui::~Gui() {

    SDL_DestroyTexture(m_texture);

    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);
}