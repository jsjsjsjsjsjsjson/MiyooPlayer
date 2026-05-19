#include "video_renderer.hpp"

VideoRenderer::VideoRenderer() = default;

VideoRenderer::~VideoRenderer() {
    reset();
}

void VideoRenderer::reset() {
    if (surface_) {
        SDL_FreeSurface(surface_);
        surface_ = nullptr;
    }
    if (sws_) {
        sws_freeContext(sws_);
        sws_ = nullptr;
    }
    buffer_.clear();
    src_w_ = src_h_ = dst_w_ = dst_h_ = 0;
    src_fmt_ = AV_PIX_FMT_NONE;
}

bool VideoRenderer::redraw(SDL_Surface* screen) {
    if (!screen || !surface_ || dst_w_ <= 0 || dst_h_ <= 0) {
        return false;
    }

    SDL_Rect dst;
    dst.x = static_cast<Sint16>((SCREEN_W - dst_w_) / 2);
    dst.y = static_cast<Sint16>((SCREEN_H - dst_h_) / 2);
    dst.w = static_cast<Uint16>(dst_w_);
    dst.h = static_cast<Uint16>(dst_h_);
    SDL_FillRect(screen, nullptr, SDL_MapRGB(screen->format, 0, 0, 0));
    SDL_BlitSurface(surface_, nullptr, screen, &dst);
    return true;
}

bool VideoRenderer::render(SDL_Surface* screen, AVFrame* frame, AVCodecContext* codec) {
    if (!screen || !frame || !codec) {
        return false;
    }

    int vw = codec->width > 0 ? codec->width : frame->width;
    int vh = codec->height > 0 ? codec->height : frame->height;
    if (vw <= 0 || vh <= 0) {
        return false;
    }

    float sx = static_cast<float>(SCREEN_W) / static_cast<float>(vw);
    float sy = static_cast<float>(SCREEN_H) / static_cast<float>(vh);
    float sc = std::min(sx, sy);
    int dw = std::max(1, static_cast<int>(vw * sc));
    int dh = std::max(1, static_cast<int>(vh * sc));
    dw &= ~1;
    dh &= ~1;

    AVPixelFormat fmt = static_cast<AVPixelFormat>(frame->format);
    if (!sws_ || src_w_ != vw || src_h_ != vh || src_fmt_ != fmt || dst_w_ != dw || dst_h_ != dh) {
        reset();
        sws_ = sws_getContext(vw, vh, fmt, dw, dh, AV_PIX_FMT_RGB565LE,
                              SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        if (!sws_) {
            return false;
        }
        src_w_ = vw;
        src_h_ = vh;
        src_fmt_ = fmt;
        dst_w_ = dw;
        dst_h_ = dh;
        buffer_.resize(static_cast<size_t>(dw * dh * 2));
        surface_ = SDL_CreateRGBSurfaceFrom(buffer_.data(), dw, dh, 16, dw * 2,
                                            0xF800, 0x07E0, 0x001F, 0);
        if (!surface_) {
            reset();
            return false;
        }
    }

    uint8_t* dst_data[4] = { buffer_.data(), nullptr, nullptr, nullptr };
    int dst_linesize[4] = { dw * 2, 0, 0, 0 };
    sws_scale(sws_, frame->data, frame->linesize, 0, vh, dst_data, dst_linesize);

    return redraw(screen);
}
