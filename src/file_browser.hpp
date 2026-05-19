#pragma once

#include "common.hpp"

enum class SortMode {
    Name,
    Type,
    Time
};

struct FileEntry {
    std::string name;
    std::string path;
    bool is_dir;
    int64_t size;
    int64_t mtime;

    FileEntry(const std::string& n, const std::string& p, bool d, int64_t sz, int64_t mt)
        : name(n), path(p), is_dir(d), size(sz), mtime(mt) {}
};

class FileBrowser {
public:
    explicit FileBrowser(std::string root);

    bool refresh();
    void up();
    void down();
    void page_up();
    void page_down();
    bool enter(std::string& selected_file);
    void go_parent();
    void set_sort_mode(SortMode mode);
    void set_show_hidden(bool show);
    void select_path(const std::string& path);
    bool next_media_after(const std::string& path, std::string& next_path) const;
    bool prev_media_before(const std::string& path, std::string& prev_path) const;
    bool first_media(std::string& path) const;
    bool random_media(std::string& path, const std::string& avoid_path = std::string()) const;

    const std::string& cwd() const { return cwd_; }
    const std::vector<FileEntry>& entries() const { return entries_; }
    int selected() const { return selected_; }
    int scroll() const { return scroll_; }
    SortMode sort_mode() const { return sort_mode_; }
    bool show_hidden() const { return show_hidden_; }
    int media_count() const;
    int dir_count() const;
    const FileEntry* selected_entry() const;

private:
    bool is_media_file(const std::string& name) const;
    void fix_scroll();
    void sort_entries();

    std::string cwd_;
    std::vector<FileEntry> entries_;
    int selected_ = 0;
    int scroll_ = 0;
    SortMode sort_mode_ = SortMode::Name;
    bool show_hidden_ = false;
};
