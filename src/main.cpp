#include "audio_queue.hpp"
#include "ffmpeg_compat.hpp"
#include "file_browser.hpp"
#include "input.hpp"
#include "media_tools.hpp"
#include "player.hpp"
#include "playlist_manager.hpp"
#include "sdl_platform.hpp"
#include "settings.hpp"
#include "ui.hpp"

enum class AppScreen {
    Home,
    Playlist,
    Browser,
    Converter,
    Settings
};

static void audio_callback(void* userdata, uint8_t* stream, int len) {
    static_cast<AudioQueue*>(userdata)->callback(stream, len);
}

static std::string media_root(int argc, char** argv) {
    if (argc > 1 && argv[1] && argv[1][0]) {
        return argv[1];
    }
    const char* env = std::getenv("MIYOO_PLAYER_DIR");
    if (env && env[0]) {
        return env;
    }
#ifdef MIYOO_NATIVE_TEST
    return ".";
#else
    return "/mnt/videos";
#endif
}

static PlaybackOrder next_order(PlaybackOrder order) {
    switch (order) {
    case PlaybackOrder::Sequential: return PlaybackOrder::ListLoop;
    case PlaybackOrder::ListLoop: return PlaybackOrder::RepeatOne;
    case PlaybackOrder::RepeatOne: return PlaybackOrder::Shuffle;
    case PlaybackOrder::Shuffle:
    default: return PlaybackOrder::Sequential;
    }
}

static bool choose_next_media(FileBrowser& browser, PlaybackOrder order,
                              const std::string& current, std::string& next) {
    switch (order) {
    case PlaybackOrder::RepeatOne:
        next = current;
        return true;
    case PlaybackOrder::Shuffle:
        return browser.random_media(next, current);
    case PlaybackOrder::ListLoop:
        if (browser.next_media_after(current, next)) return true;
        return browser.first_media(next);
    case PlaybackOrder::Sequential:
    default:
        return browser.next_media_after(current, next);
    }
}

static bool choose_prev_media(FileBrowser& browser, PlaybackOrder order,
                              const std::string& current, std::string& prev) {
    if (order == PlaybackOrder::RepeatOne) {
        prev = current;
        return true;
    }
    if (order == PlaybackOrder::Shuffle) {
        return browser.random_media(prev, current);
    }
    return browser.prev_media_before(current, prev);
}

static bool choose_next_playlist_item(const Playlist& playlist, PlaybackOrder order,
                                      int current, int& next) {
    int n = static_cast<int>(playlist.items.size());
    if (n <= 0) return false;
    switch (order) {
    case PlaybackOrder::RepeatOne:
        next = current;
        return true;
    case PlaybackOrder::Shuffle:
        if (n == 1) {
            next = 0;
        } else {
            do {
                next = std::rand() % n;
            } while (next == current);
        }
        return true;
    case PlaybackOrder::ListLoop:
        next = (current + 1) % n;
        return true;
    case PlaybackOrder::Sequential:
    default:
        if (current + 1 < n) {
            next = current + 1;
            return true;
        }
        return false;
    }
}

static bool choose_prev_playlist_item(const Playlist& playlist, PlaybackOrder order,
                                      int current, int& prev) {
    int n = static_cast<int>(playlist.items.size());
    if (n <= 0) return false;
    switch (order) {
    case PlaybackOrder::RepeatOne:
        prev = current;
        return true;
    case PlaybackOrder::Shuffle:
        if (n == 1) {
            prev = 0;
        } else {
            do {
                prev = std::rand() % n;
            } while (prev == current);
        }
        return true;
    case PlaybackOrder::ListLoop:
        prev = (current + n - 1) % n;
        return true;
    case PlaybackOrder::Sequential:
    default:
        if (current > 0) {
            prev = current - 1;
            return true;
        }
        return false;
    }
}

static PlayerResult play_path(AudioQueue& audio, Platform& platform,
                              const std::string& path, PlaybackOrder& order,
                              const AppSettings& settings) {
    Player player(audio);
    player.set_settings(settings);
    player.set_order(order);
    PlayerResult final_result = PlayerResult::Finished;
    if (player.open(path)) {
        bool playing = true;
        while (playing) {
            PlayerResult r = player.tick(platform);
            if (r == PlayerResult::Quit || r == PlayerResult::Back || r == PlayerResult::Finished ||
                r == PlayerResult::Next || r == PlayerResult::Previous) {
                final_result = r;
                playing = false;
            }
        }
        order = player.order();
        player.close();
        audio.clear();
        audio.set_paused(false);
    } else {
        final_result = PlayerResult::Back;
    }
    return final_result;
}

static PlayerResult play_browser_sequence(AudioQueue& audio, Platform& platform,
                                          FileBrowser& browser, std::string current,
                                          PlaybackOrder& order,
                                          const AppSettings& settings) {
    while (true) {
        PlayerResult r = play_path(audio, platform, current, order, settings);
        std::string next;
        if (r == PlayerResult::Next || r == PlayerResult::Finished) {
            if (!choose_next_media(browser, order, current, next)) return PlayerResult::Finished;
        } else if (r == PlayerResult::Previous) {
            if (!choose_prev_media(browser, order, current, next)) return PlayerResult::Finished;
        } else {
            return r;
        }
        current = next;
        browser.select_path(current);
    }
}

static PlayerResult play_playlist(AudioQueue& audio, Platform& platform,
                                  const Playlist& playlist, PlaybackOrder& order,
                                  const AppSettings& settings,
                                  int start_index = 0) {
    if (playlist.items.empty()) return PlayerResult::Finished;
    int index = clamp_int(start_index, 0, static_cast<int>(playlist.items.size()) - 1);
    while (index >= 0 && index < static_cast<int>(playlist.items.size())) {
        PlayerResult r = play_path(audio, platform, playlist.items[index], order, settings);
        int next = -1;
        if (r == PlayerResult::Next || r == PlayerResult::Finished) {
            if (!choose_next_playlist_item(playlist, order, index, next)) return PlayerResult::Finished;
        } else if (r == PlayerResult::Previous) {
            if (!choose_prev_playlist_item(playlist, order, index, next)) return PlayerResult::Finished;
        } else {
            return r;
        }
        index = next;
    }
    return PlayerResult::Finished;
}

static void fix_playlist_view(PlaylistViewState& view, const PlaylistManager& playlists) {
    int count = 0;
    if (const Playlist* p = playlists.get(view.playlist_index)) {
        count = static_cast<int>(p->items.size());
    }
    view.selected = clamp_int(view.selected, 0, count > 0 ? count - 1 : 0);
    const int visible = 10;
    if (view.selected < view.scroll) {
        view.scroll = view.selected;
    }
    if (view.selected >= view.scroll + visible) {
        view.scroll = view.selected - visible + 1;
    }
    view.scroll = std::max(0, view.scroll);
}

int main(int argc, char** argv) {
    ffmpeg_init_once();

    std::string root = media_root(argc, argv);
    AppSettings settings;
    settings_load(root, settings);
    ui_set_theme(settings.theme);

    AudioQueue audio(static_cast<size_t>(clamp_int(settings.audio_buffer_kb, 32, 256)) * 1024);
    Platform platform;
    if (!platform_init(platform, audio_callback, &audio, settings.audio_output_rate, settings.sdl_audio_samples)) {
        return 1;
    }
    std::srand(platform_ticks());

    AppSettings runtime_settings = settings;
    runtime_settings.audio_output_rate = platform.audio_rate;
    FileBrowser browser(root);
    browser.set_show_hidden(settings.show_hidden);
    PlaylistManager playlists(root);
    playlists.load();

    BrowserMenuState browser_menu;
    browser_menu.order = settings.default_order;
    ConverterState converter;
    HomeState home;
    PlaylistViewState playlist_view;
    SettingsState settings_state;
    settings_state.values = settings;
    AppScreen screen = AppScreen::Home;
    bool running = true;
    uint32_t last_draw = 0;

    auto toggle_mark = [&browser_menu](const std::string& path) {
        auto it = std::find(browser_menu.marked_paths.begin(), browser_menu.marked_paths.end(), path);
        if (it == browser_menu.marked_paths.end()) {
            browser_menu.marked_paths.push_back(path);
        } else {
            browser_menu.marked_paths.erase(it);
        }
    };

    while (running) {
        Action a = poll_action();

        if (screen == AppScreen::Home) {
            if (home.menu_open) {
                switch (a) {
                case Action::Quit:
                    running = false;
                    break;
                case Action::Back:
                case Action::R1:
                case Action::Menu:
                case Action::L1:
                    home.menu_open = false;
                    home.pending_delete = false;
                    last_draw = 0;
                    break;
                case Action::Up:
                    home.menu_selected = (home.menu_selected + 5) % 6;
                    home.pending_delete = false;
                    last_draw = 0;
                    break;
                case Action::Down:
                    home.menu_selected = (home.menu_selected + 1) % 6;
                    home.pending_delete = false;
                    last_draw = 0;
                    break;
                case Action::Confirm:
                case Action::Start:
                case Action::Left:
                case Action::Right:
                    if (home.menu_selected == 0) {
                        playlists.create_empty();
                        home.status = "NEW PLAYLIST";
                    } else if (home.menu_selected == 1) {
                        home.status = playlists.create_from_folder(root) >= 0 ? "ROOT SCANNED" : "NO MEDIA";
                    } else if (home.menu_selected == 2) {
                        screen = AppScreen::Browser;
                        home.status.clear();
                    } else if (home.menu_selected == 3) {
                        settings_state.values = settings;
                        settings_state.status.clear();
                        settings_state.selected = 0;
                        home.menu_open = false;
                        screen = AppScreen::Settings;
                    } else if (home.menu_selected == 4) {
                        if (a == Action::Left || a == Action::Right) {
                            home.pending_delete = false;
                        } else if (playlists.count() > 0) {
                            if (home.pending_delete) {
                                playlists.remove_playlist(playlists.selected());
                                home.status = "PLAYLIST DELETED";
                                home.pending_delete = false;
                            } else {
                                home.status = "A CONFIRM DELETE";
                                home.pending_delete = true;
                            }
                        } else {
                            home.status = "NO PLAYLIST";
                        }
                    } else {
                        home.menu_open = false;
                        home.pending_delete = false;
                    }
                    last_draw = 0;
                    break;
                default:
                    break;
                }
            } else {
                switch (a) {
                case Action::Quit:
                    running = false;
                    break;
                case Action::Menu:
                case Action::L1:
                    home.menu_open = true;
                    last_draw = 0;
                    break;
                case Action::Back:
                case Action::R1:
                    screen = AppScreen::Browser;
                    last_draw = 0;
                    break;
                case Action::Left:
                    playlists.left();
                    home.status.clear();
                    last_draw = 0;
                    break;
                case Action::Right:
                    playlists.right();
                    home.status.clear();
                    last_draw = 0;
                    break;
                case Action::CycleOrder:
                    playlists.create_empty();
                    home.status = "NEW PLAYLIST";
                    last_draw = 0;
                    break;
                case Action::SeekBack:
                case Action::L2:
                    screen = AppScreen::Browser;
                    last_draw = 0;
                    break;
                case Action::SeekForward:
                    home.status = playlists.create_from_folder(root) >= 0 ? "ROOT SCANNED" : "NO MEDIA";
                    last_draw = 0;
                    break;
                case Action::Confirm:
                case Action::Start: {
                    const Playlist* p = playlists.selected_playlist();
                    if (p) {
                        playlist_view.playlist_index = playlists.selected();
                        playlist_view.selected = 0;
                        playlist_view.scroll = 0;
                        playlist_view.menu_open = false;
                        playlist_view.status.clear();
                        screen = AppScreen::Playlist;
                        last_draw = 0;
                    }
                    break;
                }
                default:
                    break;
                }
            }
        } else if (screen == AppScreen::Playlist) {
            Playlist* p = playlists.get(playlist_view.playlist_index);
            if (!p) {
                screen = AppScreen::Home;
                last_draw = 0;
            } else if (playlist_view.menu_open) {
                switch (a) {
                case Action::Quit:
                    running = false;
                    break;
                case Action::Back:
                case Action::R1:
                case Action::Menu:
                case Action::L1:
                    playlist_view.menu_open = false;
                    last_draw = 0;
                    break;
                case Action::Up:
                    playlist_view.menu_selected = (playlist_view.menu_selected + 3) % 4;
                    playlist_view.pending_delete = false;
                    last_draw = 0;
                    break;
                case Action::Down:
                    playlist_view.menu_selected = (playlist_view.menu_selected + 1) % 4;
                    playlist_view.pending_delete = false;
                    last_draw = 0;
                    break;
                case Action::Confirm:
                case Action::Start:
                case Action::Left:
                case Action::Right:
                    if (playlist_view.menu_selected == 0 && !p->items.empty()) {
                        PlaybackOrder order = browser_menu.order;
                        PlayerResult r = play_playlist(audio, platform, *p, order, runtime_settings, playlist_view.selected);
                        browser_menu.order = order;
                        if (r == PlayerResult::Quit) running = false;
                        playlist_view.menu_open = false;
                        playlist_view.pending_delete = false;
                    } else if (playlist_view.menu_selected == 1) {
                        if (a != Action::Left && a != Action::Right) {
                            if (playlists.remove_item(playlist_view.playlist_index, playlist_view.selected)) {
                                playlist_view.status = "ITEM REMOVED";
                                fix_playlist_view(playlist_view, playlists);
                            } else {
                                playlist_view.status = "REMOVE FAILED";
                            }
                            p = playlists.get(playlist_view.playlist_index);
                        }
                    } else if (playlist_view.menu_selected == 2) {
                        if (a != Action::Left && a != Action::Right) {
                            if (playlist_view.pending_delete) {
                                playlists.remove_playlist(playlist_view.playlist_index);
                                screen = AppScreen::Home;
                                home.status = "PLAYLIST DELETED";
                                playlist_view.pending_delete = false;
                            } else {
                                playlist_view.status = "A CONFIRM DELETE LIST";
                                playlist_view.pending_delete = true;
                            }
                        }
                    } else {
                        playlist_view.menu_open = false;
                        playlist_view.pending_delete = false;
                    }
                    last_draw = 0;
                    break;
                default:
                    break;
                }
            } else {
                switch (a) {
                case Action::Quit:
                    running = false;
                    break;
                case Action::Back:
                case Action::R1:
                case Action::Left:
                    screen = AppScreen::Home;
                    last_draw = 0;
                    break;
                case Action::Menu:
                case Action::L1:
                    playlist_view.menu_open = true;
                    playlist_view.menu_selected = 0;
                    playlist_view.pending_delete = false;
                    last_draw = 0;
                    break;
                case Action::Up:
                    if (!p->items.empty()) {
                        playlist_view.selected = (playlist_view.selected + static_cast<int>(p->items.size()) - 1) % static_cast<int>(p->items.size());
                        fix_playlist_view(playlist_view, playlists);
                    }
                    last_draw = 0;
                    break;
                case Action::Down:
                    if (!p->items.empty()) {
                        playlist_view.selected = (playlist_view.selected + 1) % static_cast<int>(p->items.size());
                        fix_playlist_view(playlist_view, playlists);
                    }
                    last_draw = 0;
                    break;
                case Action::SeekBack:
                case Action::L2:
                    playlist_view.selected = clamp_int(playlist_view.selected - 8, 0, p->items.empty() ? 0 : static_cast<int>(p->items.size()) - 1);
                    fix_playlist_view(playlist_view, playlists);
                    last_draw = 0;
                    break;
                case Action::SeekForward:
                    playlist_view.selected = clamp_int(playlist_view.selected + 8, 0, p->items.empty() ? 0 : static_cast<int>(p->items.size()) - 1);
                    fix_playlist_view(playlist_view, playlists);
                    last_draw = 0;
                    break;
                case Action::CycleOrder:
                    if (playlists.remove_item(playlist_view.playlist_index, playlist_view.selected)) {
                        playlist_view.status = "ITEM REMOVED";
                        fix_playlist_view(playlist_view, playlists);
                    } else {
                        playlist_view.status = "REMOVE FAILED";
                    }
                    last_draw = 0;
                    break;
                case Action::Confirm:
                case Action::Start:
                case Action::Right:
                    if (!p->items.empty()) {
                        PlaybackOrder order = browser_menu.order;
                        PlayerResult r = play_playlist(audio, platform, *p, order, runtime_settings, playlist_view.selected);
                        browser_menu.order = order;
                        if (r == PlayerResult::Quit) running = false;
                    }
                    last_draw = 0;
                    break;
                default:
                    break;
                }
            }
        } else if (screen == AppScreen::Converter) {
            switch (a) {
            case Action::Quit:
                running = false;
                break;
            case Action::Back:
            case Action::R1:
            case Action::Menu:
            case Action::L1:
                screen = AppScreen::Browser;
                last_draw = 0;
                break;
            case Action::Up:
                converter.selected = (converter.selected + 4) % 5;
                last_draw = 0;
                break;
            case Action::Down:
                converter.selected = (converter.selected + 1) % 5;
                last_draw = 0;
                break;
            case Action::Left:
            case Action::Right:
            case Action::Confirm:
                if (converter.selected == 0) {
                    converter.options.target = convert_next_target(converter.options.target);
                } else if (converter.selected == 1) {
                    converter.options.sample_rate = convert_next_sample_rate(converter.options.sample_rate);
                } else if (converter.selected == 2) {
                    converter.options.bit_depth = convert_next_bit_depth(converter.options.bit_depth);
                } else if (converter.selected == 3) {
                    converter.options.bitrate_kbps = convert_next_bitrate(converter.options.bitrate_kbps);
                } else if (!converter.source.empty()) {
                    MediaToolResult r = media_convert(converter.source, root, converter.options);
                    converter.status = r.ok ? "CONVERT OK" : "CONVERT FAIL";
                    browser_menu.status = converter.status;
                    std::fprintf(stderr, "CONVERT %s %s\n", r.ok ? "OK" : "FAIL",
                                 r.ok ? r.output.c_str() : r.error.c_str());
                }
                last_draw = 0;
                break;
            case Action::Start:
                if (!converter.source.empty()) {
                    MediaToolResult r = media_convert(converter.source, root, converter.options);
                    converter.status = r.ok ? "CONVERT OK" : "CONVERT FAIL";
                    browser_menu.status = converter.status;
                    std::fprintf(stderr, "CONVERT %s %s\n", r.ok ? "OK" : "FAIL",
                                 r.ok ? r.output.c_str() : r.error.c_str());
                }
                last_draw = 0;
                break;
            default:
                break;
            }
        } else if (screen == AppScreen::Settings) {
            switch (a) {
            case Action::Quit:
                running = false;
                break;
            case Action::Back:
            case Action::R1:
            case Action::Menu:
            case Action::L1:
                settings_state.values = settings;
                ui_set_theme(settings.theme);
                screen = AppScreen::Home;
                last_draw = 0;
                break;
            case Action::Up:
                settings_state.selected = (settings_state.selected + 9) % 10;
                last_draw = 0;
                break;
            case Action::Down:
                settings_state.selected = (settings_state.selected + 1) % 10;
                last_draw = 0;
                break;
            case Action::Left:
            case Action::Right:
            case Action::Confirm:
                if (settings_state.selected == 0) {
                    settings_state.values.audio_output_rate = settings_next_audio_rate(settings_state.values.audio_output_rate);
                } else if (settings_state.selected == 1) {
                    settings_state.values.audio_buffer_kb = settings_next_audio_buffer_kb(settings_state.values.audio_buffer_kb);
                } else if (settings_state.selected == 2) {
                    settings_state.values.sdl_audio_samples = settings_next_sdl_audio_samples(settings_state.values.sdl_audio_samples);
                } else if (settings_state.selected == 3) {
                    settings_state.values.audio_ui_ms = settings_next_audio_ui_ms(settings_state.values.audio_ui_ms);
                } else if (settings_state.selected == 4) {
                    settings_state.values.seek_step_sec = settings_next_seek_step_sec(settings_state.values.seek_step_sec);
                } else if (settings_state.selected == 5) {
                    settings_state.values.osd_ms = settings_next_osd_ms(settings_state.values.osd_ms);
                } else if (settings_state.selected == 6) {
                    settings_state.values.show_hidden = !settings_state.values.show_hidden;
                } else if (settings_state.selected == 7) {
                    settings_state.values.default_order = settings_next_order(settings_state.values.default_order);
                } else if (settings_state.selected == 8) {
                    settings_state.values.theme = settings_next_theme(settings_state.values.theme);
                    ui_set_theme(settings_state.values.theme);
                } else {
                    bool ok = settings_save(root, settings_state.values);
                    settings = settings_state.values;
                    runtime_settings = settings;
                    runtime_settings.audio_output_rate = platform.audio_rate;
                    browser.set_show_hidden(settings.show_hidden);
                    browser_menu.order = settings.default_order;
                    ui_set_theme(settings.theme);
                    settings_state.status = ok ? "SAVED AUDIO SETTINGS NEXT START" : "SAVE FAILED";
                }
                last_draw = 0;
                break;
            case Action::Start: {
                bool ok = settings_save(root, settings_state.values);
                settings = settings_state.values;
                runtime_settings = settings;
                runtime_settings.audio_output_rate = platform.audio_rate;
                browser.set_show_hidden(settings.show_hidden);
                browser_menu.order = settings.default_order;
                ui_set_theme(settings.theme);
                settings_state.status = ok ? "SAVED AUDIO SETTINGS NEXT START" : "SAVE FAILED";
                last_draw = 0;
                break;
            }
            default:
                break;
            }
        } else {
            if (browser_menu.open) {
                switch (a) {
                case Action::Quit:
                    running = false;
                    break;
                case Action::Back:
                case Action::R1:
                case Action::Menu:
                case Action::L1:
                    browser_menu.open = false;
                    last_draw = 0;
                    break;
                case Action::Up:
                    browser_menu.selected = (browser_menu.selected + 11) % 12;
                    last_draw = 0;
                    break;
                case Action::Down:
                    browser_menu.selected = (browser_menu.selected + 1) % 12;
                    last_draw = 0;
                    break;
                case Action::CycleOrder:
                    browser_menu.order = next_order(browser_menu.order);
                    last_draw = 0;
                    break;
                case Action::Left:
                case Action::Right:
                case Action::Confirm:
                case Action::Start: {
                    if (browser_menu.selected == 0) {
                        browser_menu.open = false;
                        screen = AppScreen::Home;
                    } else if (browser_menu.selected == 1) {
                        SortMode next = SortMode::Name;
                        if (browser.sort_mode() == SortMode::Name) next = SortMode::Type;
                        else if (browser.sort_mode() == SortMode::Type) next = SortMode::Time;
                        browser.set_sort_mode(next);
                    } else if (browser_menu.selected == 2) {
                        browser.set_show_hidden(!browser.show_hidden());
                    } else if (browser_menu.selected == 3) {
                        browser_menu.order = next_order(browser_menu.order);
                    } else if (browser_menu.selected == 4) {
                        if (!browser_menu.marked_paths.empty()) {
                            Playlist temp = playlists.temporary_from_files(browser_menu.marked_paths);
                            PlayerResult r = play_playlist(audio, platform, temp, browser_menu.order, runtime_settings);
                            if (r == PlayerResult::Quit) running = false;
                            browser_menu.status = "OPENED MARKS";
                        } else {
                            browser_menu.status = "NO MARKS";
                        }
                    } else if (browser_menu.selected == 5) {
                        const FileEntry* e = browser.selected_entry();
                        std::string folder = (e && e->is_dir && e->name != "..") ? e->path : browser.cwd();
                        Playlist temp = playlists.temporary_from_folder(folder);
                        PlayerResult r = play_playlist(audio, platform, temp, browser_menu.order, runtime_settings);
                        if (r == PlayerResult::Quit) running = false;
                        browser_menu.status = temp.items.empty() ? "NO MEDIA" : "OPENED DIR";
                    } else if (browser_menu.selected == 6) {
                        const FileEntry* e = browser.selected_entry();
                        if (e && !e->is_dir && playlists.add_file_to_selected(e->path)) {
                            browser_menu.status = "ADDED FILE";
                        } else {
                            browser_menu.status = "ADD FAILED";
                        }
                    } else if (browser_menu.selected == 7) {
                        browser_menu.status = playlists.create_from_folder(browser.cwd()) >= 0 ? "LIST MADE" : "NO MEDIA";
                    } else if (browser_menu.selected == 8) {
                        const FileEntry* e = browser.selected_entry();
                        if (e && !e->is_dir) {
                            converter.source = e->path;
                            converter.options = browser_menu.conversion;
                            converter.status.clear();
                            converter.selected = 0;
                            browser_menu.open = false;
                            screen = AppScreen::Converter;
                        } else {
                            browser_menu.status = "SELECT FILE";
                        }
                    } else if (browser_menu.selected == 9) {
                        playlists.create_empty();
                        browser_menu.status = "NEW LIST";
                    } else if (browser_menu.selected == 10) {
                        browser.refresh();
                        browser_menu.status = "REFRESHED";
                    } else if (browser_menu.selected == 11) {
                        browser_menu.open = false;
                    }
                    last_draw = 0;
                    break;
                }
                default:
                    break;
                }
            } else {
                switch (a) {
                case Action::Quit:
                    running = false;
                    break;
                case Action::Menu:
                case Action::L1:
                    browser_menu.open = true;
                    last_draw = 0;
                    break;
                case Action::CycleOrder:
                    if (const FileEntry* e = browser.selected_entry()) {
                        if (!e->is_dir) {
                            toggle_mark(e->path);
                            browser_menu.status = "MARK TOGGLED";
                        }
                    }
                    last_draw = 0;
                    break;
                case Action::Up:
                    browser.up();
                    last_draw = 0;
                    break;
                case Action::Down:
                    browser.down();
                    last_draw = 0;
                    break;
                case Action::SeekBack:
                case Action::L2:
                    browser.page_up();
                    last_draw = 0;
                    break;
                case Action::SeekForward:
                    browser.page_down();
                    last_draw = 0;
                    break;
                case Action::Left:
                case Action::Back:
                case Action::R1:
                    if (browser.cwd() == root) {
                        screen = AppScreen::Home;
                    } else {
                        browser.go_parent();
                    }
                    last_draw = 0;
                    break;
                case Action::Right:
                case Action::Confirm:
                case Action::Start: {
                    std::string path;
                    if (browser.enter(path)) {
                        PlayerResult r = play_browser_sequence(audio, platform, browser, path, browser_menu.order, runtime_settings);
                        if (r == PlayerResult::Quit) running = false;
                        browser.refresh();
                        browser.select_path(path);
                    }
                    last_draw = 0;
                    break;
                }
                default:
                    break;
                }
            }
        }

        if (platform_ticks() - last_draw > 80) {
            if (screen == AppScreen::Home) {
                ui_draw_home(platform.screen, playlists, home);
            } else if (screen == AppScreen::Converter) {
                ui_draw_converter(platform.screen, converter);
            } else if (screen == AppScreen::Settings) {
                ui_draw_settings(platform.screen, settings_state);
            } else if (screen == AppScreen::Playlist) {
                ui_draw_playlist(platform.screen, playlists, playlist_view);
            } else {
                ui_draw_browser(platform.screen, browser, browser_menu);
            }
            platform_flip(platform);
            last_draw = platform_ticks();
        }
        platform_sleep_ms(8);
    }

    platform_shutdown(platform);
    return 0;
}
