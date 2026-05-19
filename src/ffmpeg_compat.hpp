#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

void ffmpeg_init_once();
void ffmpeg_flush_codec(AVCodecContext* ctx);
const AVCodec* ffmpeg_find_decoder(enum AVCodecID id);
int64_t ffmpeg_channel_layout(const AVCodecContext* ctx);
int ffmpeg_channels(const AVCodecContext* ctx);
