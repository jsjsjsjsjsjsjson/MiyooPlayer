#pragma once

#include "common.hpp"
#include "sdl_platform.hpp"

class AudioQueue {
public:
    explicit AudioQueue(size_t max_bytes = 96 * 1024);

    void clear();
    void set_paused(bool paused);
    void set_volume(int volume);
    int volume() const { return volume_; }
    size_t size() const;
    bool push(const uint8_t* data, size_t len);
    void callback(uint8_t* stream, int len);
    float clock() const;
    void set_clock_base(float pts);

private:
    mutable SDL_mutex* mutex_;
    std::vector<uint8_t> data_;
    size_t max_bytes_;
    size_t read_pos_;
    size_t write_pos_;
    size_t used_;
    bool paused_;
    int volume_;
    float audio_clock_;
    int sample_rate_;
    int channels_;
    int bytes_per_sample_;

public:
    void set_format(int sample_rate, int channels, int bytes_per_sample);
};
