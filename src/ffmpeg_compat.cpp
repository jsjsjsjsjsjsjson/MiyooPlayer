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

int ffmpeg_channels(const AVCodecContext* ctx) {
    if (!ctx) return 0;
#if LIBAVUTIL_VERSION_MAJOR >= 57
    return ctx->ch_layout.nb_channels;
#else
    return ctx->channels;
#endif
}

int64_t ffmpeg_channel_layout(const AVCodecContext* ctx) {
#if LIBAVUTIL_VERSION_MAJOR >= 57
    if (!ctx) return 3;
    if (ctx->ch_layout.order == AV_CHANNEL_ORDER_NATIVE && ctx->ch_layout.u.mask) {
        return static_cast<int64_t>(ctx->ch_layout.u.mask);
    }
    AVChannelLayout fallback;
    av_channel_layout_default(&fallback, ffmpeg_channels(ctx) > 0 ? ffmpeg_channels(ctx) : 2);
    int64_t mask = 0;
    if (fallback.order == AV_CHANNEL_ORDER_NATIVE && fallback.u.mask) {
        mask = static_cast<int64_t>(fallback.u.mask);
    }
    av_channel_layout_uninit(&fallback);
    return mask ? mask : 3;
#else
    if (!ctx) return static_cast<int64_t>(AV_CH_LAYOUT_STEREO);
    if (ctx->channel_layout) {
        return static_cast<int64_t>(ctx->channel_layout);
    }
    return av_get_default_channel_layout(ctx->channels > 0 ? ctx->channels : 2);
#endif
}

void ffmpeg_set_stereo_layout(AVCodecContext* ctx) {
    if (!ctx) return;
#if LIBAVUTIL_VERSION_MAJOR >= 57
    av_channel_layout_uninit(&ctx->ch_layout);
    av_channel_layout_default(&ctx->ch_layout, 2);
#else
    ctx->channel_layout = AV_CH_LAYOUT_STEREO;
    ctx->channels = 2;
#endif
}

int ffmpeg_copy_channel_layout_to_frame(AVFrame* frame, const AVCodecContext* ctx) {
    if (!frame || !ctx) return AVERROR(EINVAL);
#if LIBAVUTIL_VERSION_MAJOR >= 57
    av_channel_layout_uninit(&frame->ch_layout);
    if (ctx->ch_layout.nb_channels > 0) {
        return av_channel_layout_copy(&frame->ch_layout, &ctx->ch_layout);
    }
    av_channel_layout_default(&frame->ch_layout, 2);
    return 0;
#else
    frame->channel_layout = ctx->channel_layout ? ctx->channel_layout : AV_CH_LAYOUT_STEREO;
    return 0;
#endif
}

SwrContext* ffmpeg_alloc_stereo_resampler(AVSampleFormat out_sample_fmt, int out_sample_rate,
                                          const AVCodecContext* in_ctx) {
    if (!in_ctx) return nullptr;
#if LIBAVUTIL_VERSION_MAJOR >= 57 && LIBSWRESAMPLE_VERSION_MAJOR >= 4
    SwrContext* swr = nullptr;
    AVChannelLayout out_layout;
    AVChannelLayout in_layout;
    av_channel_layout_default(&out_layout, 2);
    if (in_ctx->ch_layout.nb_channels > 0) {
        if (av_channel_layout_copy(&in_layout, &in_ctx->ch_layout) < 0) {
            av_channel_layout_uninit(&out_layout);
            return nullptr;
        }
    } else {
        av_channel_layout_default(&in_layout, ffmpeg_channels(in_ctx) > 0 ? ffmpeg_channels(in_ctx) : 2);
    }

    int ret = swr_alloc_set_opts2(&swr,
                                  &out_layout, out_sample_fmt, out_sample_rate,
                                  &in_layout, in_ctx->sample_fmt, in_ctx->sample_rate,
                                  0, nullptr);
    av_channel_layout_uninit(&out_layout);
    av_channel_layout_uninit(&in_layout);
    if (ret < 0) {
        if (swr) swr_free(&swr);
        return nullptr;
    }
    return swr;
#else
    return swr_alloc_set_opts(nullptr, AV_CH_LAYOUT_STEREO, out_sample_fmt, out_sample_rate,
                              ffmpeg_channel_layout(in_ctx), in_ctx->sample_fmt,
                              in_ctx->sample_rate, 0, nullptr);
#endif
}
