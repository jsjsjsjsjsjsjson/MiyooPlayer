#pragma once

#include "common.hpp"

struct MediaToolResult {
    bool ok = false;
    std::string output;
    std::string error;
};

enum class ConvertTarget {
    MkvCopy,
    Mp4Copy,
    WavPcm,
    OggVorbis,
    Mp3,
    Aac
};

struct ConversionOptions {
    ConvertTarget target = ConvertTarget::MkvCopy;
    int sample_rate = 44100;
    int bit_depth = 16;
    int bitrate_kbps = 128;
};

const char* convert_target_name(ConvertTarget target);
ConvertTarget convert_next_target(ConvertTarget target);
int convert_next_sample_rate(int current);
int convert_next_bit_depth(int current);
int convert_next_bitrate(int current);

MediaToolResult media_convert(const std::string& input, const std::string& media_root,
                              const ConversionOptions& options);
MediaToolResult media_remux_to_mkv(const std::string& input, const std::string& media_root);
MediaToolResult media_extract_audio_wav(const std::string& input, const std::string& media_root);
