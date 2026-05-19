#include "settings.hpp"

#include <cctype>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

static std::string join_path_settings(const std::string& a, const std::string& b) {
    if (a.empty() || a == "/") {
        return "/" + b;
    }
    return a + "/" + b;
}

static bool is_dir_path(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static bool ensure_dir(const std::string& path) {
    if (path.empty() || is_dir_path(path)) {
        return true;
    }
    size_t slash = path.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
        ensure_dir(path.substr(0, slash));
    }
    return mkdir(path.c_str(), 0755) == 0 || is_dir_path(path);
}

static std::string config_dir_for_root(const std::string& root) {
    return join_path_settings(root.empty() ? "/mnt/videos" : root, ".miyoo-player");
}

std::string settings_path_for_root(const std::string& root) {
    return join_path_settings(config_dir_for_root(root), "config.ini");
}

static std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

static std::string lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

static bool parse_bool(const std::string& value) {
    std::string v = lower(trim(value));
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

static PlaybackOrder parse_order(const std::string& value) {
    std::string v = lower(trim(value));
    if (v == "loop" || v == "list_loop" || v == "listloop") return PlaybackOrder::ListLoop;
    if (v == "one" || v == "repeat_one" || v == "repeatone") return PlaybackOrder::RepeatOne;
    if (v == "rand" || v == "random" || v == "shuffle") return PlaybackOrder::Shuffle;
    return PlaybackOrder::Sequential;
}

static AppTheme parse_theme(const std::string& value) {
    std::string v = lower(trim(value));
    if (v == "amber") return AppTheme::Amber;
    if (v == "cyan") return AppTheme::Cyan;
    if (v == "light") return AppTheme::Light;
    return AppTheme::Dark;
}

static int parse_int(const std::string& value, int fallback) {
    char* end = nullptr;
    long v = std::strtol(value.c_str(), &end, 10);
    if (end == value.c_str()) {
        return fallback;
    }
    return static_cast<int>(v);
}

template <size_t N>
static int next_from_table(int value, const int (&table)[N]) {
    for (size_t i = 0; i < N; ++i) {
        if (value <= table[i]) {
            return table[(i + 1) % N];
        }
    }
    return table[0];
}

int settings_next_audio_rate(int value) {
    static const int table[] = {22050, 32000, 44100, 48000};
    return next_from_table(value, table);
}

int settings_next_audio_buffer_kb(int value) {
    static const int table[] = {48, 64, 96, 128, 192};
    return next_from_table(value, table);
}

int settings_next_sdl_audio_samples(int value) {
    static const int table[] = {512, 1024, 2048};
    return next_from_table(value, table);
}

int settings_next_audio_ui_ms(int value) {
    static const int table[] = {125, 250, 500, 1000};
    return next_from_table(value, table);
}

int settings_next_seek_step_sec(int value) {
    static const int table[] = {5, 10, 30, 60};
    return next_from_table(value, table);
}

int settings_next_osd_ms(int value) {
    static const int table[] = {1500, 3000, 5000, 8000};
    return next_from_table(value, table);
}

PlaybackOrder settings_next_order(PlaybackOrder order) {
    switch (order) {
    case PlaybackOrder::Sequential: return PlaybackOrder::ListLoop;
    case PlaybackOrder::ListLoop: return PlaybackOrder::RepeatOne;
    case PlaybackOrder::RepeatOne: return PlaybackOrder::Shuffle;
    case PlaybackOrder::Shuffle:
    default: return PlaybackOrder::Sequential;
    }
}

AppTheme settings_next_theme(AppTheme theme) {
    switch (theme) {
    case AppTheme::Dark: return AppTheme::Amber;
    case AppTheme::Amber: return AppTheme::Cyan;
    case AppTheme::Cyan: return AppTheme::Light;
    case AppTheme::Light:
    default: return AppTheme::Dark;
    }
}

const char* settings_order_name(PlaybackOrder order) {
    switch (order) {
    case PlaybackOrder::ListLoop: return "LOOP";
    case PlaybackOrder::RepeatOne: return "ONE";
    case PlaybackOrder::Shuffle: return "RAND";
    case PlaybackOrder::Sequential:
    default: return "SEQ";
    }
}

const char* settings_theme_name(AppTheme theme) {
    switch (theme) {
    case AppTheme::Amber: return "AMBER";
    case AppTheme::Cyan: return "CYAN";
    case AppTheme::Light: return "LIGHT";
    case AppTheme::Dark:
    default: return "DARK";
    }
}

bool settings_load(const std::string& root, AppSettings& settings) {
    std::ifstream in(settings_path_for_root(root).c_str());
    if (!in) {
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.resize(line.size() - 1);
        }
        std::string t = trim(line);
        if (t.empty() || t[0] == '#' || t[0] == ';') {
            continue;
        }
        size_t eq = t.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = lower(trim(t.substr(0, eq)));
        std::string value = trim(t.substr(eq + 1));
        if (key == "audio_output_rate") {
            settings.audio_output_rate = parse_int(value, settings.audio_output_rate);
        } else if (key == "audio_buffer_kb") {
            settings.audio_buffer_kb = parse_int(value, settings.audio_buffer_kb);
        } else if (key == "sdl_audio_samples") {
            settings.sdl_audio_samples = parse_int(value, settings.sdl_audio_samples);
        } else if (key == "audio_ui_ms") {
            settings.audio_ui_ms = parse_int(value, settings.audio_ui_ms);
        } else if (key == "seek_step_sec") {
            settings.seek_step_sec = parse_int(value, settings.seek_step_sec);
        } else if (key == "osd_ms") {
            settings.osd_ms = parse_int(value, settings.osd_ms);
        } else if (key == "show_hidden") {
            settings.show_hidden = parse_bool(value);
        } else if (key == "default_order") {
            settings.default_order = parse_order(value);
        } else if (key == "theme") {
            settings.theme = parse_theme(value);
        }
    }
    settings.audio_output_rate = clamp_int(settings.audio_output_rate, 8000, 96000);
    settings.audio_buffer_kb = clamp_int(settings.audio_buffer_kb, 32, 256);
    settings.sdl_audio_samples = clamp_int(settings.sdl_audio_samples, 256, 4096);
    settings.audio_ui_ms = clamp_int(settings.audio_ui_ms, 50, 2000);
    settings.seek_step_sec = clamp_int(settings.seek_step_sec, 1, 120);
    settings.osd_ms = clamp_int(settings.osd_ms, 500, 30000);
    return true;
}

bool settings_save(const std::string& root, const AppSettings& settings) {
    if (!ensure_dir(config_dir_for_root(root))) {
        return false;
    }
    std::ofstream out(settings_path_for_root(root).c_str(), std::ios::out | std::ios::trunc);
    if (!out) {
        return false;
    }
    out << "# miyoo-player settings\n";
    out << "audio_output_rate=" << settings.audio_output_rate << "\n";
    out << "audio_buffer_kb=" << settings.audio_buffer_kb << "\n";
    out << "sdl_audio_samples=" << settings.sdl_audio_samples << "\n";
    out << "audio_ui_ms=" << settings.audio_ui_ms << "\n";
    out << "seek_step_sec=" << settings.seek_step_sec << "\n";
    out << "osd_ms=" << settings.osd_ms << "\n";
    out << "show_hidden=" << (settings.show_hidden ? 1 : 0) << "\n";
    out << "default_order=" << settings_order_name(settings.default_order) << "\n";
    out << "theme=" << settings_theme_name(settings.theme) << "\n";
    return true;
}
