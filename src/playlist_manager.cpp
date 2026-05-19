#include "playlist_manager.hpp"

#include <cctype>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

static std::string join_path2(const std::string& a, const std::string& b) {
    if (a.empty() || a == "/") return "/" + b;
    return a + "/" + b;
}

static bool is_dir_path(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static bool ensure_dir(const std::string& path) {
    if (path.empty() || is_dir_path(path)) return true;
    size_t slash = path.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
        ensure_dir(path.substr(0, slash));
    }
    return mkdir(path.c_str(), 0755) == 0 || is_dir_path(path);
}

static std::string basename_no_ext(const std::string& path) {
    size_t slash = path.find_last_of('/');
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return name;
}

static std::string safe_name(std::string name) {
    for (char& c : name) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (!std::isalnum(uc) && c != '-' && c != '_') c = '_';
    }
    if (name.empty()) name = "playlist";
    return name;
}

static bool is_playlist_file(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return false;
    std::string ext = path.substr(dot + 1);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == "m3u" || ext == "m3u8";
}

PlaylistManager::PlaylistManager(const std::string& media_root)
    : media_root_(media_root),
      config_dir_(join_path2(media_root, ".miyoo-player")),
      playlist_dir_(join_path2(config_dir_, "playlists")) {}

bool PlaylistManager::ensure_dirs() const {
    return ensure_dir(config_dir_) && ensure_dir(playlist_dir_);
}

bool PlaylistManager::is_media_file(const std::string& path) const {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return false;
    std::string ext = path.substr(dot + 1);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const char* kExt[] = {
        "mp4", "m4v", "mkv", "avi", "mov", "mpg", "mpeg",
        "mp3", "ogg", "wav", "flac", "aac", "m4a", "opus"
    };
    for (const char* e : kExt) {
        if (ext == e) return true;
    }
    return false;
}

bool PlaylistManager::load() {
    ensure_dirs();
    playlists_.clear();
    DIR* d = opendir(playlist_dir_.c_str());
    if (!d) return false;

    struct dirent* de;
    while ((de = readdir(d)) != nullptr) {
        std::string name = de->d_name;
        if (name == "." || name == "..") continue;
        std::string path = join_path2(playlist_dir_, name);
        if (!is_playlist_file(path)) continue;
        Playlist p;
        p.name = basename_no_ext(name);
        p.path = path;
        std::ifstream in(path.c_str());
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            if (!line.empty() && line[line.size() - 1] == '\r') line.resize(line.size() - 1);
            p.items.push_back(line);
        }
        playlists_.push_back(p);
    }
    closedir(d);
    std::sort(playlists_.begin(), playlists_.end(), [](const Playlist& a, const Playlist& b) {
        return a.name < b.name;
    });
    selected_ = clamp_int(selected_, 0, playlists_.empty() ? 0 : static_cast<int>(playlists_.size()) - 1);
    return true;
}

bool PlaylistManager::save(int index) const {
    if (index < 0 || index >= static_cast<int>(playlists_.size())) return false;
    ensure_dirs();
    const Playlist& p = playlists_[index];
    std::ofstream out(p.path.c_str(), std::ios::out | std::ios::trunc);
    if (!out) return false;
    out << "#EXTM3U\n";
    for (const std::string& item : p.items) {
        out << item << "\n";
    }
    return true;
}

std::string PlaylistManager::next_name() const {
    for (int i = 1; i < 100; ++i) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "Playlist_%02d", i);
        bool used = false;
        for (const Playlist& p : playlists_) {
            if (p.name == buf) {
                used = true;
                break;
            }
        }
        if (!used) return buf;
    }
    return "Playlist";
}

static bool playlist_name_used(const std::vector<Playlist>& playlists, const std::string& name) {
    for (const Playlist& p : playlists) {
        if (p.name == name) return true;
    }
    return false;
}

static std::string unique_playlist_name(const std::vector<Playlist>& playlists, const std::string& base) {
    std::string name = base.empty() ? "Playlist" : base;
    if (!playlist_name_used(playlists, name)) return name;
    for (int i = 2; i < 100; ++i) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s_%02d", name.c_str(), i);
        if (!playlist_name_used(playlists, buf)) return buf;
    }
    return name + "_X";
}

std::string PlaylistManager::playlist_path_for_name(const std::string& name) const {
    return join_path2(playlist_dir_, safe_name(name) + ".m3u8");
}

int PlaylistManager::create_empty() {
    ensure_dirs();
    Playlist p;
    p.name = next_name();
    p.path = playlist_path_for_name(p.name);
    playlists_.push_back(p);
    selected_ = static_cast<int>(playlists_.size()) - 1;
    save(selected_);
    return selected_;
}

void PlaylistManager::scan_folder(const std::string& folder, std::vector<std::string>& out) const {
    DIR* d = opendir(folder.c_str());
    if (!d) return;
    struct dirent* de;
    while ((de = readdir(d)) != nullptr) {
        std::string name = de->d_name;
        if (name == "." || name == ".." || name.empty() || name[0] == '.') continue;
        std::string path = join_path2(folder, name);
        struct stat st;
        if (stat(path.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            scan_folder(path, out);
        } else if (is_media_file(path)) {
            out.push_back(path);
        }
    }
    closedir(d);
}

int PlaylistManager::create_from_folder(const std::string& folder) {
    ensure_dirs();
    Playlist p;
    p.name = safe_name(basename_no_ext(folder.empty() ? media_root_ : folder));
    if (p.name == "." || p.name == "/") p.name = "Folder";
    p.name = unique_playlist_name(playlists_, p.name);
    p.path = playlist_path_for_name(p.name);
    scan_folder(folder, p.items);
    if (p.items.empty()) return -1;
    std::sort(p.items.begin(), p.items.end());
    playlists_.push_back(p);
    selected_ = static_cast<int>(playlists_.size()) - 1;
    save(selected_);
    return selected_;
}

Playlist PlaylistManager::temporary_from_folder(const std::string& folder) const {
    Playlist p;
    p.name = "TEMP";
    p.path.clear();
    scan_folder(folder, p.items);
    std::sort(p.items.begin(), p.items.end());
    return p;
}

Playlist PlaylistManager::temporary_from_files(const std::vector<std::string>& files) const {
    Playlist p;
    p.name = "MARKED";
    p.path.clear();
    p.items = files;
    return p;
}

bool PlaylistManager::add_file(int index, const std::string& file) {
    if (file.empty()) return false;
    if (index < 0) index = create_empty();
    if (index < 0 || index >= static_cast<int>(playlists_.size())) return false;
    Playlist& p = playlists_[index];
    if (std::find(p.items.begin(), p.items.end(), file) == p.items.end()) {
        p.items.push_back(file);
    }
    return save(index);
}

bool PlaylistManager::add_file_to_selected(const std::string& file) {
    if (playlists_.empty()) create_empty();
    return add_file(selected_, file);
}

bool PlaylistManager::remove_playlist(int index) {
    if (index < 0 || index >= static_cast<int>(playlists_.size())) return false;
    std::string path = playlists_[index].path;
    if (!path.empty()) {
        unlink(path.c_str());
    }
    playlists_.erase(playlists_.begin() + index);
    selected_ = clamp_int(index, 0, playlists_.empty() ? 0 : static_cast<int>(playlists_.size()) - 1);
    return true;
}

bool PlaylistManager::remove_item(int playlist_index, int item_index) {
    if (playlist_index < 0 || playlist_index >= static_cast<int>(playlists_.size())) return false;
    Playlist& p = playlists_[playlist_index];
    if (item_index < 0 || item_index >= static_cast<int>(p.items.size())) return false;
    p.items.erase(p.items.begin() + item_index);
    return save(playlist_index);
}

void PlaylistManager::left() {
    if (!playlists_.empty()) {
        selected_ = (selected_ + static_cast<int>(playlists_.size()) - 1) % static_cast<int>(playlists_.size());
    }
}

void PlaylistManager::right() {
    if (!playlists_.empty()) {
        selected_ = (selected_ + 1) % static_cast<int>(playlists_.size());
    }
}

void PlaylistManager::select(int index) {
    selected_ = clamp_int(index, 0, playlists_.empty() ? 0 : static_cast<int>(playlists_.size()) - 1);
}

const Playlist* PlaylistManager::selected_playlist() const {
    return get(selected_);
}

Playlist* PlaylistManager::selected_playlist() {
    return get(selected_);
}

const Playlist* PlaylistManager::get(int index) const {
    if (index < 0 || index >= static_cast<int>(playlists_.size())) return nullptr;
    return &playlists_[index];
}

Playlist* PlaylistManager::get(int index) {
    if (index < 0 || index >= static_cast<int>(playlists_.size())) return nullptr;
    return &playlists_[index];
}
