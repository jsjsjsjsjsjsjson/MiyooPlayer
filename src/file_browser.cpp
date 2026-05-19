#include "file_browser.hpp"

#include <cerrno>
#include <cctype>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static std::string join_path(const std::string& a, const std::string& b) {
    if (a.empty() || a == "/") {
        return "/" + b;
    }
    return a + "/" + b;
}

static std::string parent_path(const std::string& p) {
    if (p.empty() || p == "/") {
        return "/";
    }
    size_t pos = p.find_last_of('/');
    if (pos == 0 || pos == std::string::npos) {
        return "/";
    }
    return p.substr(0, pos);
}

FileBrowser::FileBrowser(std::string root) : cwd_(root.empty() ? "/mnt/videos" : root) {
    refresh();
}

bool FileBrowser::is_media_file(const std::string& name) const {
    size_t dot = name.find_last_of('.');
    if (dot == std::string::npos) {
        return false;
    }
    std::string ext = name.substr(dot + 1);
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    static const char* kExt[] = {
        "mp4", "m4v", "mkv", "avi", "mov", "mpg", "mpeg",
        "mp3", "ogg", "wav", "flac", "aac", "m4a", "opus"
    };
    for (const char* e : kExt) {
        if (ext == e) {
            return true;
        }
    }
    return false;
}

bool FileBrowser::refresh() {
    entries_.clear();
    DIR* d = opendir(cwd_.c_str());
    if (!d) {
        cwd_ = "/";
        d = opendir(cwd_.c_str());
        if (!d) {
            return false;
        }
    }

    if (cwd_ != "/") {
        entries_.push_back(FileEntry("..", parent_path(cwd_), true, 0, 0));
    }

    struct dirent* de;
    while ((de = readdir(d)) != nullptr) {
        std::string name = de->d_name;
        if (name == "." || name == ".." || name.empty()) {
            continue;
        }
        if (!show_hidden_ && name[0] == '.') {
            continue;
        }
        std::string path = join_path(cwd_, name);
        struct stat st;
        if (stat(path.c_str(), &st) != 0) {
            continue;
        }
        bool is_dir = S_ISDIR(st.st_mode);
        if (!is_dir && !is_media_file(name)) {
            continue;
        }
        entries_.push_back(FileEntry(name, path, is_dir,
                                     is_dir ? 0 : static_cast<int64_t>(st.st_size),
                                     static_cast<int64_t>(st.st_mtime)));
    }
    closedir(d);

    sort_entries();
    selected_ = clamp_int(selected_, 0, entries_.empty() ? 0 : static_cast<int>(entries_.size()) - 1);
    fix_scroll();
    return true;
}

void FileBrowser::sort_entries() {
    std::sort(entries_.begin(), entries_.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.name == "..") return true;
        if (b.name == "..") return false;
        if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
        return false;
    });

    auto first_regular = std::find_if(entries_.begin(), entries_.end(), [](const FileEntry& e) {
        return e.name != "..";
    });
    std::stable_sort(first_regular, entries_.end(), [this](const FileEntry& a, const FileEntry& b) {
        if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
        if (sort_mode_ == SortMode::Time && a.mtime != b.mtime) return a.mtime > b.mtime;
        if (sort_mode_ == SortMode::Type) {
            size_t da = a.name.find_last_of('.');
            size_t db = b.name.find_last_of('.');
            std::string ea = da == std::string::npos ? "" : a.name.substr(da + 1);
            std::string eb = db == std::string::npos ? "" : b.name.substr(db + 1);
            if (ea != eb) return ea < eb;
        }
        return a.name < b.name;
    });
}

void FileBrowser::fix_scroll() {
    const int visible = 10;
    if (selected_ < scroll_) {
        scroll_ = selected_;
    }
    if (selected_ >= scroll_ + visible) {
        scroll_ = selected_ - visible + 1;
    }
    scroll_ = std::max(0, scroll_);
}

void FileBrowser::up() {
    if (!entries_.empty()) {
        selected_ = (selected_ + static_cast<int>(entries_.size()) - 1) % static_cast<int>(entries_.size());
        fix_scroll();
    }
}

void FileBrowser::down() {
    if (!entries_.empty()) {
        selected_ = (selected_ + 1) % static_cast<int>(entries_.size());
        fix_scroll();
    }
}

void FileBrowser::page_up() {
    if (!entries_.empty()) {
        selected_ = clamp_int(selected_ - 8, 0, static_cast<int>(entries_.size()) - 1);
        fix_scroll();
    }
}

void FileBrowser::page_down() {
    if (!entries_.empty()) {
        selected_ = clamp_int(selected_ + 8, 0, static_cast<int>(entries_.size()) - 1);
        fix_scroll();
    }
}

bool FileBrowser::enter(std::string& selected_file) {
    if (entries_.empty()) {
        return false;
    }
    const FileEntry& e = entries_[selected_];
    if (e.is_dir) {
        cwd_ = e.path;
        selected_ = 0;
        scroll_ = 0;
        refresh();
        return false;
    }
    selected_file = e.path;
    return true;
}

void FileBrowser::go_parent() {
    cwd_ = parent_path(cwd_);
    selected_ = 0;
    scroll_ = 0;
    refresh();
}

void FileBrowser::set_sort_mode(SortMode mode) {
    sort_mode_ = mode;
    std::string selected_path;
    if (const FileEntry* e = selected_entry()) {
        selected_path = e->path;
    }
    sort_entries();
    select_path(selected_path);
}

void FileBrowser::set_show_hidden(bool show) {
    if (show_hidden_ != show) {
        show_hidden_ = show;
        refresh();
    }
}

void FileBrowser::select_path(const std::string& path) {
    if (path.empty()) {
        fix_scroll();
        return;
    }
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (entries_[i].path == path) {
            selected_ = i;
            fix_scroll();
            return;
        }
    }
    fix_scroll();
}

bool FileBrowser::next_media_after(const std::string& path, std::string& next_path) const {
    int start = -1;
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (entries_[i].path == path) {
            start = i;
            break;
        }
    }
    if (start < 0) {
        return false;
    }
    for (int i = start + 1; i < static_cast<int>(entries_.size()); ++i) {
        if (!entries_[i].is_dir) {
            next_path = entries_[i].path;
            return true;
        }
    }
    return false;
}

bool FileBrowser::prev_media_before(const std::string& path, std::string& prev_path) const {
    int start = -1;
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        if (entries_[i].path == path) {
            start = i;
            break;
        }
    }
    if (start < 0) return false;
    for (int i = start - 1; i >= 0; --i) {
        if (!entries_[i].is_dir) {
            prev_path = entries_[i].path;
            return true;
        }
    }
    return false;
}

bool FileBrowser::first_media(std::string& path) const {
    for (const FileEntry& e : entries_) {
        if (!e.is_dir) {
            path = e.path;
            return true;
        }
    }
    return false;
}

bool FileBrowser::random_media(std::string& path, const std::string& avoid_path) const {
    std::vector<const FileEntry*> media;
    for (const FileEntry& e : entries_) {
        if (!e.is_dir && e.path != avoid_path) {
            media.push_back(&e);
        }
    }
    if (media.empty() && !avoid_path.empty()) {
        for (const FileEntry& e : entries_) {
            if (!e.is_dir) {
                media.push_back(&e);
            }
        }
    }
    if (media.empty()) {
        return false;
    }
    const FileEntry* picked = media[static_cast<size_t>(std::rand()) % media.size()];
    path = picked->path;
    return true;
}

int FileBrowser::media_count() const {
    int n = 0;
    for (const FileEntry& e : entries_) {
        if (!e.is_dir) ++n;
    }
    return n;
}

int FileBrowser::dir_count() const {
    int n = 0;
    for (const FileEntry& e : entries_) {
        if (e.is_dir && e.name != "..") ++n;
    }
    return n;
}

const FileEntry* FileBrowser::selected_entry() const {
    if (selected_ < 0 || selected_ >= static_cast<int>(entries_.size())) {
        return nullptr;
    }
    return &entries_[selected_];
}
