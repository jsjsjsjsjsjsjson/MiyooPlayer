#pragma once

#include "common.hpp"

enum class AppTheme {
    Dark,
    Amber,
    Cyan,
    Light
};

struct AppSettings {
    int audio_output_rate = 44100;
    int audio_buffer_kb = 96;
    int sdl_audio_samples = 1024;
    int audio_ui_ms = 250;
    int seek_step_sec = 10;
    int osd_ms = 3000;
    bool show_hidden = false;
    PlaybackOrder default_order = PlaybackOrder::Sequential;
    AppTheme theme = AppTheme::Dark;
};

std::string settings_path_for_root(const std::string& root);
bool settings_load(const std::string& root, AppSettings& settings);
bool settings_save(const std::string& root, const AppSettings& settings);

int settings_next_audio_rate(int value);
int settings_next_audio_buffer_kb(int value);
int settings_next_sdl_audio_samples(int value);
int settings_next_audio_ui_ms(int value);
int settings_next_seek_step_sec(int value);
int settings_next_osd_ms(int value);
PlaybackOrder settings_next_order(PlaybackOrder order);
AppTheme settings_next_theme(AppTheme theme);
const char* settings_order_name(PlaybackOrder order);
const char* settings_theme_name(AppTheme theme);
