#include "player.hpp"

#include <libgen.h>

#include <cctype>


namespace {

static std::string trim_metadata(std::string s) {
    size_t begin = 0;
    while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin]))) {
        ++begin;
    }
    size_t end = s.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(begin, end - begin);
}

static std::string clean_metadata_value(const char* value) {
    if (!value) {
        return std::string();
    }
    std::string out;
    bool last_space = false;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p; ++p) {
        unsigned char c = *p;
        bool make_space = c == '\r' || c == '\n' || c == '\t';
        if (make_space) {
            if (!last_space && !out.empty()) {
                out.push_back(' ');
                last_space = true;
            }
            continue;
        }
        if ((c < 0x20) || c == 0x7F) {
            continue;
        }
        out.push_back(static_cast<char>(c));
        last_space = c == ' ';
    }
    return trim_metadata(out);
}

static std::string metadata_lookup(const AVDictionary* dict, const char* const* keys, size_t key_count) {
    if (!dict) {
        return std::string();
    }
    for (size_t i = 0; i < key_count; ++i) {
        AVDictionaryEntry* entry = av_dict_get(dict, keys[i], nullptr, 0);
        if (entry) {
            std::string value = clean_metadata_value(entry->value);
            if (!value.empty()) {
                return value;
            }
        }
    }
    return std::string();
}

static std::string metadata_lookup_all(AVFormatContext* fmt, const char* const* keys, size_t key_count) {
    if (!fmt) {
        return std::string();
    }
    std::string value = metadata_lookup(fmt->metadata, keys, key_count);
    if (!value.empty()) {
        return value;
    }
    for (unsigned i = 0; i < fmt->nb_streams; ++i) {
        if (!fmt->streams || !fmt->streams[i]) {
            continue;
        }
        value = metadata_lookup(fmt->streams[i]->metadata, keys, key_count);
        if (!value.empty()) {
            return value;
        }
    }
    return std::string();
}

} // namespace

Player::Player(AudioQueue& audio) : audio_(audio) {
    pkt_ = av_packet_alloc();
    frame_ = av_frame_alloc();
}

Player::~Player() {
    close();
    if (pkt_) av_packet_free(&pkt_);
    if (frame_) av_frame_free(&frame_);
}

void Player::set_settings(const AppSettings& settings) {
    output_sample_rate_ = clamp_int(settings.audio_output_rate, 8000, 96000);
    seek_step_sec_ = clamp_int(settings.seek_step_sec, 1, 120);
    osd_ms_ = static_cast<uint32_t>(clamp_int(settings.osd_ms, 500, 30000));
    audio_ui_ms_ = static_cast<uint32_t>(clamp_int(settings.audio_ui_ms, 50, 2000));
    queue_high_water_bytes_ = static_cast<size_t>(clamp_int(settings.audio_buffer_kb / 2, 24, 128)) * 1024;
}

bool Player::open_codec(int stream_index, AVCodecContext*& ctx) {
    AVStream* st = fmt_->streams[stream_index];
    const AVCodec* dec = ffmpeg_find_decoder(st->codecpar->codec_id);
    if (!dec) {
        return false;
    }
    ctx = avcodec_alloc_context3(dec);
    if (!ctx) {
        return false;
    }
    if (avcodec_parameters_to_context(ctx, st->codecpar) < 0) {
        return false;
    }
    ctx->flags2 |= AV_CODEC_FLAG2_FAST;
    return avcodec_open2(ctx, dec, nullptr) == 0;
}

bool Player::open(const std::string& path) {
    close();
    path_ = path;
    if (avformat_open_input(&fmt_, path.c_str(), nullptr, nullptr) < 0) {
        std::fprintf(stderr, "cannot open media: %s\n", path.c_str());
        return false;
    }
    if (avformat_find_stream_info(fmt_, nullptr) < 0) {
        return false;
    }
    load_metadata();

    for (unsigned i = 0; i < fmt_->nb_streams; ++i) {
        AVMediaType t = fmt_->streams[i]->codecpar->codec_type;
        if (t == AVMEDIA_TYPE_AUDIO && audio_stream_ < 0) audio_stream_ = static_cast<int>(i);
        if (t == AVMEDIA_TYPE_VIDEO && video_stream_ < 0) video_stream_ = static_cast<int>(i);
    }
    if (audio_stream_ < 0 && video_stream_ < 0) {
        return false;
    }
    if (audio_stream_ >= 0 && !open_codec(audio_stream_, audio_ctx_)) {
        audio_stream_ = -1;
    }
    if (video_stream_ >= 0 && !open_codec(video_stream_, video_ctx_)) {
        video_stream_ = -1;
    }
    if (audio_stream_ < 0 && video_stream_ < 0) {
        return false;
    }

    if (audio_ctx_) {
        swr_ = swr_alloc_set_opts(nullptr, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, output_sample_rate_,
                                  ffmpeg_channel_layout(audio_ctx_), audio_ctx_->sample_fmt,
                                  audio_ctx_->sample_rate, 0, nullptr);
        if (!swr_ || swr_init(swr_) < 0) {
            return false;
        }
        audio_.set_format(output_sample_rate_, 2, 2);
    }
    duration_ = fmt_->duration > 0 ? static_cast<float>(fmt_->duration) / static_cast<float>(AV_TIME_BASE) : 0.0f;
    audio_.clear();
    audio_.set_clock_base(0.0f);
    paused_ = false;
    eof_ = false;
    osd_until_ = platform_ticks() + osd_ms_;
    last_osd_refresh_ = 0;
    last_audio_ui_refresh_ = 0;
    overlays_on_screen_ = false;
    wall_base_ = now_seconds();
    pause_started_ = 0.0f;
    renderer_.reset();
    return true;
}

void Player::close() {
    audio_.clear();
    if (swr_) swr_free(&swr_);
    if (audio_ctx_) avcodec_free_context(&audio_ctx_);
    if (video_ctx_) avcodec_free_context(&video_ctx_);
    if (fmt_) avformat_close_input(&fmt_);
    audio_stream_ = video_stream_ = -1;
    duration_ = video_clock_ = 0.0f;
    paused_ = false;
    eof_ = false;
    menu_open_ = false;
    osd_until_ = 0;
    last_osd_refresh_ = 0;
    last_audio_ui_refresh_ = 0;
    overlays_on_screen_ = false;
    wall_base_ = 0.0f;
    pause_started_ = 0.0f;
    path_.clear();
    metadata_line_.clear();
    renderer_.reset();
}


void Player::load_metadata() {
    metadata_line_.clear();
    if (!fmt_) {
        return;
    }

    static const char* const author_keys[] = {
        "artist", "album_artist", "album artist", "author", "composer", "performer", "author-sort"
    };
    static const char* const title_keys[] = {
        "title", "name", "tracktitle", "song"
    };

    std::string author = metadata_lookup_all(fmt_, author_keys, sizeof(author_keys) / sizeof(author_keys[0]));
    std::string name = metadata_lookup_all(fmt_, title_keys, sizeof(title_keys) / sizeof(title_keys[0]));

    if (!author.empty() && !name.empty()) {
        metadata_line_ = author + " - " + name;
    } else if (!name.empty()) {
        metadata_line_ = name;
    } else if (!author.empty()) {
        metadata_line_ = author;
    }
}

float Player::frame_pts(AVFrame* frame, int stream_index) const {
    int64_t ts = frame->best_effort_timestamp;
    if (ts == AV_NOPTS_VALUE) {
        ts = frame->pts;
    }
    if (ts == AV_NOPTS_VALUE || stream_index < 0) {
        return 0.0f;
    }
    AVRational tb = fmt_->streams[stream_index]->time_base;
    if (tb.den == 0) {
        return 0.0f;
    }
    return static_cast<float>(ts) * static_cast<float>(tb.num) / static_cast<float>(tb.den);
}

bool Player::decode_audio_frame(AVFrame* frame) {
    if (!swr_) {
        return true;
    }
    int out_samples = av_rescale_rnd(swr_get_delay(swr_, audio_ctx_->sample_rate) + frame->nb_samples,
                                     output_sample_rate_, audio_ctx_->sample_rate, AV_ROUND_UP);
    int out_bytes = out_samples * 2 * 2;
    audio_tmp_.resize(static_cast<size_t>(out_bytes));
    uint8_t* out[] = { audio_tmp_.data(), nullptr };
    int got = swr_convert(swr_, out, out_samples, const_cast<const uint8_t**>(frame->data), frame->nb_samples);
    if (got <= 0) {
        return true;
    }
    size_t bytes = static_cast<size_t>(got * 2 * 2);
    if (audio_.size() < 4096) {
        audio_.set_clock_base(frame_pts(frame, audio_stream_));
    }
    return audio_.push(audio_tmp_.data(), bytes);
}

bool Player::should_render_video(float vpts) const {
    if (!audio_ctx_) {
        return vpts <= (now_seconds() - wall_base_) + 0.04f;
    }
    float a = audio_.clock();
    if (vpts < a - 0.08f) {
        return false;
    }
    return vpts <= a + 0.04f || audio_.size() < 2048;
}

bool Player::decode_video_frame(Platform& platform, AVFrame* frame) {
    float pts = frame_pts(frame, video_stream_);
    video_clock_ = pts;
    if (audio_ctx_) {
        float diff = pts - audio_.clock();
        if (diff < -0.08f) {
            return true;
        }
        if (diff > 0.04f) {
            platform_sleep_ms(static_cast<unsigned>(std::min(20.0f, diff * 1000.0f)));
        }
    } else {
        float diff = pts - (now_seconds() - wall_base_);
        if (diff < -0.08f) {
            return true;
        }
        if (diff > 0.04f) {
            platform_sleep_ms(static_cast<unsigned>(std::min(20.0f, diff * 1000.0f)));
        }
    }
    if (renderer_.render(platform.screen, frame, video_ctx_)) {
        overlays_on_screen_ = draw_overlays(platform, true);
        platform_flip(platform);
    }
    return true;
}

bool Player::decode_some(Platform& platform) {
    if (eof_) {
        return false;
    }
    int loops = 0;
    while (loops++ < 24) {
        if (audio_ctx_ && audio_.size() > queue_high_water_bytes_) {
            return true;
        }
        int r = av_read_frame(fmt_, pkt_);
        if (r < 0) {
            eof_ = true;
            return false;
        }
        AVCodecContext* ctx = nullptr;
        int si = pkt_->stream_index;
        if (si == audio_stream_) ctx = audio_ctx_;
        if (si == video_stream_) ctx = video_ctx_;
        if (!ctx) {
            av_packet_unref(pkt_);
            continue;
        }
        if (avcodec_send_packet(ctx, pkt_) == 0) {
            while (avcodec_receive_frame(ctx, frame_) == 0) {
                if (si == audio_stream_) {
                    if (!decode_audio_frame(frame_)) {
                        av_frame_unref(frame_);
                        av_packet_unref(pkt_);
                        return true;
                    }
                } else if (si == video_stream_) {
                    decode_video_frame(platform, frame_);
                }
                av_frame_unref(frame_);
            }
        }
        av_packet_unref(pkt_);
    }
    return true;
}

void Player::seek_to(float sec) {
    if (!fmt_) return;
    sec = std::max(0.0f, duration_ > 0.1f ? std::min(sec, duration_ - 0.1f) : sec);
    int64_t ts = static_cast<int64_t>(sec * AV_TIME_BASE);
    if (av_seek_frame(fmt_, -1, ts, AVSEEK_FLAG_BACKWARD) >= 0) {
        if (audio_ctx_) ffmpeg_flush_codec(audio_ctx_);
        if (video_ctx_) ffmpeg_flush_codec(video_ctx_);
        if (swr_) swr_init(swr_);
        audio_.clear();
        audio_.set_clock_base(sec);
        video_clock_ = sec;
        wall_base_ = now_seconds() - sec;
        eof_ = false;
    }
}

void Player::seek_relative(float delta) {
    float pos = audio_ctx_ ? audio_.clock() : video_clock_;
    seek_to(pos + delta);
}

void Player::handle(Action action) {
    switch (action) {
    case Action::Menu:
        menu_open_ = !menu_open_;
        show_osd(menu_open_ ? 60000 : osd_ms_);
        break;
    case Action::CycleOrder:
        cycle_order();
        show_osd(osd_ms_);
        break;
    case Action::Start:
    case Action::Pause:
    case Action::Confirm:
        if (!paused_) {
            pause_started_ = now_seconds();
            paused_ = true;
        } else {
            if (pause_started_ > 0.0f) {
                wall_base_ += now_seconds() - pause_started_;
            }
            pause_started_ = 0.0f;
            paused_ = false;
        }
        audio_.set_paused(paused_);
        show_osd(paused_ ? 60000 : osd_ms_);
        break;
    case Action::SeekBack:
    case Action::Left:
        seek_relative(-static_cast<float>(seek_step_sec_));
        show_osd(osd_ms_);
        break;
    case Action::SeekForward:
    case Action::Right:
        seek_relative(static_cast<float>(seek_step_sec_));
        show_osd(osd_ms_);
        break;
    case Action::VolDown:
    case Action::Down:
        audio_.set_volume(audio_.volume() - 8);
        show_osd(osd_ms_);
        break;
    case Action::VolUp:
    case Action::Up:
        audio_.set_volume(audio_.volume() + 8);
        show_osd(osd_ms_);
        break;
    default:
        break;
    }
}

void Player::set_order(PlaybackOrder order) {
    order_ = order;
}

void Player::cycle_order() {
    switch (order_) {
    case PlaybackOrder::Sequential: order_ = PlaybackOrder::ListLoop; break;
    case PlaybackOrder::ListLoop: order_ = PlaybackOrder::RepeatOne; break;
    case PlaybackOrder::RepeatOne: order_ = PlaybackOrder::Shuffle; break;
    case PlaybackOrder::Shuffle: order_ = PlaybackOrder::Sequential; break;
    }
}

void Player::show_osd(uint32_t ms) {
    osd_until_ = platform_ticks() + ms;
}

bool Player::should_show_osd() const {
    return menu_open_ || paused_ || !video_ctx_ || platform_ticks() < osd_until_;
}

bool Player::draw_overlays(Platform& platform, bool force) {
    const uint32_t now = platform_ticks();
    const bool osd_visible = should_show_osd();
    const bool any_overlay_visible = osd_visible || menu_open_;
    if (!any_overlay_visible) {
        return false;
    }
    if (!force && last_osd_refresh_ != 0 && now - last_osd_refresh_ < audio_ui_ms_) {
        return false;
    }

    PlaybackInfo pi = info();
    pi.osd_visible = osd_visible;
    if (pi.osd_visible) {
        ui_draw_playback_osd(platform.screen, pi);
    }
    if (menu_open_) {
        ui_draw_playback_menu(platform.screen, pi);
    }
    last_osd_refresh_ = now;
    return true;
}

bool Player::present_cached_video(Platform& platform, bool force_overlay) {
    if (!video_ctx_ || !renderer_.redraw(platform.screen)) {
        return false;
    }
    overlays_on_screen_ = draw_overlays(platform, force_overlay);
    platform_flip(platform);
    return true;
}

PlaybackInfo Player::info() const {
    PlaybackInfo pi;
    size_t slash = path_.find_last_of('/');
    pi.title = slash == std::string::npos ? path_ : path_.substr(slash + 1);
    pi.metadata_line = metadata_line_;
    pi.pos = audio_ctx_ ? audio_.clock() : video_clock_;
    pi.duration = duration_;
    pi.paused = paused_;
    pi.volume = audio_.volume();
    pi.has_video = video_ctx_ != nullptr;
    pi.osd_visible = should_show_osd();
    pi.order = order_;
    pi.audio_queue_kb = static_cast<int>(audio_.size() / 1024);
    if (video_ctx_) {
        pi.video_w = video_ctx_->width;
        pi.video_h = video_ctx_->height;
    }
    return pi;
}

PlayerResult Player::tick(Platform& platform) {
    Action a = poll_action();
    if (a == Action::Quit) return PlayerResult::Quit;
    if (a == Action::Back) return PlayerResult::Back;
    if (a == Action::Up) return PlayerResult::Previous;
    if (a == Action::Down) return PlayerResult::Next;
    if (a != Action::None) handle(a);

    if (!paused_) {
        decode_some(platform);
    }

    const uint32_t now = platform_ticks();
    const bool action_redraw = a != Action::None;
    if (!video_ctx_ && (now - last_audio_ui_refresh_ >= audio_ui_ms_ || action_redraw || paused_ || menu_open_)) {
        ui_clear(platform.screen, 7, 9, 12);
        ui_draw_text(platform.screen, 36, 86, "AUDIO PLAYER", 220, 220, 220, 2);
        overlays_on_screen_ = draw_overlays(platform, true);
        platform_flip(platform);
        last_audio_ui_refresh_ = now;
    } else if (video_ctx_ && (paused_ || menu_open_)) {
        present_cached_video(platform, true);
    } else if (video_ctx_ && should_show_osd() && (action_redraw || now - last_osd_refresh_ >= audio_ui_ms_)) {
        present_cached_video(platform, true);
    } else if (video_ctx_ && overlays_on_screen_ && !should_show_osd()) {
        present_cached_video(platform, false);
    }

    if (eof_ && audio_.size() == 0) {
        return PlayerResult::Finished;
    }
    platform_sleep_ms(video_ctx_ ? 4 : 8);
    return PlayerResult::Playing;
}
