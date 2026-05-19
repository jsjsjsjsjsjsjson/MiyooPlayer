#include "ffmpeg_compat.hpp"

void ffmpeg_init_once() {
#if LIBAVFORMAT_VERSION_MAJOR < 58
    av_register_all();
#endif
    avformat_network_init();
}

void ffmpeg_flush_codec(AVCodecContext* ctx) {
    if (ctx) {
        avcodec_flush_buffers(ctx);
    }
}

const AVCodec* ffmpeg_find_decoder(enum AVCodecID id) {
#if LIBAVCODEC_VERSION_MAJOR >= 59
    return avcodec_find_decoder(id);
#else
    return avcodec_find_decoder(id);
#endif
}

int64_t ffmpeg_channel_layout(const AVCodecContext* ctx) {
#if LIBAVUTIL_VERSION_MAJOR >= 57
    if (ctx->ch_layout.u.mask) {
        return static_cast<int64_t>(ctx->ch_layout.u.mask);
    }
    return av_get_default_channel_layout(ctx->ch_layout.nb_channels);
#else
    if (ctx->channel_layout) {
        return static_cast<int64_t>(ctx->channel_layout);
    }
    return av_get_default_channel_layout(ctx->channels);
#endif
}

int ffmpeg_channels(const AVCodecContext* ctx) {
#if LIBAVUTIL_VERSION_MAJOR >= 57
    return ctx->ch_layout.nb_channels;
#else
    return ctx->channels;
#endif
}
