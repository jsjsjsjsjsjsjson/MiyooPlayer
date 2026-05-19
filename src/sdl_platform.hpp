#pragma once

#ifdef MIYOO_USE_SDL2
#include <SDL2/SDL.h>
#else
#include <SDL/SDL.h>
#endif

#include "common.hpp"

struct Platform {
#ifdef MIYOO_USE_SDL2
    SDL_Window* window = nullptr;
#endif
    SDL_Surface* screen = nullptr;
    bool audio_open = false;
    int audio_rate = 44100;
    int audio_samples = 1024;
};

using AudioCallback = void (*)(void*, uint8_t*, int);

bool platform_init(Platform& p, AudioCallback cb, void* userdata,
                   int audio_rate = 44100, int audio_samples = 1024);
void platform_shutdown(Platform& p);
void platform_flip(Platform& p);
void platform_sleep_ms(unsigned ms);
uint32_t platform_ticks();
void platform_pause_audio(bool pause);
void platform_lock_screen(Platform& p);
void platform_unlock_screen(Platform& p);
