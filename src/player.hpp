#pragma once

#include "audio_queue.hpp"
#include "input.hpp"
#include "settings.hpp"
#include "ui.hpp"
#include "video_renderer.hpp"

enum class PlayerResult {
    Playing,
    Finished,
    Previous,
    Next,
    Back,
    Quit
};

class Player {
public:
    explicit Player(AudioQueue& audio);
    ~Player();

    bool open(const std::string& path);
    void close();
    PlayerResult tick(Platform& platform);
    void handle(Action action);
    void set_order(PlaybackOrder order);
    void set_settings(const AppSettings& settings);
    PlaybackOrder order() const { return order_; }
    PlaybackInfo info() const;

private:
    bool open_codec(int stream_index, AVCodecContext*& ctx);
    void load_metadata();
    bool decode_some(Platform& platform);
    bool decode_audio_frame(AVFrame* frame);
    bool decode_video_frame(Platform& platform, AVFrame* frame);
    void seek_relative(float delta);
    void seek_to(float sec);
    float frame_pts(AVFrame* frame, int stream_index) const;
    bool should_render_video(float vpts) const;
    void show_osd(uint32_t ms = 3000);
    bool should_show_osd() const;
    bool draw_overlays(Platform& platform, bool force);
    bool present_cached_video(Platform& platform, bool force_overlay);
    void cycle_order();

    AudioQueue& audio_;
    std::string path_;
    std::string metadata_line_;
    AVFormatContext* fmt_ = nullptr;
    AVCodecContext* audio_ctx_ = nullptr;
    AVCodecContext* video_ctx_ = nullptr;
    SwrContext* swr_ = nullptr;
    AVPacket* pkt_ = nullptr;
    AVFrame* frame_ = nullptr;
    int audio_stream_ = -1;
    int video_stream_ = -1;
    float duration_ = 0.0f;
    float video_clock_ = 0.0f;
    bool paused_ = false;
    bool eof_ = false;
    bool menu_open_ = false;
    PlaybackOrder order_ = PlaybackOrder::Sequential;
    int output_sample_rate_ = 44100;
    int seek_step_sec_ = 10;
    uint32_t osd_ms_ = 3000;
    uint32_t audio_ui_ms_ = 250;
    size_t queue_high_water_bytes_ = 48 * 1024;
    uint32_t osd_until_ = 0;
    uint32_t last_osd_refresh_ = 0;
    uint32_t last_audio_ui_refresh_ = 0;
    bool overlays_on_screen_ = false;
    float wall_base_ = 0.0f;
    float pause_started_ = 0.0f;
    VideoRenderer renderer_;
    std::vector<uint8_t> audio_tmp_;
};
