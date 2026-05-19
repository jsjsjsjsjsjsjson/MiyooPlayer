#include "sdl_platform.hpp"
#include "audio_queue.hpp"

AudioQueue::AudioQueue(size_t max_bytes)
    : mutex_(SDL_CreateMutex()),
      data_(max_bytes),
      max_bytes_(max_bytes),
      read_pos_(0),
      write_pos_(0),
      used_(0),
      paused_(false),
      volume_(SDL_MIX_MAXVOLUME),
      audio_clock_(0.0f),
      sample_rate_(44100),
      channels_(2),
      bytes_per_sample_(2) {}

void AudioQueue::set_format(int sample_rate, int channels, int bytes_per_sample) {
    SDL_LockMutex(mutex_);
    sample_rate_ = sample_rate > 0 ? sample_rate : 44100;
    channels_ = channels > 0 ? channels : 2;
    bytes_per_sample_ = bytes_per_sample > 0 ? bytes_per_sample : 2;
    SDL_UnlockMutex(mutex_);
}

void AudioQueue::clear() {
    SDL_LockMutex(mutex_);
    read_pos_ = 0;
    write_pos_ = 0;
    used_ = 0;
    SDL_UnlockMutex(mutex_);
}

void AudioQueue::set_paused(bool paused) {
    SDL_LockMutex(mutex_);
    paused_ = paused;
    SDL_UnlockMutex(mutex_);
}

void AudioQueue::set_volume(int volume) {
    SDL_LockMutex(mutex_);
    volume_ = clamp_int(volume, 0, SDL_MIX_MAXVOLUME);
    SDL_UnlockMutex(mutex_);
}

size_t AudioQueue::size() const {
    SDL_LockMutex(mutex_);
    size_t n = used_;
    SDL_UnlockMutex(mutex_);
    return n;
}

bool AudioQueue::push(const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        return true;
    }
    SDL_LockMutex(mutex_);
    bool ok = used_ + len <= max_bytes_;
    if (ok) {
        size_t first = std::min(len, max_bytes_ - write_pos_);
        std::memcpy(&data_[write_pos_], data, first);
        if (len > first) {
            std::memcpy(&data_[0], data + first, len - first);
        }
        write_pos_ = (write_pos_ + len) % max_bytes_;
        used_ += len;
    }
    SDL_UnlockMutex(mutex_);
    return ok;
}

void AudioQueue::callback(uint8_t* stream, int len) {
    std::memset(stream, 0, len);
    SDL_LockMutex(mutex_);
    if (!paused_) {
        int n = std::min<int>(len, static_cast<int>(used_));
        int remaining = n;
        int out_pos = 0;
        while (remaining > 0) {
            int chunk = std::min<int>(remaining, static_cast<int>(max_bytes_ - read_pos_));
            if (volume_ >= SDL_MIX_MAXVOLUME) {
                std::memcpy(stream + out_pos, &data_[read_pos_], chunk);
            } else {
                SDL_MixAudio(stream + out_pos, &data_[read_pos_], chunk, volume_);
            }
            read_pos_ = (read_pos_ + static_cast<size_t>(chunk)) % max_bytes_;
            used_ -= static_cast<size_t>(chunk);
            out_pos += chunk;
            remaining -= chunk;
        }
        if (n > 0) {
            int frame_bytes = std::max(1, channels_ * bytes_per_sample_);
            audio_clock_ += static_cast<float>(n / frame_bytes) / static_cast<float>(sample_rate_);
        }
    }
    SDL_UnlockMutex(mutex_);
}

float AudioQueue::clock() const {
    SDL_LockMutex(mutex_);
    float c = audio_clock_;
    SDL_UnlockMutex(mutex_);
    return c;
}

void AudioQueue::set_clock_base(float pts) {
    SDL_LockMutex(mutex_);
    audio_clock_ = pts;
    SDL_UnlockMutex(mutex_);
}
