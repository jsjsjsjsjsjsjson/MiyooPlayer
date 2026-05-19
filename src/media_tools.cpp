#include "media_tools.hpp"

#include "ffmpeg_compat.hpp"

#include <cctype>
#include <fstream>
#include <sys/stat.h>

static std::string join_path3(const std::string& a, const std::string& b) {
    if (a.empty() || a == "/") return "/" + b;
    return a + "/" + b;
}

static bool ensure_dir3(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) return true;
    size_t slash = path.find_last_of('/');
    if (slash != std::string::npos && slash > 0) ensure_dir3(path.substr(0, slash));
    return mkdir(path.c_str(), 0755) == 0 || (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
}

static std::string basename_no_ext3(const std::string& path) {
    size_t slash = path.find_last_of('/');
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    for (char& c : name) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (!std::isalnum(uc) && c != '-' && c != '_') c = '_';
    }
    if (name.empty()) name = "media";
    return name;
}

static std::string export_dir(const std::string& root) {
    std::string dir = join_path3(join_path3(root, ".miyoo-player"), "exports");
    ensure_dir3(dir);
    return dir;
}

const char* convert_target_name(ConvertTarget target) {
    switch (target) {
    case ConvertTarget::Mp4Copy: return "MP4 COPY";
    case ConvertTarget::WavPcm: return "WAV PCM";
    case ConvertTarget::OggVorbis: return "OGG VORB";
    case ConvertTarget::Mp3: return "MP3";
    case ConvertTarget::Aac: return "AAC";
    case ConvertTarget::MkvCopy:
    default: return "MKV COPY";
    }
}

ConvertTarget convert_next_target(ConvertTarget target) {
    switch (target) {
    case ConvertTarget::MkvCopy: return ConvertTarget::Mp4Copy;
    case ConvertTarget::Mp4Copy: return ConvertTarget::WavPcm;
    case ConvertTarget::WavPcm: return ConvertTarget::OggVorbis;
    case ConvertTarget::OggVorbis: return ConvertTarget::Mp3;
    case ConvertTarget::Mp3: return ConvertTarget::Aac;
    case ConvertTarget::Aac:
    default: return ConvertTarget::MkvCopy;
    }
}

int convert_next_sample_rate(int current) {
    static const int rates[] = {22050, 32000, 44100, 48000};
    for (int i = 0; i < 4; ++i) {
        if (rates[i] == current) return rates[(i + 1) % 4];
    }
    return 44100;
}

int convert_next_bit_depth(int current) {
    if (current == 16) return 24;
    if (current == 24) return 32;
    return 16;
}

int convert_next_bitrate(int current) {
    static const int rates[] = {96, 128, 160, 192, 256};
    for (int i = 0; i < 5; ++i) {
        if (rates[i] == current) return rates[(i + 1) % 5];
    }
    return 128;
}

static MediaToolResult media_remux_copy(const std::string& input, const std::string& media_root,
                                        const char* format, const char* extension) {
    MediaToolResult res;
    AVFormatContext* in = nullptr;
    AVFormatContext* out = nullptr;
    if (avformat_open_input(&in, input.c_str(), nullptr, nullptr) < 0) {
        res.error = "open input failed";
        return res;
    }
    if (avformat_find_stream_info(in, nullptr) < 0) {
        res.error = "stream info failed";
        avformat_close_input(&in);
        return res;
    }

    res.output = join_path3(export_dir(media_root), basename_no_ext3(input) + extension);
    if (avformat_alloc_output_context2(&out, nullptr, format, res.output.c_str()) < 0 || !out) {
        res.error = "output alloc failed";
        avformat_close_input(&in);
        return res;
    }
    std::vector<int> map(in->nb_streams, -1);
    for (unsigned i = 0; i < in->nb_streams; ++i) {
        AVStream* ist = in->streams[i];
        if (ist->codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            ist->codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            ist->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) continue;
        AVStream* ost = avformat_new_stream(out, nullptr);
        if (!ost) continue;
        map[i] = static_cast<int>(ost->index);
        avcodec_parameters_copy(ost->codecpar, ist->codecpar);
        ost->codecpar->codec_tag = 0;
    }
    if (!(out->oformat->flags & AVFMT_NOFILE) &&
        avio_open(&out->pb, res.output.c_str(), AVIO_FLAG_WRITE) < 0) {
        res.error = "output open failed";
        avformat_free_context(out);
        avformat_close_input(&in);
        return res;
    }
    if (avformat_write_header(out, nullptr) < 0) {
        res.error = "write header failed";
        if (!(out->oformat->flags & AVFMT_NOFILE)) avio_closep(&out->pb);
        avformat_free_context(out);
        avformat_close_input(&in);
        return res;
    }
    AVPacket pkt;
    av_init_packet(&pkt);
    while (av_read_frame(in, &pkt) >= 0) {
        int out_index = pkt.stream_index >= 0 && pkt.stream_index < static_cast<int>(map.size())
            ? map[pkt.stream_index] : -1;
        if (out_index >= 0) {
            AVStream* ist = in->streams[pkt.stream_index];
            AVStream* ost = out->streams[out_index];
            pkt.pts = av_rescale_q_rnd(pkt.pts, ist->time_base, ost->time_base,
                                       static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            pkt.dts = av_rescale_q_rnd(pkt.dts, ist->time_base, ost->time_base,
                                       static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            pkt.duration = av_rescale_q(pkt.duration, ist->time_base, ost->time_base);
            pkt.pos = -1;
            pkt.stream_index = out_index;
            av_interleaved_write_frame(out, &pkt);
        }
        av_packet_unref(&pkt);
    }
    av_write_trailer(out);
    if (!(out->oformat->flags & AVFMT_NOFILE)) avio_closep(&out->pb);
    avformat_free_context(out);
    avformat_close_input(&in);
    res.ok = true;
    return res;
}

MediaToolResult media_remux_to_mkv(const std::string& input, const std::string& media_root) {
    MediaToolResult res;
    AVFormatContext* in = nullptr;
    AVFormatContext* out = nullptr;
    if (avformat_open_input(&in, input.c_str(), nullptr, nullptr) < 0) {
        res.error = "open input failed";
        return res;
    }
    if (avformat_find_stream_info(in, nullptr) < 0) {
        res.error = "stream info failed";
        avformat_close_input(&in);
        return res;
    }

    res.output = join_path3(export_dir(media_root), basename_no_ext3(input) + ".mkv");
    if (avformat_alloc_output_context2(&out, nullptr, "matroska", res.output.c_str()) < 0 || !out) {
        res.error = "output alloc failed";
        avformat_close_input(&in);
        return res;
    }

    std::vector<int> map(in->nb_streams, -1);
    for (unsigned i = 0; i < in->nb_streams; ++i) {
        AVStream* ist = in->streams[i];
        if (ist->codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            ist->codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            ist->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            continue;
        }
        AVStream* ost = avformat_new_stream(out, nullptr);
        if (!ost) continue;
        map[i] = static_cast<int>(ost->index);
        avcodec_parameters_copy(ost->codecpar, ist->codecpar);
        ost->codecpar->codec_tag = 0;
    }

    if (!(out->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&out->pb, res.output.c_str(), AVIO_FLAG_WRITE) < 0) {
            res.error = "output open failed";
            avformat_free_context(out);
            avformat_close_input(&in);
            return res;
        }
    }
    if (avformat_write_header(out, nullptr) < 0) {
        res.error = "write header failed";
        if (!(out->oformat->flags & AVFMT_NOFILE)) avio_closep(&out->pb);
        avformat_free_context(out);
        avformat_close_input(&in);
        return res;
    }

    AVPacket pkt;
    av_init_packet(&pkt);
    while (av_read_frame(in, &pkt) >= 0) {
        int out_index = pkt.stream_index >= 0 && pkt.stream_index < static_cast<int>(map.size())
            ? map[pkt.stream_index] : -1;
        if (out_index >= 0) {
            AVStream* ist = in->streams[pkt.stream_index];
            AVStream* ost = out->streams[out_index];
            pkt.pts = av_rescale_q_rnd(pkt.pts, ist->time_base, ost->time_base,
                                       static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            pkt.dts = av_rescale_q_rnd(pkt.dts, ist->time_base, ost->time_base,
                                       static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            pkt.duration = av_rescale_q(pkt.duration, ist->time_base, ost->time_base);
            pkt.pos = -1;
            pkt.stream_index = out_index;
            av_interleaved_write_frame(out, &pkt);
        }
        av_packet_unref(&pkt);
    }
    av_write_trailer(out);
    if (!(out->oformat->flags & AVFMT_NOFILE)) avio_closep(&out->pb);
    avformat_free_context(out);
    avformat_close_input(&in);
    res.ok = true;
    return res;
}

static void wav_u16(std::ofstream& out, uint16_t v) {
    out.put(static_cast<char>(v & 0xff));
    out.put(static_cast<char>((v >> 8) & 0xff));
}

static void wav_u32(std::ofstream& out, uint32_t v) {
    wav_u16(out, static_cast<uint16_t>(v & 0xffff));
    wav_u16(out, static_cast<uint16_t>((v >> 16) & 0xffff));
}

static void write_wav_header(std::ofstream& out, uint32_t data_bytes, int sample_rate, int bit_depth) {
    int bytes_per_sample = bit_depth == 24 ? 3 : (bit_depth == 32 ? 4 : 2);
    int block_align = 2 * bytes_per_sample;
    out.seekp(0, std::ios::beg);
    out.write("RIFF", 4);
    wav_u32(out, 36 + data_bytes);
    out.write("WAVEfmt ", 8);
    wav_u32(out, 16);
    wav_u16(out, 1);
    wav_u16(out, 2);
    wav_u32(out, static_cast<uint32_t>(sample_rate));
    wav_u32(out, static_cast<uint32_t>(sample_rate * block_align));
    wav_u16(out, static_cast<uint16_t>(block_align));
    wav_u16(out, static_cast<uint16_t>(bit_depth));
    out.write("data", 4);
    wav_u32(out, data_bytes);
}

static uint32_t write_pcm_samples(std::ofstream& out, const std::vector<uint8_t>& data, int samples,
                                  int bit_depth) {
    int sample_count = samples * 2;
    if (bit_depth == 16) {
        uint32_t bytes = static_cast<uint32_t>(sample_count * 2);
        out.write(reinterpret_cast<const char*>(data.data()), bytes);
        return bytes;
    }
    if (bit_depth == 24) {
        const uint8_t* p = data.data();
        for (int i = 0; i < sample_count; ++i) {
            out.put(static_cast<char>(p[i * 4 + 1]));
            out.put(static_cast<char>(p[i * 4 + 2]));
            out.put(static_cast<char>(p[i * 4 + 3]));
        }
        return static_cast<uint32_t>(sample_count * 3);
    }
    uint32_t bytes = static_cast<uint32_t>(sample_count * 4);
    out.write(reinterpret_cast<const char*>(data.data()), bytes);
    return bytes;
}

static MediaToolResult media_extract_audio_wav_opts(const std::string& input, const std::string& media_root,
                                                    const ConversionOptions& options) {
    MediaToolResult res;
    AVFormatContext* fmt = nullptr;
    AVCodecContext* dec_ctx = nullptr;
    SwrContext* swr = nullptr;
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    int audio_stream = -1;
    if (!pkt || !frame) {
        res.error = "alloc failed";
        goto done;
    }
    if (avformat_open_input(&fmt, input.c_str(), nullptr, nullptr) < 0 ||
        avformat_find_stream_info(fmt, nullptr) < 0) {
        res.error = "open input failed";
        goto done;
    }
    for (unsigned i = 0; i < fmt->nb_streams; ++i) {
        if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream = static_cast<int>(i);
            break;
        }
    }
    if (audio_stream < 0) {
        res.error = "no audio";
        goto done;
    }
    {
        const AVCodec* dec = ffmpeg_find_decoder(fmt->streams[audio_stream]->codecpar->codec_id);
        if (!dec) {
            res.error = "decoder missing";
            goto done;
        }
        dec_ctx = avcodec_alloc_context3(dec);
        if (!dec_ctx ||
            avcodec_parameters_to_context(dec_ctx, fmt->streams[audio_stream]->codecpar) < 0 ||
            avcodec_open2(dec_ctx, dec, nullptr) < 0) {
            res.error = "decoder open failed";
            goto done;
        }
        AVSampleFormat out_fmt = options.bit_depth == 16 ? AV_SAMPLE_FMT_S16 : AV_SAMPLE_FMT_S32;
        swr = swr_alloc_set_opts(nullptr, AV_CH_LAYOUT_STEREO, out_fmt, options.sample_rate,
                                 ffmpeg_channel_layout(dec_ctx), dec_ctx->sample_fmt,
                                 dec_ctx->sample_rate, 0, nullptr);
        if (!swr || swr_init(swr) < 0) {
            res.error = "resampler failed";
            goto done;
        }
    }

    res.output = join_path3(export_dir(media_root), basename_no_ext3(input) + ".wav");
    {
        std::ofstream out(res.output.c_str(), std::ios::binary | std::ios::trunc);
        if (!out) {
            res.error = "wav open failed";
            goto done;
        }
        write_wav_header(out, 0, options.sample_rate, options.bit_depth);
        uint32_t written = 0;
        std::vector<uint8_t> audio_buf;
        while (av_read_frame(fmt, pkt) >= 0) {
            if (pkt->stream_index == audio_stream && avcodec_send_packet(dec_ctx, pkt) == 0) {
                while (avcodec_receive_frame(dec_ctx, frame) == 0) {
                    int out_samples = av_rescale_rnd(swr_get_delay(swr, dec_ctx->sample_rate) + frame->nb_samples,
                                                     options.sample_rate, dec_ctx->sample_rate, AV_ROUND_UP);
                    int out_bps = options.bit_depth == 16 ? 2 : 4;
                    audio_buf.resize(static_cast<size_t>(out_samples * 2 * out_bps));
                    uint8_t* dst[] = { audio_buf.data(), nullptr };
                    int got = swr_convert(swr, dst, out_samples,
                                          const_cast<const uint8_t**>(frame->data), frame->nb_samples);
                    if (got > 0) {
                        written += write_pcm_samples(out, audio_buf, got, options.bit_depth);
                    }
                    av_frame_unref(frame);
                }
            }
            av_packet_unref(pkt);
        }
        avcodec_send_packet(dec_ctx, nullptr);
        while (avcodec_receive_frame(dec_ctx, frame) == 0) {
            int out_samples = av_rescale_rnd(swr_get_delay(swr, dec_ctx->sample_rate) + frame->nb_samples,
                                             options.sample_rate, dec_ctx->sample_rate, AV_ROUND_UP);
            int out_bps = options.bit_depth == 16 ? 2 : 4;
            audio_buf.resize(static_cast<size_t>(out_samples * 2 * out_bps));
            uint8_t* dst[] = { audio_buf.data(), nullptr };
            int got = swr_convert(swr, dst, out_samples,
                                  const_cast<const uint8_t**>(frame->data), frame->nb_samples);
            if (got > 0) {
                written += write_pcm_samples(out, audio_buf, got, options.bit_depth);
            }
            av_frame_unref(frame);
        }
        write_wav_header(out, written, options.sample_rate, options.bit_depth);
        res.ok = true;
    }

done:
    if (swr) swr_free(&swr);
    if (dec_ctx) avcodec_free_context(&dec_ctx);
    if (fmt) avformat_close_input(&fmt);
    if (pkt) av_packet_free(&pkt);
    if (frame) av_frame_free(&frame);
    return res;
}

static AVSampleFormat choose_sample_fmt(const AVCodec* enc) {
    if (!enc || !enc->sample_fmts) return AV_SAMPLE_FMT_FLTP;
    for (const AVSampleFormat* p = enc->sample_fmts; *p != AV_SAMPLE_FMT_NONE; ++p) {
        if (*p == AV_SAMPLE_FMT_FLTP || *p == AV_SAMPLE_FMT_S16P || *p == AV_SAMPLE_FMT_S16) return *p;
    }
    return enc->sample_fmts[0];
}

static bool encode_audio_frame(AVCodecContext* enc_ctx, AVFormatContext* out_fmt, AVStream* out_stream,
                               AVFrame* frame, MediaToolResult& res) {
    int ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        res.error = "encode send failed";
        return false;
    }
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = nullptr;
    pkt.size = 0;
    while ((ret = avcodec_receive_packet(enc_ctx, &pkt)) == 0) {
        av_packet_rescale_ts(&pkt, enc_ctx->time_base, out_stream->time_base);
        pkt.stream_index = out_stream->index;
        if (av_interleaved_write_frame(out_fmt, &pkt) < 0) {
            av_packet_unref(&pkt);
            res.error = "write packet failed";
            return false;
        }
        av_packet_unref(&pkt);
    }
    return ret == AVERROR(EAGAIN) || ret == AVERROR_EOF;
}

static MediaToolResult media_encode_audio(const std::string& input, const std::string& media_root,
                                          const ConversionOptions& options, AVCodecID codec_id,
                                          const char* format, const char* extension) {
    MediaToolResult res;
    AVFormatContext* in_fmt = nullptr;
    AVFormatContext* out_fmt = nullptr;
    AVCodecContext* dec_ctx = nullptr;
    AVCodecContext* enc_ctx = nullptr;
    SwrContext* swr = nullptr;
    AVPacket* pkt = av_packet_alloc();
    AVFrame* decoded = av_frame_alloc();
    AVFrame* converted = av_frame_alloc();
    int audio_stream = -1;
    AVStream* out_stream = nullptr;
    if (!pkt || !decoded || !converted) {
        res.error = "alloc failed";
        goto done;
    }
    if (avformat_open_input(&in_fmt, input.c_str(), nullptr, nullptr) < 0 ||
        avformat_find_stream_info(in_fmt, nullptr) < 0) {
        res.error = "open input failed";
        goto done;
    }
    for (unsigned i = 0; i < in_fmt->nb_streams; ++i) {
        if (in_fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream = static_cast<int>(i);
            break;
        }
    }
    if (audio_stream < 0) {
        res.error = "no audio";
        goto done;
    }
    {
        const AVCodec* dec = ffmpeg_find_decoder(in_fmt->streams[audio_stream]->codecpar->codec_id);
        const AVCodec* enc = avcodec_find_encoder(codec_id);
        if (!dec || !enc) {
            res.error = "encoder not enabled";
            goto done;
        }
        dec_ctx = avcodec_alloc_context3(dec);
        enc_ctx = avcodec_alloc_context3(enc);
        if (!dec_ctx || !enc_ctx ||
            avcodec_parameters_to_context(dec_ctx, in_fmt->streams[audio_stream]->codecpar) < 0 ||
            avcodec_open2(dec_ctx, dec, nullptr) < 0) {
            res.error = "decoder open failed";
            goto done;
        }
        enc_ctx->sample_rate = options.sample_rate;
        enc_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
        enc_ctx->channels = 2;
        enc_ctx->sample_fmt = choose_sample_fmt(enc);
        enc_ctx->bit_rate = options.bitrate_kbps * 1000;
        enc_ctx->time_base = AVRational{1, options.sample_rate};
        res.output = join_path3(export_dir(media_root), basename_no_ext3(input) + extension);
        if (avformat_alloc_output_context2(&out_fmt, nullptr, format, res.output.c_str()) < 0 || !out_fmt) {
            res.error = "output alloc failed";
            goto done;
        }
        if (out_fmt->oformat->flags & AVFMT_GLOBALHEADER) enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        if (avcodec_open2(enc_ctx, enc, nullptr) < 0) {
            res.error = "encoder open failed";
            goto done;
        }
        out_stream = avformat_new_stream(out_fmt, nullptr);
        if (!out_stream || avcodec_parameters_from_context(out_stream->codecpar, enc_ctx) < 0) {
            res.error = "stream setup failed";
            goto done;
        }
        out_stream->time_base = enc_ctx->time_base;
        swr = swr_alloc_set_opts(nullptr, enc_ctx->channel_layout, enc_ctx->sample_fmt, enc_ctx->sample_rate,
                                 ffmpeg_channel_layout(dec_ctx), dec_ctx->sample_fmt,
                                 dec_ctx->sample_rate, 0, nullptr);
        if (!swr || swr_init(swr) < 0) {
            res.error = "resampler failed";
            goto done;
        }
    }
    if (!(out_fmt->oformat->flags & AVFMT_NOFILE) &&
        avio_open(&out_fmt->pb, res.output.c_str(), AVIO_FLAG_WRITE) < 0) {
        res.error = "output open failed";
        goto done;
    }
    if (avformat_write_header(out_fmt, nullptr) < 0) {
        res.error = "write header failed";
        goto done;
    }

    while (av_read_frame(in_fmt, pkt) >= 0) {
        if (pkt->stream_index == audio_stream && avcodec_send_packet(dec_ctx, pkt) == 0) {
            while (avcodec_receive_frame(dec_ctx, decoded) == 0) {
                int out_samples = av_rescale_rnd(swr_get_delay(swr, dec_ctx->sample_rate) + decoded->nb_samples,
                                                 enc_ctx->sample_rate, dec_ctx->sample_rate, AV_ROUND_UP);
                converted->nb_samples = out_samples;
                converted->channel_layout = enc_ctx->channel_layout;
                converted->format = enc_ctx->sample_fmt;
                converted->sample_rate = enc_ctx->sample_rate;
                if (av_frame_get_buffer(converted, 0) < 0) {
                    res.error = "frame buffer failed";
                    av_frame_unref(decoded);
                    goto done;
                }
                int got = swr_convert(swr, converted->data, out_samples,
                                      const_cast<const uint8_t**>(decoded->data), decoded->nb_samples);
                if (got > 0) {
                    converted->nb_samples = got;
                    if (!encode_audio_frame(enc_ctx, out_fmt, out_stream, converted, res)) {
                        av_frame_unref(converted);
                        av_frame_unref(decoded);
                        goto done;
                    }
                }
                av_frame_unref(converted);
                av_frame_unref(decoded);
            }
        }
        av_packet_unref(pkt);
    }
    avcodec_send_packet(dec_ctx, nullptr);
    while (avcodec_receive_frame(dec_ctx, decoded) == 0) {
        int out_samples = av_rescale_rnd(swr_get_delay(swr, dec_ctx->sample_rate) + decoded->nb_samples,
                                         enc_ctx->sample_rate, dec_ctx->sample_rate, AV_ROUND_UP);
        converted->nb_samples = out_samples;
        converted->channel_layout = enc_ctx->channel_layout;
        converted->format = enc_ctx->sample_fmt;
        converted->sample_rate = enc_ctx->sample_rate;
        if (av_frame_get_buffer(converted, 0) < 0) {
            res.error = "frame buffer failed";
            av_frame_unref(decoded);
            goto done;
        }
        int got = swr_convert(swr, converted->data, out_samples,
                              const_cast<const uint8_t**>(decoded->data), decoded->nb_samples);
        if (got > 0) {
            converted->nb_samples = got;
            if (!encode_audio_frame(enc_ctx, out_fmt, out_stream, converted, res)) {
                av_frame_unref(converted);
                av_frame_unref(decoded);
                goto done;
            }
        }
        av_frame_unref(converted);
        av_frame_unref(decoded);
    }
    encode_audio_frame(enc_ctx, out_fmt, out_stream, nullptr, res);
    av_write_trailer(out_fmt);
    res.ok = true;

done:
    if (out_fmt && !(out_fmt->oformat->flags & AVFMT_NOFILE) && out_fmt->pb) avio_closep(&out_fmt->pb);
    if (out_fmt) avformat_free_context(out_fmt);
    if (swr) swr_free(&swr);
    if (dec_ctx) avcodec_free_context(&dec_ctx);
    if (enc_ctx) avcodec_free_context(&enc_ctx);
    if (in_fmt) avformat_close_input(&in_fmt);
    if (pkt) av_packet_free(&pkt);
    if (decoded) av_frame_free(&decoded);
    if (converted) av_frame_free(&converted);
    return res;
}

MediaToolResult media_extract_audio_wav(const std::string& input, const std::string& media_root) {
    ConversionOptions opts;
    opts.target = ConvertTarget::WavPcm;
    opts.sample_rate = 44100;
    opts.bit_depth = 16;
    return media_extract_audio_wav_opts(input, media_root, opts);
}

MediaToolResult media_convert(const std::string& input, const std::string& media_root,
                              const ConversionOptions& options) {
    switch (options.target) {
    case ConvertTarget::MkvCopy:
        return media_remux_copy(input, media_root, "matroska", ".mkv");
    case ConvertTarget::Mp4Copy:
        return media_remux_copy(input, media_root, "mp4", ".mp4");
    case ConvertTarget::WavPcm:
        return media_extract_audio_wav_opts(input, media_root, options);
    case ConvertTarget::OggVorbis:
        return media_encode_audio(input, media_root, options, AV_CODEC_ID_VORBIS, "ogg", ".ogg");
    case ConvertTarget::Mp3:
        return media_encode_audio(input, media_root, options, AV_CODEC_ID_MP3, "mp3", ".mp3");
    case ConvertTarget::Aac: {
        return media_encode_audio(input, media_root, options, AV_CODEC_ID_AAC, "adts", ".aac");
    }
    default: {
        MediaToolResult res;
        res.error = "unknown target";
        return res;
    }
    }
}
