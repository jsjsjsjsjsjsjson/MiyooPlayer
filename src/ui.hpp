#pragma once

#include "file_browser.hpp"
#include "media_tools.hpp"
#include "playlist_manager.hpp"
#include "sdl_platform.hpp"
#include "settings.hpp"

struct PlaybackInfo {
    std::string title;
    std::string metadata_line;
    float pos = 0.0f;
    float duration = 0.0f;
    bool paused = false;
    int volume = 128;
    bool has_video = false;
    bool osd_visible = true;
    PlaybackOrder order = PlaybackOrder::Sequential;
    int audio_queue_kb = 0;
    int video_w = 0;
    int video_h = 0;
};

struct BrowserMenuState {
    bool open = false;
    int selected = 0;
    PlaybackOrder order = PlaybackOrder::Sequential;
    ConversionOptions conversion;
    std::vector<std::string> marked_paths;
    std::string status;
};

struct ConverterState {
    std::string source;
    ConversionOptions options;
    int selected = 0;
    std::string status;
};

struct HomeState {
    bool menu_open = false;
    int menu_selected = 0;
    std::string status;
    bool pending_delete = false;
};

struct SettingsState {
    AppSettings values;
    int selected = 0;
    std::string status;
};

struct PlaylistViewState {
    int playlist_index = 0;
    int selected = 0;
    int scroll = 0;
    bool menu_open = false;
    int menu_selected = 0;
    std::string status;
    bool pending_delete = false;
};

void ui_clear(SDL_Surface* s, uint8_t r, uint8_t g, uint8_t b);
void ui_set_theme(AppTheme theme);
void ui_draw_home(SDL_Surface* s, const PlaylistManager& playlists, const HomeState& home);
void ui_draw_browser(SDL_Surface* s, const FileBrowser& browser, const BrowserMenuState& menu);
void ui_draw_playlist(SDL_Surface* s, const PlaylistManager& playlists, const PlaylistViewState& view);
void ui_draw_converter(SDL_Surface* s, const ConverterState& converter);
void ui_draw_settings(SDL_Surface* s, const SettingsState& settings);
void ui_draw_playback_osd(SDL_Surface* s, const PlaybackInfo& info);
void ui_draw_playback_menu(SDL_Surface* s, const PlaybackInfo& info);
void ui_draw_text(SDL_Surface* s, int x, int y, const std::string& text,
                  uint8_t r, uint8_t g, uint8_t b, int scale = 1);
