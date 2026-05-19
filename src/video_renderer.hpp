#pragma once

#include "ffmpeg_compat.hpp"
#include "sdl_platform.hpp"

class VideoRenderer {
public:
    VideoRenderer();
    ~VideoRenderer();

    bool render(SDL_Surface* screen, AVFrame* frame, AVCodecContext* codec);
    bool redraw(SDL_Surface* screen);
    void reset();

private:
    SwsContext* sws_ = nullptr;
    int src_w_ = 0;
    int src_h_ = 0;
    AVPixelFormat src_fmt_ = AV_PIX_FMT_NONE;
    int dst_w_ = 0;
    int dst_h_ = 0;
    std::vector<uint8_t> buffer_;
    SDL_Surface* surface_ = nullptr;
};
