#include "sdl_platform.hpp"

#include <ctime>

float now_seconds() {
    return static_cast<float>(SDL_GetTicks()) / 1000.0f;
}

bool platform_init(Platform& p, AudioCallback cb, void* userdata, int audio_rate, int audio_samples) {
    setenv("SDL_NOMOUSE", "1", 1);
    setenv("SDL_MOUSEDRV", "dummy", 1);
#ifndef MIYOO_USE_SDL2
    setenv("SDL_VIDEODRIVER", "fbcon", 0);
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK) < 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

#ifdef MIYOO_USE_SDL2
    p.window = SDL_CreateWindow("miyoo-player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    if (!p.window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    p.screen = SDL_GetWindowSurface(p.window);
#else
    SDL_ShowCursor(SDL_DISABLE);
    p.screen = SDL_SetVideoMode(SCREEN_W, SCREEN_H, 16, SDL_SWSURFACE);
#endif
    if (!p.screen) {
        std::fprintf(stderr, "SDL video setup failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_AudioSpec want;
    std::memset(&want, 0, sizeof(want));
    want.freq = clamp_int(audio_rate, 8000, 96000);
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = static_cast<Uint16>(clamp_int(audio_samples, 256, 4096));
    want.callback = cb;
    want.userdata = userdata;

    SDL_AudioSpec got;
    if (SDL_OpenAudio(&want, &got) < 0) {
        std::fprintf(stderr, "SDL_OpenAudio failed: %s\n", SDL_GetError());
        return false;
    }
    p.audio_open = true;
    p.audio_rate = got.freq > 0 ? got.freq : want.freq;
    p.audio_samples = got.samples > 0 ? got.samples : want.samples;
    SDL_PauseAudio(0);
    return true;
}

void platform_shutdown(Platform& p) {
    if (p.audio_open) {
        SDL_CloseAudio();
        p.audio_open = false;
    }
#ifdef MIYOO_USE_SDL2
    if (p.window) {
        SDL_DestroyWindow(p.window);
        p.window = nullptr;
        p.screen = nullptr;
    }
#endif
    SDL_Quit();
}

void platform_flip(Platform& p) {
#ifdef MIYOO_USE_SDL2
    if (p.window) {
        SDL_UpdateWindowSurface(p.window);
    }
#else
    if (p.screen) {
        SDL_Flip(p.screen);
    }
#endif
}

void platform_sleep_ms(unsigned ms) {
    SDL_Delay(ms);
}

uint32_t platform_ticks() {
    return SDL_GetTicks();
}

void platform_pause_audio(bool pause) {
    SDL_PauseAudio(pause ? 1 : 0);
}

void platform_lock_screen(Platform& p) {
    if (p.screen && SDL_MUSTLOCK(p.screen)) {
        SDL_LockSurface(p.screen);
    }
}

void platform_unlock_screen(Platform& p) {
    if (p.screen && SDL_MUSTLOCK(p.screen)) {
        SDL_UnlockSurface(p.screen);
    }
}
