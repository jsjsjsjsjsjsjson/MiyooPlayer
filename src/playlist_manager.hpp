#pragma once

#include "common.hpp"

struct Playlist {
    std::string name;
    std::string path;
    std::vector<std::string> items;
};

class PlaylistManager {
public:
    explicit PlaylistManager(const std::string& media_root);

    bool load();
    bool save(int index) const;
    int create_empty();
    int create_from_folder(const std::string& folder);
    Playlist temporary_from_folder(const std::string& folder) const;
    Playlist temporary_from_files(const std::vector<std::string>& files) const;
    bool add_file(int index, const std::string& file);
    bool add_file_to_selected(const std::string& file);
    bool remove_playlist(int index);
    bool remove_item(int playlist_index, int item_index);

    void left();
    void right();
    void select(int index);

    int selected() const { return selected_; }
    int count() const { return static_cast<int>(playlists_.size()); }
    const Playlist* selected_playlist() const;
    Playlist* selected_playlist();
    const Playlist* get(int index) const;
    Playlist* get(int index);
    const std::vector<Playlist>& playlists() const { return playlists_; }
    const std::string& directory() const { return playlist_dir_; }

private:
    bool ensure_dirs() const;
    bool is_media_file(const std::string& path) const;
    void scan_folder(const std::string& folder, std::vector<std::string>& out) const;
    std::string next_name() const;
    std::string playlist_path_for_name(const std::string& name) const;

    std::string media_root_;
    std::string config_dir_;
    std::string playlist_dir_;
    std::vector<Playlist> playlists_;
    int selected_ = 0;
};
