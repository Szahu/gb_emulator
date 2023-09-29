#include "gui.hpp"

Gui::Gui(uint8_t screen_width, uint8_t screen_height, uint8_t* frame_buffer_ref) {

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

void Gui::RenderFrame(std::atomic<bool>& stop_signal) {
    // Render SDL stuff
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        stop_signal = event.type == SDL_EVENT_QUIT;
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
    SDL_Quit();
}